#include "v4l2.h"

#include <sys/ioctl.h>
#include <sys/stat.h>

#include <malloc.h>
#include <fcntl.h>
#include <errno.h>


/// This macro can be used to retry a failed syscall that may have returned 
/// a 'EINTR' error code.
#define RETRY_INT(expr) ({ int __res; do { __res = (expr); } while (__res == -1 && errno == EINTR); __res; })


///
/// MISC FUNCTION FOR VIDEO DEVICE
///

enum vid_result vid_open(int *fd, const char *path) {

    struct stat st;
    if (stat(path, &st) == -1)
        return VID_ERR_SYS;

    if (!S_ISCHR(st.st_mode))
        return VID_ERR_NO_VIDEO;

    int new_fd = open(path, O_RDWR | O_NONBLOCK, 0);
    if (new_fd == -1)
        return VID_ERR_SYS;

    struct v4l2_capability cap;
    if (RETRY_INT(ioctl(new_fd, VIDIOC_QUERYCAP, &cap)) == -1)
        return VID_ERR_NO_VIDEO;
    
    if (!(cap.capabilities & V4L2_CAP_STREAMING))
        return VID_ERR_NO_STREAMING;

    *fd = new_fd;
    return VID_OK;

}

enum vid_result vid_query_capability(int fd, struct v4l2_capability *dst) {
    if (RETRY_INT(ioctl(fd, VIDIOC_QUERYCAP, dst)) == -1)
        return VID_ERR_SYS;
    return VID_OK;
}

enum vid_result vid_stream_on(int fd, enum v4l2_buf_type type) {
    if (RETRY_INT(ioctl(fd, VIDIOC_STREAMON, &type)) == -1)
        return VID_ERR_SYS;
    return VID_OK;
}

enum vid_result vid_stream_off(int fd, enum v4l2_buf_type type) {
    if (RETRY_INT(ioctl(fd, VIDIOC_STREAMOFF, &type)) == -1)
        return VID_ERR_SYS;
    return VID_OK;
}

///
/// VIDEO CROPPING
///

enum vid_result vid_query_crop_capability(int fd, struct v4l2_cropcap *dst) {
    if (RETRY_INT(ioctl(fd, VIDIOC_CROPCAP, dst)) == -1)
        return VID_ERR_SYS;
    return VID_OK;
}

enum vid_result vid_get_crop(int fd, enum v4l2_buf_type type, struct v4l2_rect *dst) {
    
    struct v4l2_crop crop = {0};
    crop.type = type;
    if (RETRY_INT(ioctl(fd, VIDIOC_G_CROP, &crop)) == -1)
        return VID_ERR_SYS;

    *dst = crop.c;
    return VID_OK;

}

enum vid_result vid_set_crop(int fd, enum v4l2_buf_type type, struct v4l2_rect src) {

    struct v4l2_crop crop = {0};
    crop.type = type;
    crop.c = src;
    if (RETRY_INT(ioctl(fd, VIDIOC_S_CROP, &crop)) == -1)
        return VID_ERR_SYS;

    return VID_OK;

}

///
/// VIDEO FORMAT
///

enum vid_result vid_enum_format(int fd, struct v4l2_fmtdesc *dst) {
    if (RETRY_INT(ioctl(fd, VIDIOC_ENUM_FMT, dst)) == -1)
        return VID_ERR_STOP;
    return VID_OK;
}

enum vid_result vid_get_format(int fd, struct v4l2_format *dst) {
    if (RETRY_INT(ioctl(fd, VIDIOC_G_FMT, dst)) == -1)
        return VID_ERR_SYS;
    return VID_OK;
}

enum vid_result vid_set_format(int fd, struct v4l2_format *src) {
    if (RETRY_INT(ioctl(fd, VIDIOC_S_FMT, src)) == -1)
        return VID_ERR_SYS;
    return VID_OK;
}

enum vid_result vid_set_format_checked(int fd, enum v4l2_buf_type type, unsigned width, unsigned height, unsigned pixelformat) {
    
    struct v4l2_format fmt = {0};
    fmt.type = type;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = pixelformat;

    enum vid_result res = vid_set_format(fd, &fmt);
    if (res != VID_OK)
        return res;
    
    if (fmt.fmt.pix.width != width || fmt.fmt.pix.height != height || fmt.fmt.pix.pixelformat != pixelformat)
        return VID_ERR_NEGOCIATION;

    return VID_OK;

}

enum vid_result vid_set_format_mp_checked(int fd, enum v4l2_buf_type type, unsigned width, unsigned height, unsigned pixelformat, unsigned planes) {
    
    struct v4l2_format fmt = {0};
    fmt.type = type;
    fmt.fmt.pix_mp.width = width;
    fmt.fmt.pix_mp.height = height;
    fmt.fmt.pix_mp.pixelformat = pixelformat;
    fmt.fmt.pix_mp.num_planes = planes;

    enum vid_result res = vid_set_format(fd, &fmt);
    if (res != VID_OK)
        return res;
    
    if (fmt.fmt.pix.width != width || fmt.fmt.pix.height != height || fmt.fmt.pix.pixelformat != pixelformat)
        return VID_ERR_NEGOCIATION;

    return VID_OK;

}

///
/// VIDEO BUFFERS 
///

enum vid_result vid_request_buffers(int fd, struct v4l2_requestbuffers *req) {
    if (RETRY_INT(ioctl(fd, VIDIOC_REQBUFS, req)) == -1)
        return VID_ERR_SYS;
    return VID_OK;
}

enum vid_result vid_export_buffer(int fd, struct v4l2_exportbuffer *exp) {
    if (RETRY_INT(ioctl(fd, VIDIOC_EXPBUF, exp)) == -1)
        return VID_ERR_SYS;
    return VID_OK;
}

enum vid_result vid_query_buffer(int fd, struct v4l2_buffer *buf) {
    if (RETRY_INT(ioctl(fd, VIDIOC_QUERYBUF, buf)) == -1)
        return VID_ERR_SYS;
    return VID_OK;
}

enum vid_result vid_queue_buffer(int fd, struct v4l2_buffer *buf) {
    if (RETRY_INT(ioctl(fd, VIDIOC_QBUF, buf)) == -1)
        return VID_ERR_SYS;
    if (buf->flags & V4L2_BUF_FLAG_ERROR)
        return VID_ERR_NEGOCIATION;
    return VID_OK;
}

enum vid_result vid_unqueue_buffer(int fd, struct v4l2_buffer *buf) {
    if (RETRY_INT(ioctl(fd, VIDIOC_DQBUF, buf)) == -1) {
        if (errno == EAGAIN) {
            return VID_ERR_RETRY;
        } else {
            return VID_ERR_SYS;
        }
    }
    if (buf->flags & V4L2_BUF_FLAG_ERROR)
        return VID_ERR_NEGOCIATION;
    return VID_OK;
}
