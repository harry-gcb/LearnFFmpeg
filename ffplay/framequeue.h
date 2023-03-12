#ifndef FFPLAY_FRAMEQUEUE_H_
#define FFPLAY_FRAMEQUEUE_H_

#include "SDL2/SDL_thread.h"

#include "frame.h"
#include "packetqueue.h"

#define VIDEO_PICTURE_QUEUE_SIZE 3
#define SUBPICTURE_QUEUE_SIZE 16
#define SAMPLE_QUEUE_SIZE 9
#define FRAME_QUEUE_SIZE FFMAX(SAMPLE_QUEUE_SIZE, FFMAX(VIDEO_PICTURE_QUEUE_SIZE, SUBPICTURE_QUEUE_SIZE))

typedef struct FrameQueue {
    Frame queue[FRAME_QUEUE_SIZE];
    int rindex;
    int windex;
    int size;
    int max_size;
    int keep_last;
    int rindex_shown;
    SDL_mutex *mutex;
    SDL_cond *cond;
    PacketQueue *pktq;
} FrameQueue;

void frame_queue_unref_item(Frame *vp);
// 初始化AVFrame队列
int frame_queue_init(FrameQueue *f, PacketQueue *pktq, int max_size, int keep_last);
void frame_queue_destory(FrameQueue *f);
void frame_queue_signal(FrameQueue *f);
Frame *frame_queue_peek(FrameQueue *f);
Frame *frame_queue_peek_next(FrameQueue *f);
Frame *frame_queue_peek_last(FrameQueue *f);
Frame *frame_queue_peek_writable(FrameQueue *f);
Frame *frame_queue_peek_readable(FrameQueue *f);
void frame_queue_push(FrameQueue *f);
void frame_queue_next(FrameQueue *f);
int frame_queue_nb_remaining(FrameQueue *f);
int64_t frame_queue_last_pos(FrameQueue *f);

#endif