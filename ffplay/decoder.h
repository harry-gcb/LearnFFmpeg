#ifndef FFPLAY_DECODER_H_
#define FFPLAY_DECODER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"

#ifdef __cplusplus
}
#endif

#include "SDL2/SDL_thread.h"

#include "packetqueue.h"
#include "framequeue.h"

typedef struct Decoder {
    AVPacket *pkt;
    PacketQueue *queue;
    AVCodecContext *avctx;
    int pkt_serial;
    int finished;
    int packet_pending;
    SDL_cond *empty_queue_cond;
    int64_t start_pts;
    AVRational start_pts_tb;
    int64_t next_pts;
    AVRational next_pts_tb;
    SDL_Thread *decoder_tid;
} Decoder;

// 初始化解码器
int decoder_init(Decoder *d, AVCodecContext *avctx, PacketQueue *queue, SDL_cond *empty_queue_cond);
// 解码数据
int decoder_decode_frame(Decoder *d, AVFrame *frame, AVSubtitle *sub);
// 开始解码
int decoder_start(Decoder *d, int (*fn)(void *), const char *thread_name, void* arg);
// 停止解码
void decoder_abort(Decoder *d, FrameQueue *fq);
// 销毁解码器
void decoder_destroy(Decoder *d);


#endif