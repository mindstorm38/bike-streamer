/// This program is the client, or embedded, part of bike streamer, it configures the 
/// video device to output H264 compressed video stream using the v4l2 userspace API.

#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <linux/videodev2.h>

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>


/// Different kind of results.
enum result_kind {
    OK,
    ERR_ERRNO,
    ERR_OOM,
    ERR_TIMEOUT,
    ERR_OUT_OF_RANGE,
    ERR_DEVICE_STAT,
    ERR_DEVICE_NOT_CHR,
    ERR_DEVICE_OPEN,
    ERR_V4L2,
    ERR_V4L2_CAPTURE,
    ERR_V4L2_STREAMING,
    ERR_V4L2_FORMAT,
    ERR_V4L2_MMAP,
    ERR_V4L2_BUFFER_COUNT,
    ERR_V4L2_QBUF,
    ERR_V4L2_DQBUF,
    ERR_V4L2_STREAMON,
    ERR_V4L2_STREAMOFF,
};

/// A result structure.
struct result {
    /// True when the returned value is an error code.
    enum result_kind kind;
    /// Depending on the kind of result, the internal value can be interpreted.
    union {
        /// The result value as a integer number.
        int num;
        /// The result value as a pointer.
        void *ptr;
    };
};

#define RESULT(kind_) ((struct result) { .kind = (kind_) })
#define RESULT_NUM(kind_, num_) ((struct result) { .kind = (kind_), .num = (num_) })
#define RESULT_PTR(kind_, ptr_) ((struct result) { .kind = (kind_), .ptr = (ptr_) })

/// This macro can be used to retry a failed syscall that may have returned 
/// a 'EINTR' error code.
#define RETRY_INT(expr) ({ int __res; do { __res = (expr); } while (__res == -1 && errno == EINTR); __res; })

/// Internal structure to track a memory mapped video capture buffer.
struct video_buffer {
    void *start;
    size_t length;
};


/// Open a video device, checking that is's a right device before.
static struct result open_device(const char *path) {

    struct stat st;
    if (stat(path, &st) == -1)
        return RESULT_NUM(ERR_DEVICE_STAT, errno);

    if (!S_ISCHR(st.st_mode))
        return RESULT(ERR_DEVICE_NOT_CHR);

    int fd = open(path, O_RDWR | O_NONBLOCK, 0);
    if (fd == -1)
        return RESULT_NUM(ERR_DEVICE_OPEN, errno);

    return RESULT_NUM(OK, fd);

}

/// Initialize the format and buffers for a video capturing device. If result is OK, the
/// value is a pointer to the buffers structure, containing the requested number of 
/// buffers.
static struct result init_device(int fd, unsigned buffers_count) {

    // Query and check requird capabilities.
    struct v4l2_capability cap = {0};
    if (RETRY_INT(ioctl(fd, VIDIOC_QUERYCAP, &cap)) == -1) {
        if (errno == EINVAL) {
            return RESULT(ERR_V4L2);
        } else {
            return RESULT_NUM(ERR_ERRNO, errno);
        }
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
        return RESULT(ERR_V4L2_CAPTURE);

    if (!(cap.capabilities & V4L2_CAP_STREAMING))
        return RESULT(ERR_V4L2_STREAMING);

    // List supported formats for debug purpose.
    printf("info: supported formats:\n");
    struct v4l2_fmtdesc fmtdesc = {0};
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    while (RETRY_INT(ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc)) == 0) {    
        printf("- %s\n", fmtdesc.description);
        fmtdesc.index++;
    }

    // struct v4l2_cropcap cropcap = {0};
    // cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    // if (RETRY_INT(ioctl(fd, VIDIOC_CROPCAP, &cropcap)) == 0) {

    //     struct v4l2_crop crop = { 
    //         .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
    //         .c = cropcap.defrect,
    //     };

    //     printf("info: crop %d/%d (%d/%d)\n", crop.c.left, crop.c.top, crop.c.width, crop.c.height);

    //     if (RETRY_INT(ioctl(fd, VIDIOC_S_CROP, &crop)) == -1) {
    //         // Errors ignored if we cannot set crop.
    //     }

    // } else if (errno == ENODATA) {
    //     printf("warn: cropping not supported\n");
    // } else if (errno == EINVAL) {
    //     printf("warn: invalid cropcap structure\n");
    // }

    // Set format to H264.
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (RETRY_INT(ioctl(fd, VIDIOC_G_FMT, &fmt)) == -1)
        return RESULT_NUM(ERR_V4L2_FORMAT, errno);

    // fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    // fmt.fmt.pix.width = 4056;
    // fmt.fmt.pix.height = 3040;
    // fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
    // fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    // if (RETRY_INT(ioctl(fd, VIDIOC_S_FMT, &fmt)) == -1)
    //     return RESULT_NUM(ERR_V4L2_FORMAT, errno);

    printf("info: format image size: %d\n", fmt.fmt.pix.sizeimage);

    // Request 4 rotating userspace buffers.
    struct v4l2_requestbuffers req = {0};
    req.count = buffers_count;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (RETRY_INT(ioctl(fd, VIDIOC_REQBUFS, &req)) == -1) {
        if (errno == EINVAL) {
            return RESULT(ERR_V4L2_MMAP);
        } else {
            return RESULT_NUM(ERR_ERRNO, errno);
        }
    }

    printf("info: buffer count: %d\n", req.count);
    if (req.count != buffers_count)
        return RESULT(ERR_V4L2_BUFFER_COUNT);

    /// Allocating our tracking buffers to register the memory mapped regions.
    struct video_buffer *buffers = calloc(buffers_count, sizeof(struct video_buffer));
    if (!buffers)
        return RESULT(ERR_OOM);

    for (unsigned buffer_index = 0; buffer_index < buffers_count; buffer_index++) {

        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = buffer_index;

        if (RETRY_INT(ioctl(fd, VIDIOC_QUERYBUF, &buf)) == -1)
            return RESULT_NUM(ERR_ERRNO, errno);
        
        // If the memory mapping fail, we also free the allocated buffers and all already
        // allocated ones.
        void *start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (!start) {
            for (unsigned alloc_index = 0; alloc_index < buffer_index; alloc_index++) {
                munmap(buffers[alloc_index].start, buffers[alloc_index].length);
            }
            free(buffers);
            return RESULT_NUM(ERR_ERRNO, errno);
        }

        buffers[buffer_index].start = start;
        buffers[buffer_index].length = buf.length;
        printf("info: buffer %d allocated at %p for %d\n", buffer_index, start, buf.length);

    }

    return RESULT_PTR(OK, buffers);

}

/// Start the capturing device.
static struct result start_device(int fd, unsigned buffers_count, struct video_buffer *buffers) {

    for (unsigned buffer_index = 0; buffer_index < buffers_count; buffer_index++) {

        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = buffer_index;

        if (RETRY_INT(ioctl(fd, VIDIOC_QBUF, &buf)) == -1)
            return RESULT_NUM(ERR_V4L2_QBUF, errno);

    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (RETRY_INT(ioctl(fd, VIDIOC_STREAMON, &type)) == -1)
        return RESULT_NUM(ERR_V4L2_STREAMON, errno);

    return RESULT(OK);

}

/// Stop the capturing device.
static struct result stop_device(int fd) {
    
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (RETRY_INT(ioctl(fd, VIDIOC_STREAMOFF, &type)) == -1)
        return RESULT_NUM(ERR_V4L2_STREAMOFF, errno);

    return RESULT(OK);

}

/// Process a buffer that has been read.
static struct result process_buffer(void *start, size_t length, struct timeval time) {
    printf("process buffer at %p (%zu) at %ld\n", start, length, time.tv_sec);
    return RESULT(OK);
}

/// Try to read a frame from the device, if successful the buffer read is passed to 
/// the 'process_buffer' function below.
static struct result read_device(int fd, unsigned buffers_count, struct video_buffer *buffers) {

    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    
    if (RETRY_INT(ioctl(fd, VIDIOC_DQBUF, &buf)) == -1) {
        if (errno == EAGAIN) {
            return RESULT(OK);
        } else {
            return RESULT_NUM(ERR_V4L2_DQBUF, errno);
        }
    }

    if (buf.index >= buffers_count)
        return RESULT(ERR_OUT_OF_RANGE);

    struct result res = process_buffer(buffers[buf.index].start, buf.bytesused, buf.timestamp);
    if (res.kind != OK)
        return res;

    if (RETRY_INT(ioctl(fd, VIDIOC_QBUF, &buf)) == -1)
        return RESULT_NUM(ERR_V4L2_QBUF, errno);

    return RESULT(OK);

}

/// Program entry, wrapper inside this function in order to write all error messages
/// in main and avoid dealing with message in this function.
static struct result main_wrapper() {
    
    struct result res;
    
    printf("info: open device...\n");
    res = open_device("/dev/video0");
    int fd = res.num;
    if (res.kind != OK) {
        return res;
    }

    unsigned buffers_count = 4;

    printf("info: init device...\n");
    res = init_device(fd, buffers_count);
    struct video_buffer *buffers = res.ptr;
    if (res.kind != OK) {
        close(fd);
        return res;
    }

    printf("info: start device...\n");
    res = start_device(fd, buffers_count, buffers);
    if (res.kind != OK)
        return res;

    // Timeout duration when selecting.
    struct timeval timeout = {0};

    // The file descriptor set used for selecting.
    fd_set fds;
    FD_ZERO(&fds);

    for (;;) {

        FD_SET(fd, &fds);
        timeout.tv_sec = 2;
        int ret = RETRY_INT(select(fd + 1, &fds, NULL, NULL, &timeout));
        
        if (ret == -1) {
            return RESULT_NUM(ERR_ERRNO, errno);
        } else if (ret == 0) {
            // The select timedout, which is weird because the camera should send frames.
            return RESULT(ERR_TIMEOUT);
        }

        res = read_device(fd, buffers_count, buffers);
        if (res.kind != OK)
            return res;

    }

    printf("info: stop device...\n");
    res = stop_device(fd);
    if (res.kind != OK)
        return res;

    return RESULT_NUM(OK, 0);

}


int main() {

    struct result res = main_wrapper();

    switch (res.kind) {
    case OK:
        break;
    case ERR_ERRNO:
        printf("error: unspecified error (%d: %s)\n", res.num, strerror(res.num));
        break;
    case ERR_OOM:
        printf("error: out of memory\n");
        break;
    case ERR_TIMEOUT:
        printf("error: timed out\n");
        break;
    case ERR_OUT_OF_RANGE:
        printf("error: out of range\n");
        break;
    case ERR_DEVICE_STAT:
        printf("error: failed to stat device (%d: %s)\n", res.num, strerror(res.num));
        break;
    case ERR_DEVICE_NOT_CHR:
        printf("error: device file is not a char\n");
        break;
    case ERR_DEVICE_OPEN:
        printf("error: failed to open device (%d: %s)\n", res.num, strerror(res.num));
        break;
    case ERR_V4L2:
        printf("error: device do not support v4l2 api\n");
        break;
    case ERR_V4L2_CAPTURE:
        printf("error: device do not support video capture\n");
        break;
    case ERR_V4L2_STREAMING:
        printf("error: device do not support video streaming\n");
        break;
    case ERR_V4L2_FORMAT:
        printf("error: device do not support requested video format\n");
        break;
    case ERR_V4L2_MMAP:
        printf("error: device do not support video mmap mode\n");
        break;
    case ERR_V4L2_BUFFER_COUNT:
        printf("error: device do not support requested buffer count\n");
        break;
    case ERR_V4L2_QBUF:
        printf("error: failed to enqueue buffer (%d: %s)\n", res.num, strerror(res.num));
        break;
    case ERR_V4L2_DQBUF:
        printf("error: failed to dequeue buffer (%d: %s)\n", res.num, strerror(res.num));
        break;
    case ERR_V4L2_STREAMON:
        printf("error: failed to turn stream on (%d: %s)\n", res.num, strerror(res.num));
        break;
    case ERR_V4L2_STREAMOFF:
        printf("error: failed to turn stream off (%d: %s)\n", res.num, strerror(res.num));
        break;
    }

}
