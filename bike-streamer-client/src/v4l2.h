/// A Video4Linux2 abstraction layer to ease pipelining.
/// This abstraction is specialized for streaming MMAP and DMABUF.

#include <linux/videodev2.h>


enum vid_result {
    VID_OK,
    VID_ERR_STOP,         // Stop enumeration
    VID_ERR_RETRY,        // Retry
    VID_ERR_SYS,          // System error in errno
    VID_ERR_NO_VIDEO,
    VID_ERR_NO_STREAMING,
    VID_ERR_NEGOCIATION,
};

enum vid_result vid_open(int *fd, const char *path);
enum vid_result vid_query_capability(int fd, struct v4l2_capability *dst);
enum vid_result vid_stream_on(int fd, enum v4l2_buf_type type);
enum vid_result vid_stream_off(int fd, enum v4l2_buf_type type);

enum vid_result vid_query_crop_capability(int fd, struct v4l2_cropcap *dst);
enum vid_result vid_get_crop(int fd, enum v4l2_buf_type type, struct v4l2_rect *dst);
enum vid_result vid_set_crop(int fd, enum v4l2_buf_type type, struct v4l2_rect src);

enum vid_result vid_enum_format(int fd, struct v4l2_fmtdesc *dst);
enum vid_result vid_get_format(int fd, struct v4l2_format *dst);
enum vid_result vid_set_format(int fd, struct v4l2_format *src);
enum vid_result vid_set_format_checked(int fd, enum v4l2_buf_type type, unsigned width, unsigned height, unsigned pixelformat);
enum vid_result vid_set_format_mp_checked(int fd, enum v4l2_buf_type type, unsigned width, unsigned height, unsigned pixelformat, unsigned planes);

enum vid_result vid_request_buffers(int fd, struct v4l2_requestbuffers *req);
enum vid_result vid_export_buffer(int fd, struct v4l2_exportbuffer *exp);
enum vid_result vid_query_buffer(int fd, struct v4l2_buffer *buf);
enum vid_result vid_queue_buffer(int fd, struct v4l2_buffer *buf);
enum vid_result vid_unqueue_buffer(int fd, struct v4l2_buffer *buf);


// SHORTCUT FOR REQUEST BUFFERS //
static enum vid_result vid_request_checked_buffers(int fd, enum v4l2_buf_type type, enum v4l2_memory memory, unsigned count) {

    struct v4l2_requestbuffers req = {0};
    req.type = type;
    req.memory = memory;
    req.count = count;

    enum vid_result res = vid_request_buffers(fd, &req);
    if (res != VID_OK)
        return res;
    if (req.count != count)
        return VID_ERR_NEGOCIATION;
    return VID_OK;

}

static inline enum vid_result vid_request_mmap_buffers(int fd, enum v4l2_buf_type type, unsigned count) {
    return vid_request_checked_buffers(fd, type, V4L2_MEMORY_MMAP, count);
}

static inline enum vid_result vid_request_dma_buffers(int fd, enum v4l2_buf_type type, unsigned count) {
    return vid_request_checked_buffers(fd, type, V4L2_MEMORY_DMABUF, count);
}

static enum vid_result vid_export_mmap_buffer(int fd, enum v4l2_buf_type type, unsigned index, int *dmabuf_fd) {

    struct v4l2_exportbuffer exp = {0};
    exp.type = type;
    exp.index = index;
    
    enum vid_result res = vid_export_buffer(fd, &exp);
    if (res != VID_OK)
        return res;

    *dmabuf_fd = exp.fd;
    return VID_OK;

}

static enum vid_result vid_export_mmap_buffer_mp(int fd, enum v4l2_buf_type type, unsigned index, unsigned plane, int *dmabuf_fd) {

    struct v4l2_exportbuffer exp = {0};
    exp.type = type;
    exp.index = index;
    exp.plane = plane;
    
    enum vid_result res = vid_export_buffer(fd, &exp);
    if (res != VID_OK)
        return res;

    *dmabuf_fd = exp.fd;
    return VID_OK;

}

// SHORTCUT FOR QUERY BUFFER (only for MMAP) //
static enum vid_result vid_query_mmap_buffer(int fd, enum v4l2_buf_type type, unsigned index, unsigned *length, unsigned *offset) {
    
    struct v4l2_buffer buf = {0};
    buf.type = type;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = index;

    enum vid_result res = vid_query_buffer(fd, &buf);
    if (res != VID_OK)
        return res;

    *length = buf.length;
    *offset = buf.m.offset;
    return VID_OK;

}

static enum vid_result vid_query_mmap_buffer_mp(int fd, enum v4l2_buf_type type, unsigned index, unsigned planes_count, unsigned *planes_length, unsigned *planes_offset) {
    
    struct v4l2_plane planes[VIDEO_MAX_PLANES] = {0};
    struct v4l2_buffer buf = {0};
    buf.type = type;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.m.planes = planes;
    buf.length = planes_count;

    enum vid_result res = vid_query_buffer(fd, &buf);
    if (res != VID_OK)
        return res;

    for (unsigned i = 0; i < planes_count; i++) {
        planes_length[i] = planes[i].length;
        planes_offset[i] = planes[i].m.mem_offset;
    }
    return VID_OK;

}

// SHORTCUT FOR QUEUE/DEQUEUE MMAP BUFFER //
static enum vid_result vid_queue_mmap_buffer(int fd, enum v4l2_buf_type type, unsigned index) {
    struct v4l2_buffer buf = {0};
    buf.type = type;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = index;
    return vid_queue_buffer(fd, &buf);
}

static enum vid_result vid_queue_mmap_buffer_mp(int fd, enum v4l2_buf_type type, unsigned index, unsigned planes_count) {
    struct v4l2_plane planes[VIDEO_MAX_PLANES] = {0};
    struct v4l2_buffer buf = {0};
    buf.type = type;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.m.planes = planes;
    buf.length = planes_count;
    return vid_queue_buffer(fd, &buf);
}

static enum vid_result vid_unqueue_mmap_buffer(int fd, enum v4l2_buf_type type, unsigned *index, unsigned *size) {
    
    struct v4l2_buffer buf = {0};
    buf.type = type;
    buf.memory = V4L2_MEMORY_MMAP;
    
    enum vid_result res = vid_unqueue_buffer(fd, &buf);
    if (res != VID_OK)
        return res;

    *index = buf.index;
    *size = buf.bytesused;
    return VID_OK;

}

static enum vid_result vid_unqueue_mmap_buffer_mp(int fd, enum v4l2_buf_type type, unsigned *index, unsigned planes_count, unsigned *planes_size) {
    
    struct v4l2_plane planes[VIDEO_MAX_PLANES] = {0};
    struct v4l2_buffer buf = {0};
    buf.type = type;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.m.planes = planes;
    buf.length = planes_count;
    
    enum vid_result res = vid_unqueue_buffer(fd, &buf);
    if (res != VID_OK)
        return res;

    *index = buf.index;
    for (unsigned i = 0; i < planes_count; i++) {
        planes_size[i] = planes[i].bytesused;
    }
    
    return VID_OK;

}

// SHORTCUT FOR QUEUE/DEQUEUE MMAP BUFFER //
static enum vid_result vid_queue_dma_buffer(int fd, enum v4l2_buf_type type, unsigned index, int dmabuf_fd) {
    struct v4l2_buffer buf = {0};
    buf.type = type;
    buf.memory = V4L2_MEMORY_DMABUF;
    buf.index = index;
    buf.m.fd = dmabuf_fd;
    return vid_queue_buffer(fd, &buf);
}

static enum vid_result vid_queue_dma_buffer_mp(int fd, enum v4l2_buf_type type, unsigned index, unsigned planes_count, int *planes_dmabuf_fd) {
    
    struct v4l2_plane planes[VIDEO_MAX_PLANES] = {0};
    for (unsigned i = 0; i < planes_count; i++) {
        planes[i].m.fd = planes_dmabuf_fd[i];
    }

    struct v4l2_buffer buf = {0};
    buf.type = type;
    buf.memory = V4L2_MEMORY_DMABUF;
    buf.m.planes = planes;
    buf.length = planes_count;

    return vid_queue_buffer(fd, &buf);

}

static enum vid_result vid_unqueue_dma_buffer(int fd, enum v4l2_buf_type type, unsigned *index, unsigned *size, int *dmabuf_fd) {
    
    struct v4l2_buffer buf = {0};
    buf.type = type;
    buf.memory = V4L2_MEMORY_DMABUF;
    
    enum vid_result res = vid_unqueue_buffer(fd, &buf);
    if (res != VID_OK)
        return res;

    *index = buf.index;
    *size = buf.bytesused;
    *dmabuf_fd = buf.m.fd;
    return VID_OK;

}

static enum vid_result vid_unqueue_dma_buffer_mp(int fd, enum v4l2_buf_type type, unsigned *index, unsigned planes_count, unsigned *planes_size, unsigned *planes_dmabuf_fd) {
    
    struct v4l2_plane planes[VIDEO_MAX_PLANES] = {0};
    struct v4l2_buffer buf = {0};
    buf.type = type;
    buf.memory = V4L2_MEMORY_DMABUF;
    buf.m.planes = planes;
    buf.length = planes_count;
    
    enum vid_result res = vid_unqueue_buffer(fd, &buf);
    if (res != VID_OK)
        return res;

    *index = buf.index;
    for (unsigned i = 0; i < planes_count; i++) {
        planes_size[i] = planes[i].bytesused;
        planes_dmabuf_fd[i] = planes[i].m.fd;
    }
    
    return VID_OK;

}
