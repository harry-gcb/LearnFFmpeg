#ifndef FFPLAY_PACKETQUEUE_H_
#define FFPLAY_PACKETQUEUE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "libavcodec/packet.h"
#include "libavutil/fifo.h"
#include "libavutil/avutil.h"

#ifdef __cplusplus
}
#endif

#include "SDL2/SDL_thread.h"

typedef struct PacketQueue {
    AVFifo *pkt_list; // 实际的FIFO，实际上存的是MyAVPacketList
    int nb_packets; // AVPacket的数量
    int size;       // (MyPacketList的大小+AVPacket->size) * nb_packets
    int64_t duration; // AVPacket->duration 的累积值
    int abort_request; // 是否终止请求
    // 队列的序号，每次跳转播放时间点 ，serial 就会 +1。
    // 另一个数据结构 MyAVPacketList 里面也有一个 serial 字段，两个 serial 通过比较匹配来丢弃无效的缓存帧
    int serial;
    SDL_mutex *mutex; // 队列锁
    SDL_cond *cond;   // 队列信号
} PacketQueue;

// 初始化AVPacket队列
int packet_queue_init(PacketQueue *q);
// 从A队列取出一个AVPacket
int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial);
// push一个AVPacket到队列
int packet_queue_put(PacketQueue *q, AVPacket *pkt);
// push一个空的AVPacket到队列
int packet_queue_put_nullpacket(PacketQueue *q, AVPacket *pkt, int stream_index);
// 终止请求，不允许写入AVPacket
void packet_queue_abort(PacketQueue *q);
// 清空AVPacket队列
void packet_queue_flush(PacketQueue *q);
// 启用AVPacket队列
void packet_queue_start(PacketQueue *q);
// 销毁AVPacket队列
void packet_queue_destroy(PacketQueue *q);


#endif