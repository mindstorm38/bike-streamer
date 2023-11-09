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
/// VIDEO SELECTION
///

enum vid_result vid_get_selection(int fd, struct v4l2_selection *sel) {
    if (RETRY_INT(ioctl(fd, VIDIOC_G_SELECTION, sel)) == -1)
        return VID_ERR_SYS;
    return VID_OK;
}

enum vid_result vid_set_selection(int fd, struct v4l2_selection *sel) {
    if (RETRY_INT(ioctl(fd, VIDIOC_S_SELECTION, sel)) == -1)
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

/// 
/// VIDEO STREAM PARAMETERS
///

enum vid_result vid_get_param(int fd, struct v4l2_streamparm *param) {
    if (RETRY_INT(ioctl(fd, VIDIOC_G_PARM, param)) == -1)
        return VID_ERR_SYS;
    return VID_OK;
}

enum vid_result vid_set_param(int fd, struct v4l2_streamparm *param) {
    if (RETRY_INT(ioctl(fd, VIDIOC_S_PARM, param)) == -1)
        return VID_ERR_SYS;
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
