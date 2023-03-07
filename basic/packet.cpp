#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/avformat.h"

#ifdef __cplusplus
}
#endif

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("./a.out filename");
        return -1;
    }
    int ret = -1;
    AVFormatContext *fmt_ctx = nullptr;
    fmt_ctx = avformat_alloc_context();
    if (!fmt_ctx) {
      printf("avformat_alloc_context failed\n");
      return ret;
    }
    ret = avformat_open_input(&fmt_ctx, argv[1], nullptr,nullptr);
    if(ret < 0){
        printf("avformat_open_input failed: %d\n", ret);
        return ret;
    }
    AVPacket *pkt = av_packet_alloc();
    for(int i = 0; i < 100 ;i++){
        ret = av_read_frame(fmt_ctx, pkt);
        if (ret < 0) {
            printf("av_packet_alloc failed: %d\n", ret);
            return ret;
        }else{
            printf("read success\n");
            printf("stream 0 type: %d\n",fmt_ctx->streams[0]->codecpar->codec_type);
            printf("stream 1 type: %d\n",fmt_ctx->streams[1]->codecpar->codec_type);
            printf("stream_index: %d\n",pkt->stream_index);
            printf("duration: %ld ,time_base: %d/%d\n",pkt->duration,
                    fmt_ctx->streams[1]->time_base.num,fmt_ctx->streams[0]->time_base.den);
            printf("size: %d\n",pkt->size);
            printf("pos: %ld\n",pkt->pos);
            printf("data: %x %x %x %x %x %x %x\n",
                    pkt->data[0],pkt->data[1],pkt->data[2],pkt->data[3],pkt->data[4],
                    pkt->data[5],pkt->data[6]);
        }
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    return 0;
}