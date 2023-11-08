# Bike Streamer Client

This program is the client-side running on the Raspberry PI, it is responsible of 
capturing video frames from the camera sensor with the v4l2 linux userspace API.
After some processing, frames and telemetry are sent over the network with a primitive
UDP socket, the server answers with the received frames in order to provides at bit
of loss detection, but is not intended to recover lost frames.

Usefull v4l2 or libcamera commands:
```
libcamera-hello --list-camera
v4l2-ctl --list-devices


```

Usufull links:
- https://docs.kernel.org/userspace-api/media/v4l/dmabuf.html !!!
- https://www.kernel.org/doc/html/next/userspace-api/media/v4l/dev-encoder.html !!!
- http://trac.gateworks.com/wiki/linux/media
- https://www.marcusfolkesson.se/blog/v4l2-and-media-controller/
- https://www.ffmpeg.org/doxygen/4.0/

Sensor base formats:
- 12-bit Bayer RGRG/GBGB Packed
- 12-bit Bayer RGRG/GBGB
- 10-bit Bayer RGRG/GBGB Packed
- 10-bit Bayer RGRG/GBGB
