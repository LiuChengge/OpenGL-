#ifndef MJPEG2JPEG_H
#define MJPEG2JPEG_H

using byte = unsigned char;

bool mjpeg2jpeg(const byte *in, const unsigned int in_size, byte *out, unsigned int out_limit_size, unsigned int *out_size);

#endif // MJPEG2JPEG_H
