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
struct buffer_map {
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

    FILE *out_file = fopen("out.h264", "w");
    if (!out_file) {
        fprintf(stderr, "error: failed to open output file (%s)\n", strerror(errno));
        exit(1);
    }

    FILE *out_raw_file = fopen("out.raw", "w");
    if (!out_file) {
        fprintf(stderr, "error: failed to open output raw file (%s)\n", strerror(errno));
        exit(1);
    }

    int sensor_fd;
    int adapter_fd;
    int encoder_fd;

    struct v4l2_capability cap = {0};
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

    printf("info: setting sensor capture format...\n");
    struct v4l2_format sensor_cap_fmt = {0};
    sensor_cap_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    sensor_cap_fmt.fmt.pix.width = 2028;
    sensor_cap_fmt.fmt.pix.height = 1520;
    sensor_cap_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SBGGR12P;
    sensor_cap_fmt.fmt.pix.colorspace = V4L2_COLORSPACE_RAW;
    check_res(vid_set_format(sensor_fd, &sensor_cap_fmt));
    // check_res(vid_set_checked_format(sensor_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, 2028, 1080, V4L2_PIX_FMT_SBGGR12P));

    printf("info: setting adapter capture format...\n");
    check_res(vid_set_checked_format_mp(adapter_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, 1920, 1080, V4L2_PIX_FMT_RGB24, 1));
    
    printf("info: setting adapter output format...\n");
    struct v4l2_format adapter_out_fmt = {0};
    adapter_out_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    adapter_out_fmt.fmt.pix_mp.width = 2028;
    adapter_out_fmt.fmt.pix_mp.height = 1520;
    adapter_out_fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_SBGGR12P;
    adapter_out_fmt.fmt.pix_mp.colorspace = sensor_cap_fmt.fmt.pix.colorspace;
    adapter_out_fmt.fmt.pix_mp.ycbcr_enc = sensor_cap_fmt.fmt.pix.ycbcr_enc;
    adapter_out_fmt.fmt.pix_mp.quantization = sensor_cap_fmt.fmt.pix.quantization;
    check_res(vid_set_format(adapter_fd, &adapter_out_fmt));

    // check_res(vid_set_checked_format_mp(adapter_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, 2028, 1080, V4L2_PIX_FMT_SBGGR12P, 1));
    
    printf("info: setting encoder capture format...\n");
    check_res(vid_set_checked_format_mp(encoder_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, 1920, 1080, V4L2_PIX_FMT_H264, 1));
    printf("info: setting encoder output format...\n");
    check_res(vid_set_checked_format_mp(encoder_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, 1920, 1080, V4L2_PIX_FMT_RGB24, 1));
    
    // TODO: Check that setting capture format didn't change the output format.
    // struct v4l2_format fmt = {0};
    // fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    // check_res(vid_get_format(adapter_fd, &fmt));

    struct v4l2_streamparm param = {0};
    param.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    check_res(vid_get_param(encoder_fd, &param));
    printf("info: encoder framerate: %d/%d\n", param.parm.output.timeperframe.numerator, param.parm.output.timeperframe.denominator);

    printf("info: adjusting adapter selection...\n");
    struct v4l2_rect adapter_crop = {
        .left = 0,
        .top = 0,
        .width = 1920,
        .height = 1080,
    };

    struct v4l2_rect adapter_compose = {
        .left = 0,
        .top = 0,
        .width = 1920,
        .height = 1080,
    };

    check_res(vid_set_checked_selection(adapter_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_SEL_TGT_CROP, V4L2_SEL_FLAG_GE | V4L2_SEL_FLAG_LE, adapter_crop));
    check_res(vid_set_checked_selection(adapter_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_SEL_TGT_COMPOSE, V4L2_SEL_FLAG_GE | V4L2_SEL_FLAG_LE, adapter_compose));
    
    printf("info: requesting buffers...\n");
    check_res(vid_request_mmap_buffers(sensor_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, BUFFERS_COUNT));
    check_res(vid_request_dma_buffers(adapter_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, BUFFERS_COUNT));
    check_res(vid_request_mmap_buffers(adapter_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, BUFFERS_COUNT));
    check_res(vid_request_dma_buffers(encoder_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, BUFFERS_COUNT));
    check_res(vid_request_mmap_buffers(encoder_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, BUFFERS_COUNT));
    
    printf("info: init sensor capture buffers...\n");
    int sensor_dmabuf_fd[BUFFERS_COUNT] = {0};
    struct buffer_map sensor_buffers_map[BUFFERS_COUNT] = {0};
    for (unsigned i = 0; i < BUFFERS_COUNT; i++) {

        unsigned length, offset;
        check_res(vid_query_mmap_buffer(sensor_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, i, &length, &offset));
        
        void *start = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, sensor_fd, offset);
        if (!start) {
            printf("error: failed to memory map\n");
            exit(1);
        }

        sensor_buffers_map[i].start = start;
        sensor_buffers_map[i].length = length;

        check_res(vid_export_mmap_buffer(sensor_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, i, &sensor_dmabuf_fd[i]));
        check_res(vid_queue_mmap_buffer(sensor_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, i));

    }

    printf("info: init adapter capture buffer...\n");
    int adapter_dmabuf_fd[BUFFERS_COUNT] = {0};
    struct buffer_map adapter_buffers_map[BUFFERS_COUNT] = {0};
    for (unsigned i = 0; i < BUFFERS_COUNT; i++) {
        
        unsigned length, offset;
        check_res(vid_query_mmap_buffer_mp(adapter_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, i, 1, &length, &offset));
        
        void *start = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, adapter_fd, offset);
        if (!start) {
            printf("error: failed to memory map\n");
            exit(1);
        }

        adapter_buffers_map[i].start = start;
        adapter_buffers_map[i].length = length;

        check_res(vid_export_mmap_buffer_mp(adapter_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, i, 0, &adapter_dmabuf_fd[i]));
        check_res(vid_queue_mmap_buffer_mp(adapter_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, i, 1));
    
    }

    printf("info: init encoder capture buffer...\n");
    struct buffer_map encoder_buffers_map[BUFFERS_COUNT] = {0};
    for (unsigned i = 0; i < BUFFERS_COUNT; i++) {

        unsigned length, offset;
        check_res(vid_query_mmap_buffer_mp(encoder_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, i, 1, &length, &offset));

        void *start = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, encoder_fd, offset);
        if (!start) {
            printf("error: failed to memory map\n");
            exit(1);
        }

        encoder_buffers_map[i].start = start;
        encoder_buffers_map[i].length = length;

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

    for (int z = 0; z < 1000; z++) {

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
            // sleep(5);
            // exit(1);
        } else if (sensor_events & POLLIN) {

            // Start by unqueueing a potential captured buffer.            
            struct v4l2_buffer cap_buf = {0};
            cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            cap_buf.memory = V4L2_MEMORY_MMAP;

            if (check_ok_or_retry(vid_unqueue_buffer(sensor_fd, &cap_buf))) {

                if (cap_buf.flags & V4L2_BUF_FLAG_ERROR) {
                    printf("warn: sensor buffer has error!\n");
                }

                // For debug purpose, we write the frame in the raw output file.
                struct buffer_map *map = &sensor_buffers_map[cap_buf.index];

                if (z >= 990) {
                    printf("info: writing raw file...\n");
                    ftruncate(fileno(out_raw_file), 0);
                    fseek(out_raw_file, 0, 0);
                    // fputs("P6 1920 1080 255\n", out_raw_file);
                    fwrite(map->start, 1, cap_buf.bytesused, out_raw_file);
                }

                // Once we successfully captured a buffer, we get the DMABUF file 
                // descriptor associated to that buffer in order to pass it to the
                // adapter device that convert the image format, in order to be later
                // accepted by H.264 encoder.
                int dmabuf_fd = sensor_dmabuf_fd[cap_buf.index];
                // printf("info: sensor buffer %d with %d bytes (fd %d)\n", cap_buf.index, cap_buf.bytesused, dmabuf_fd);

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

            }

        }

        if (adapter_events & POLLERR) {
            fprintf(stderr, "error: adapter error\n");
            // sleep(5);
            // exit(1);
        } else if (adapter_events & POLLIN) {

            struct v4l2_plane cap_plane = {0};     
            struct v4l2_buffer cap_buf = {0};
            cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            cap_buf.memory = V4L2_MEMORY_MMAP;
            cap_buf.m.planes = &cap_plane;
            cap_buf.length = 1;

            if (check_ok_or_retry(vid_unqueue_buffer(adapter_fd, &cap_buf))) {

                if (cap_buf.flags & V4L2_BUF_FLAG_ERROR) {
                    printf("warn: adapter buffer has error!\n");
                }
                
                // // For debug purpose, we write the frame in the raw output file.
                // struct buffer_map *map = &adapter_buffers_map[cap_buf.index];

                // if (z >= 990) {
                //     printf("info: writing raw ppm file...\n");
                //     ftruncate(fileno(out_raw_file), 0);
                //     fseek(out_raw_file, 0, 0);
                //     fputs("P6 1920 1080 255\n", out_raw_file);
                //     unsigned long written_size = fwrite(map->start, 1, cap_plane.bytesused, out_raw_file);
                // }

                // Read the comment above, the pattern is the same here except the the 
                // capture buffer is planar instead of non-planar, but this make no major
                // difference since we have one plane.
                int dmabuf_fd = adapter_dmabuf_fd[cap_buf.index];
                // printf("info: adapter buffer %d with %d bytes (fd %d)\n", cap_buf.index, cap_plane.bytesused, dmabuf_fd);

                struct v4l2_plane out_plane = {0};
                out_plane.m.fd = dmabuf_fd;
                out_plane.length = cap_plane.length; // Remember: use the plane info.
                out_plane.bytesused = cap_plane.bytesused;

                struct v4l2_buffer out_buf = {0};
                out_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
                out_buf.memory = V4L2_MEMORY_DMABUF;
                out_buf.timestamp = cap_buf.timestamp;
                out_buf.field = cap_buf.field;
                out_buf.index = cap_buf.index;
                out_buf.m.planes = &out_plane;
                out_buf.length = 1;

                check_res(vid_queue_buffer(encoder_fd, &out_buf));

            }

            // Try unqueuing a previous output buffer.
            struct v4l2_plane out_plane = {0};     
            struct v4l2_buffer out_buf = {0};
            out_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
            out_buf.memory = V4L2_MEMORY_DMABUF;
            out_buf.m.planes = &out_plane;
            out_buf.length = 1;

            if (check_ok_or_retry(vid_unqueue_buffer(adapter_fd, &out_buf))) {
                // printf("info: adapter output buffer %d unqueued (fd %d)\n", out_buf.index, out_plane.m.fd);
                check_res(vid_queue_mmap_buffer(sensor_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, out_buf.index));
            }

        }

        if (encoder_events & POLLERR) {
            fprintf(stderr, "error: encoder error\n");
            // sleep(5);
            // exit(1);
        } else if (encoder_events & POLLIN) {
            
            struct v4l2_plane cap_plane = {0};     
            struct v4l2_buffer cap_buf = {0};
            cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            cap_buf.memory = V4L2_MEMORY_MMAP;
            cap_buf.m.planes = &cap_plane;
            cap_buf.length = 1;

            if (check_ok_or_retry(vid_unqueue_buffer(encoder_fd, &cap_buf))) {

                // We reached the end of our pipeline! The fully encoded frame should be
                // available in the buffer that we just unqueued, we just need to know
                // we this frame is mapped in our memory.
                struct buffer_map *map = &encoder_buffers_map[cap_buf.index];

                printf("info: encoded buffer %d with %d bytes at %p\n", cap_buf.index, cap_plane.bytesused, map->start);
                // TODO: Process frame.

                if (cap_buf.flags & V4L2_BUF_FLAG_ERROR) {
                    printf("warn: encoded buffer has error!\n");
                }

                unsigned long written_size = fwrite(map->start, 1, cap_plane.bytesused, out_file);
                printf("info: written size %lu\n", written_size);

                // Queue the capture buffer after frame has been processed.
                check_res(vid_queue_buffer(encoder_fd, &cap_buf));

            }

            // Try unqueuing a previous output buffer.
            struct v4l2_plane out_plane = {0};     
            struct v4l2_buffer out_buf = {0};
            out_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
            out_buf.memory = V4L2_MEMORY_DMABUF;
            out_buf.m.planes = &out_plane;
            out_buf.length = 1;

            if (check_ok_or_retry(vid_unqueue_buffer(encoder_fd, &out_buf))) {
                // printf("info: encoder output buffer %d unqueued (fd %d)\n", out_buf.index, out_plane.m.fd);
                check_res(vid_queue_mmap_buffer_mp(adapter_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, out_buf.index, 1));
            }

        }

    }

    return 0;

}
