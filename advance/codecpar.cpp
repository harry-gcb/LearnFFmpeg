#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/opt.h"

#ifdef __cplusplus
}
#endif

int main(int argc, char *argv[]) {
    AVCodec *codec = (AVCodec *)avcodec_find_encoder(AV_CODEC_ID_H264);
    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    const AVOption *opt = nullptr;
    while (opt = av_opt_next(codec_ctx, opt)) {
        printf("common opt name: %s\n", opt->name);
    }
    while (opt = av_opt_next(codec_ctx->priv_data, opt)) {
        printf("private opt name: %s\n", opt->name);
    }
    avcodec_free_context(&codec_ctx);
    return 0;
}