#!/bin/bash

export GST_PLUGIN_PATH=/home/theo/libcamera/build/src/gstreamer
# export LIBCAMERA_LOG_LEVELS=*:DEBUG

gst-launch-1.0 libcamerasrc ! \
     video/x-raw,colorimetry=bt709,format=NV12,width=1280,height=720,framerate=10/1 ! \
     x264enc key-int-max=12 byte-stream=true ! mpegtsmux ! \
     tcpserversink host=0.0.0.0 port=5000

# gst-launch-1.0 v4l2src device=/dev/video0 ! \
#      x264enc key-int-max=12 byte-stream=true ! mpegtsmux ! \
#      tcpserversink host=0.0.0.0 port=5000
