#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/bprint.h"

#ifdef __cplusplus
}
#endif

static int read_packet(AVFormatContext *in_fmt_ctx) {
    int ret = 0;
    AVPacket *pkt = av_packet_alloc();
    ret = av_read_frame(in_fmt_ctx, pkt);
    if (ret != 0) {
        printf("av_read_frame failed: %d\n", ret);
    } else {
        printf("stream_index=%d, pts=%ld, pos=%d, data=%x %x %x %x %x %x\n", pkt->stream_index, pkt->pts, pkt->pos, 
                pkt->data[0], pkt->data[1], pkt->data[2], pkt->data[3], pkt->data[4], pkt->data[5]);
    }
    av_packet_unref(pkt);
    av_packet_free(&pkt);
    return ret;
}

int main(int argc, char *argv[]) {
    int ret = -1;
    if (argc < 2) {
        printf("usage: ./a.out infile\n");
        return ret;
    }
    
    AVFormatContext *in_fmt_ctx = avformat_alloc_context();
    if (!in_fmt_ctx) {
        printf("avformat_alloc_context failed\n");
        return ret;
    }
    ret = avformat_open_input(&in_fmt_ctx, argv[1], nullptr, nullptr);
    if (ret != 0) {
        printf("avformat_open_input failed: %d\n", ret);
        return ret;
    }

    read_packet(in_fmt_ctx);
    read_packet(in_fmt_ctx);
    read_packet(in_fmt_ctx);
    read_packet(in_fmt_ctx);
    int64_t seek_target = AV_TIME_BASE;
    int64_t seek_min = INT64_MIN;
    int64_t seek_max = INT64_MAX;
    int flags = 0;

    ret = avformat_seek_file(in_fmt_ctx, -1, seek_min, seek_target, seek_max, flags);
    if (ret != 0) {
        printf("avformat_seek_file failed:%d\n", ret);
        return ret;
    }

    read_packet(in_fmt_ctx);
    read_packet(in_fmt_ctx);
    read_packet(in_fmt_ctx);
    read_packet(in_fmt_ctx);

    avformat_free_context(in_fmt_ctx);
    return ret;
}