#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/avformat.h"

#ifdef __cplusplus
}
#endif

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage: ./a.out filename\n");
        return -1;
    }
    int err = 0;
    int type = 1;
    AVDictionary *fmt_opts = nullptr;
    AVFormatContext *fmt_ctx = nullptr;
    fmt_ctx = avformat_alloc_context();
    if (!fmt_ctx) {
        printf("avformat_alloc_context: %d\n", AVERROR(ENOMEM));
        return -1;
    }
    if (2 == type) {
        av_dict_set(&fmt_opts, "probesize", "32", 0);
    }
    if ((err = avformat_open_input(&fmt_ctx, argv[1], nullptr, &fmt_opts)) < 0) {
        printf("avformat_open_input failed: %d\n", err);
    } else {
        avformat_find_stream_info(fmt_ctx, nullptr);
        printf("open success\n");
        printf("filename - %s\n", fmt_ctx->url);
        printf("duration - %lu\n", fmt_ctx->duration);
        printf("nb_streams - %u\n", fmt_ctx->nb_streams);
        for (int i=0; i<fmt_ctx->nb_streams; ++i) {
            printf("stream codec_type - %d\n", fmt_ctx->streams[i]->codecpar->codec_type);
        }
        printf("iformat name - %s\n", fmt_ctx->iformat->name);
        printf("iformat long name - %s\n", fmt_ctx->iformat->long_name);
    }
    if (2 == type) {
        av_dict_free(&fmt_opts);
    }
    return 0;
}