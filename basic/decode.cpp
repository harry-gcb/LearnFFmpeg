#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/imgutils.h"

#ifdef __cplusplus
}
#endif

int main(int argc, char *argv[]) {
    if (argc < 2) {
        av_log(nullptr, AV_LOG_ERROR, "./a.out filename [type]\n");
        return 0;
    }
    int type = 1;
    if (3 == argc) {
        type = atoi(argv[2]);
    }
    int err = 0;
    AVFormatContext *fmt_ctx = nullptr;
    fmt_ctx = avformat_alloc_context();
    if (!fmt_ctx) {
        av_log(nullptr, AV_LOG_ERROR, "avformat_alloc_context failed\n");
        return 0;
    }
    err = avformat_open_input(&fmt_ctx, argv[1], nullptr, nullptr);
    if (err < 0) {
        av_log(nullptr, AV_LOG_ERROR, "avformat_open_input failed: %d\n", err);
        return 0;
    }

    int ret = 0;
    AVCodecContext *avctx = avcodec_alloc_context3(nullptr);
    ret = avcodec_parameters_to_context(avctx, fmt_ctx->streams[0]->codecpar);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "avcodec_parameters_to_context failed: %d\n", ret);
        return ret;
    }
    // avctx->codec_id = AV_CODEC_ID_H264;
    const AVCodec *codec = avcodec_find_decoder(avctx->codec_id);
    ret = avcodec_open2(avctx, codec, nullptr);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "avcodec_open2 failed: %d\n", ret);
        return ret;
    }

    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    int frame_num = 0;
    int read_end = 0;
    for (;;) {
        if (1 == read_end) {
            break;
        }
        ret = av_read_frame(fmt_ctx, pkt);
        if (1 == pkt->stream_index) {
            av_packet_unref(pkt);
            continue;
        }
        // 读取完文件，这时候 pkt 的 data 跟 size 应该是 null, 冲刷解码器
        avcodec_send_packet(avctx, pkt);
        // 循环不断从解码器读数据，直到没有数据可读
        for (;;) {
            // read frame
            ret = avcodec_receive_frame(avctx, frame);
            if (AVERROR(EAGAIN) == ret) {
                // 提示 EAGAIN 代表 解码器 需要 更多的 AVPacket
                // 跳出 第一层 for，让 解码器拿到更多的 AVPacket
                break;
            } else if (AVERROR_EOF == ret) {
                // 提示 AVERROR_EOF 代表之前已经往 解码器发送了一个 data 跟 size 都是 NULL 的 AVPacket
                // 发送 NULL 的 AVPacket 是提示解码器把所有的缓存帧全都刷出来。
                // 通常只有在 读完输入文件才会发送 NULL 的 AVPacket，或者需要用现有的解码器解码另一个的视频流才会这么干
                av_log(nullptr, AV_LOG_INFO, "averror_eof\n");
                read_end = 1;
                break;
            } else if (ret >= 0) {
                av_log(nullptr, AV_LOG_INFO, "decode success\n");
                av_log(nullptr, AV_LOG_INFO, "width: %d, height: %d\n", frame->width, frame->height);
                av_log(nullptr, AV_LOG_INFO, "pts: %lld\n", frame->pts);
                av_log(nullptr, AV_LOG_INFO, "key_frame: %d\n", frame->key_frame);
                av_log(nullptr, AV_LOG_INFO, "AVPictureType: %d\n", frame->pict_type);
                av_log(nullptr, AV_LOG_INFO, "Y size: %d\n", frame->linesize[0]);
                av_log(nullptr, AV_LOG_INFO, "U size: %d\n", frame->linesize[1]);
                av_log(nullptr, AV_LOG_INFO, "V size: %d\n", frame->linesize[2]);
                int num = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, 1920, 1080, 1);
                av_log(nullptr, AV_LOG_INFO, "num: %d\n", num);
            } else {
                av_log(nullptr, AV_LOG_ERROR, "failed\n");
                return ret;
            }
        }
        // 释放 pkt 里面的编码数据
        av_packet_unref(pkt);
    }
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_close(avctx);
    
    return 0;

}