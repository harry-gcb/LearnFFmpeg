#ifndef FFPLAY_FRAME_H_
#define FFPLAY_FRAME_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "libavcodec/avcodec.h"

#ifdef __cplusplus
}
#endif

/* Common struct for handling all types of decoded data and allocated render buffers. */
typedef struct Frame {
    AVFrame *frame;
    AVSubtitle sub;
    int serial;
    double pts;           /* presentation timestamp for the frame */
    double duration;      /* estimated duration of the frame */
    int64_t pos;          /* byte position of the frame in the input file */
    int width;
    int height;
    int format;
    AVRational sar;
    int uploaded;
    int flip_v;
} Frame;

#endif