#include "v4l2.h"

#include <sys/select.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/poll.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>


/// Internal structure to keep track of sensor memory mapped buffers.
struct encoder_buffer {
    void *start;
    unsigned length;
};


static void check_res(enum vid_result res) {
    switch (res) {
    case VID_OK:
        return;
    case VID_ERR_STOP:
        fprintf(stderr, "error: unhandled stop enumeration\n");
        break;
    case VID_ERR_RETRY:
        fprintf(stderr, "error: unhandled retry\n");
        break;
    case VID_ERR_SYS:
        fprintf(stderr, "error: system error (%s)\n", strerror(errno));
        break;
    case VID_ERR_NO_VIDEO:
        fprintf(stderr, "error: device do not support video\n");
        break;
    case VID_ERR_NO_STREAMING:
        fprintf(stderr, "error: device do not support streaming\n");
        break;
    case VID_ERR_NEGOCIATION:
        fprintf(stderr, "error: failed to negociate\n");
        break;
    }
    exit(1);
}

static bool check_ok_or_retry(enum vid_result res) {
    if (res == VID_OK) {
        return true;
    } else if (res == VID_ERR_RETRY) {
        return false;
    } else {
        check_res(res);
        exit(1);  // Ensure that the result is not undefined.
    }
}

static void check_cap(struct v4l2_capability *cap, unsigned flag, const char *err) {
    if (!(cap->capabilities & flag)) {
        fprintf(stderr, "error: %s\n", err);
        exit(1);
    }
}

static void print_formats(int fd, enum v4l2_buf_type type) {
    struct v4l2_fmtdesc fmtdesc = {0};
    fmtdesc.type = type;
    while (vid_enum_format(fd, &fmtdesc) == VID_OK) {
        printf("      - %s (%.4s)\n", fmtdesc.description, (const char *) &fmtdesc.pixelformat);
        fmtdesc.index++;
    }
}


#define BUFFERS_COUNT 4


int main() {

    int sensor_fd;
    int adapter_fd;
    int encoder_fd;

    struct v4l2_capability cap = {0};
    struct v4l2_cropcap cropcap = {0};
    struct v4l2_rect rect = {0};
    
    printf("info: opening video devices...\n");
    check_res(vid_open(&sensor_fd, "/dev/video0"));    // IMX477
    check_res(vid_open(&adapter_fd, "/dev/video12"));  // BCM2835
    check_res(vid_open(&encoder_fd, "/dev/video11"));  // BCM2835

    printf("info: checking capabilities...\n");
    check_res(vid_query_capability(sensor_fd, &cap));
    check_cap(&cap, V4L2_CAP_VIDEO_CAPTURE, "sensor device must support video 'capture'");
    check_res(vid_query_capability(adapter_fd, &cap));
    check_cap(&cap, V4L2_CAP_VIDEO_M2M_MPLANE, "converter device must support video 'mplane m2m'");
    check_res(vid_query_capability(encoder_fd, &cap));
    check_cap(&cap, V4L2_CAP_VIDEO_M2M_MPLANE, "encoder device must support video 'mplane m2m'");

    // print_formats(adapter_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);

    // cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    // check_res(vid_query_crop_capability(adapter_fd, &cropcap));
    // printf("cropcap: %d/%d %dx%d\n", cropcap.defrect.left, cropcap.defrect.top, cropcap.defrect.width, cropcap.defrect.height);


    // check_res(vid_get_crop(adapter_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, &rect));
    // printf("crop: %d/%d %dx%d\n", rect.left, rect.top, rect.width, rect.height);

    printf("info: setting sensor capture format...\n");
    check_res(vid_set_format_checked(sensor_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, 2028, 1080, V4L2_PIX_FMT_SRGGB12));
    printf("info: setting adapter output format...\n");
    check_res(vid_set_format_mp_checked(adapter_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, 2028, 1080, V4L2_PIX_FMT_SRGGB12, 1));
    printf("info: setting adapter capture format...\n");
    check_res(vid_set_format_mp_checked(adapter_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, 1920, 1080, V4L2_PIX_FMT_RGB24, 1));
    printf("info: setting encoder output format...\n");
    check_res(vid_set_format_mp_checked(encoder_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, 1920, 1080, V4L2_PIX_FMT_RGB24, 1));
    printf("info: setting encoder capture format...\n");
    check_res(vid_set_format_mp_checked(encoder_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, 1920, 1080, V4L2_PIX_FMT_H264, 1));

    // We request 4 mmap buffers for the IMX sensor, we then export the buffers as DMABUF
    // file descriptors. The encoder output is using DMA buffers that will be given from
    // the IMX exported buffers. We'll then use a memory mapped region to fetch the 
    // encoded H264 data.
    printf("info: requesting buffers...\n");
    check_res(vid_request_mmap_buffers(sensor_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, BUFFERS_COUNT));
    check_res(vid_request_dma_buffers(adapter_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, BUFFERS_COUNT));
    check_res(vid_request_mmap_buffers(adapter_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, BUFFERS_COUNT));
    check_res(vid_request_dma_buffers(encoder_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, BUFFERS_COUNT));
    check_res(vid_request_mmap_buffers(encoder_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, BUFFERS_COUNT));

    printf("info: init sensor capture buffers...\n");
    int sensor_dmabuf_fd[BUFFERS_COUNT] = {0};
    for (unsigned i = 0; i < BUFFERS_COUNT; i++) {
        check_res(vid_export_mmap_buffer(sensor_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, i, &sensor_dmabuf_fd[i]));
        check_res(vid_queue_mmap_buffer(sensor_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, i));
    }

    printf("info: init adapter capture buffer...\n");
    int adapter_dmabuf_fd[BUFFERS_COUNT] = {0};
    for (unsigned i = 0; i < BUFFERS_COUNT; i++) {
        check_res(vid_export_mmap_buffer_mp(adapter_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, i, 0, &adapter_dmabuf_fd[i]));
        check_res(vid_queue_mmap_buffer_mp(adapter_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, i, 1));
    }

    printf("info: init encoder capture buffer...\n");
    struct encoder_buffer encoder_buffers[BUFFERS_COUNT] = {0};
    for (unsigned i = 0; i < BUFFERS_COUNT; i++) {

        unsigned length, offset;
        check_res(vid_query_mmap_buffer_mp(encoder_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, i, 1, &length, &offset));

        void *start = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, encoder_fd, offset);
        if (!start) {
            printf("error: failed to memory map\n");
            exit(1);
        }

        encoder_buffers[i].start = start;
        encoder_buffers[i].length = length;

        check_res(vid_queue_mmap_buffer_mp(encoder_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, i, 1));

    }

    printf("info: switch on devices...\n");
    check_res(vid_stream_on(sensor_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE));
    check_res(vid_stream_on(adapter_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE));
    check_res(vid_stream_on(adapter_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE));
    check_res(vid_stream_on(encoder_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE));
    check_res(vid_stream_on(encoder_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE));

    printf("info: looping...\n");

    struct pollfd fds[3] = {0};
    fds[0].fd = sensor_fd;
    fds[0].events = POLLIN;
    fds[1].fd = adapter_fd;
    fds[1].events = POLLIN;
    fds[2].fd = encoder_fd;
    fds[2].events = POLLIN;

    for (;;) {

        int ret = poll(fds, 3, 2000);
        if (ret == 0) {
            fprintf(stderr, "error: poll timed out\n");
            exit(1);
        } else if (ret == -1 && errno == EINTR) {
            continue;
        } else if (ret == -1) {
            fprintf(stderr, "error: poll error (%s)\n", strerror(errno));
            exit(1);
        }

        short int sensor_events = fds[0].revents;
        short int adapter_events = fds[1].revents;
        short int encoder_events = fds[2].revents;

        if (sensor_events & POLLERR) {
            fprintf(stderr, "error: sensor error\n");
        } else if (sensor_events & POLLIN) {

            // Start by unqueueing a potential captured buffer.            
            struct v4l2_buffer cap_buf = {0};
            cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            cap_buf.memory = V4L2_MEMORY_MMAP;

            if (check_ok_or_retry(vid_unqueue_buffer(sensor_fd, &cap_buf))) {

                // Once we successfully captured a buffer, we get the DMABUF file 
                // descriptor associated to that buffer in order to pass it to the
                // adapter device that convert the image format, in order to be later
                // accepted by H.264 encoder.
                int dmabuf_fd = sensor_dmabuf_fd[cap_buf.index];
                printf("info: sensor buffer %d with %d bytes (fd %d)\n", cap_buf.index, cap_buf.bytesused, dmabuf_fd);

                // We configured the adapter device to take planar buffer (because it
                // only accepts that), but because there is only one plane we just use
                // a regular variable, we should set the length and byteused of the 
                // buffer pointed to by the given DMABUF.
                struct v4l2_plane out_plane = {0};
                out_plane.m.fd = dmabuf_fd;
                out_plane.length = cap_buf.length;
                out_plane.bytesused = cap_buf.bytesused;

                // Note that we are importing most of the parameters from captured buffer
                // like the index, because we configured as many sensor capture buffers as
                // adapter output buffers.
                struct v4l2_buffer out_buf = {0};
                out_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
                out_buf.memory = V4L2_MEMORY_DMABUF;
                out_buf.timestamp = cap_buf.timestamp;
                out_buf.field = cap_buf.field;
                out_buf.index = cap_buf.index;
                out_buf.m.planes = &out_plane;
                out_buf.length = 1;

                check_res(vid_queue_buffer(adapter_fd, &out_buf));

            } else {
                printf("info: sensor unqueue retry...\n");
            }

        }

        if (adapter_events & POLLERR) {
            fprintf(stderr, "error: adapter error\n");
        } else if (adapter_events & POLLIN) {

            // Start by unqueueing a potential captured buffer.       
            struct v4l2_plane cap_plane = {0};     
            struct v4l2_buffer cap_buf = {0};
            cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            cap_buf.memory = V4L2_MEMORY_MMAP;
            cap_buf.m.planes = &cap_plane;
            cap_buf.length = 1;

            if (check_ok_or_retry(vid_unqueue_buffer(adapter_fd, &cap_buf))) {

                // Read the comment above, the pattern is the same here except the the 
                // capture buffer is planar instead of non-planar, but this make no major
                // difference since we have one plane.
                int dmabuf_fd = adapter_dmabuf_fd[cap_buf.index];
                printf("info: adapter buffer %d with %d bytes (fd %d)\n", cap_buf.index, cap_plane.bytesused, dmabuf_fd);

                struct v4l2_plane out_plane = {0};
                out_plane.m.fd = dmabuf_fd;
                out_plane.length = cap_buf.length;
                out_plane.bytesused = cap_buf.bytesused;

                struct v4l2_buffer out_buf = {0};
                out_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
                out_buf.memory = V4L2_MEMORY_DMABUF;
                // out_buf.timestamp = cap_buf.timestamp;
                out_buf.field = cap_buf.field;
                out_buf.index = cap_buf.index;
                out_buf.m.planes = &out_plane;
                out_buf.length = 1;

                check_res(vid_queue_buffer(encoder_fd, &out_buf));

            }

            // TODO: Unqueue output buffers and queue back capture one to the sensor.

        }

        if (encoder_events & POLLERR) {
            // fprintf(stderr, "error: encoder error\n");
        } else if (encoder_events & POLLIN) {
            // Process the encoded frame from its memory mapping (send it over UDP?).
            printf("info: encoder pollin\n");
        }

        sleep(1);

    }

    return 0;

}
