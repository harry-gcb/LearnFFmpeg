#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"

#ifdef __cplusplus
}
#endif

int main(int argc, char *argv[]) {
    if (argc < 2) {
        av_log(nullptr, AV_LOG_INFO, "usage: ./a.out filename\n");
        return -1;
    }
    // open input
    int ret = 0;
    AVFormatContext *fmt_ctx = nullptr;
    fmt_ctx = avformat_alloc_context();
    if (!fmt_ctx) {
        av_log(nullptr, AV_LOG_INFO, "avformat_alloc_context failed\n");
        return -1;
    }
    ret = avformat_open_input(&fmt_ctx, argv[1], nullptr, nullptr);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_INFO, "avformat_open_input failed: %d\n", ret);
        return -1;
    }
    // open avcodec
    AVCodecContext *dec_ctx = avcodec_alloc_context3(nullptr);
    ret = avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[0]->codecpar);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_INFO, "avcodec_parameters_to_context failed: %d\n", ret);
        return -1;
    }
    AVCodec *decoder = (AVCodec *)avcodec_find_decoder(dec_ctx->codec_id);
    ret = avcodec_open2(dec_ctx, decoder, nullptr);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_INFO, "avcodec_open2 failed: %d\n", ret);
        return -1;
    }

    AVCodecContext *enc_ctx = nullptr;
    AVFrame *frame = av_frame_alloc();
    AVPacket *pkt_in = av_packet_alloc();
    AVPacket *pkt_out = av_packet_alloc();
    

    int frame_num = 0;
    int read_end = 0;

    for (;;) {
        if (read_end) {
            break;
        }
        ret = av_read_frame(fmt_ctx, pkt_in);
        if (1 == pkt_in->stream_index) {
            av_packet_unref(pkt_in);
            continue;
        }
        if (ret != 0 && ret != AVERROR_EOF) {
            av_log(nullptr, AV_LOG_ERROR, "av_read_frame failed: %d\n", ret);
            break;
        }
retry:
        ret = avcodec_send_packet(dec_ctx, pkt_in);
        if (AVERROR(EAGAIN) == ret) {
            av_log(nullptr, AV_LOG_WARNING, "avcodec_send_packet warning\n");
            goto retry;
        }
        for (;;) {
            ret = avcodec_receive_frame(dec_ctx, frame);
            if (AVERROR(EAGAIN) == ret) {
                break;
            } else if (AVERROR_EOF == ret) {
                read_end = 1;
            } else if (ret < 0) {
                av_log(nullptr, AV_LOG_ERROR, "avcodec_receive_frame failed: %d\n", ret);
                return ret;
            }
            if (nullptr == enc_ctx) {
                AVCodec *encoder = (AVCodec *)avcodec_find_encoder(AV_CODEC_ID_H264);
                enc_ctx = avcodec_alloc_context3(encoder);
                enc_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
                enc_ctx->bit_rate = 400000;
                enc_ctx->framerate = dec_ctx->framerate;
                enc_ctx->gop_size = 10;
                enc_ctx->max_b_frames = 5;
                enc_ctx->profile = FF_PROFILE_H264_MAIN;

                enc_ctx->time_base = fmt_ctx->streams[0]->time_base; // frame->time_base;
                enc_ctx->width = frame->width;
                enc_ctx->height = frame->height;
                enc_ctx->sample_aspect_ratio = frame->sample_aspect_ratio;
                enc_ctx->pix_fmt = (AVPixelFormat)frame->format;
                enc_ctx->color_range = frame->color_range;
                enc_ctx->color_primaries = frame->color_primaries;
                enc_ctx->color_trc = frame->color_trc;
                enc_ctx->colorspace = frame->colorspace;
                enc_ctx->chroma_sample_location = frame->chroma_location;

                enc_ctx->field_order = AV_FIELD_PROGRESSIVE;
                ret = avcodec_open2(enc_ctx, encoder, nullptr);
                if (ret < 0) {
                    av_log(nullptr, AV_LOG_ERROR, "avcodec_open2 failed: %d\n", ret);
                    return ret;
                }
            }
            ret = avcodec_send_frame(enc_ctx, read_end ? nullptr : frame);
            for (;;) {
                ret = avcodec_receive_packet(enc_ctx, pkt_out);
                if (AVERROR(EAGAIN) == ret || AVERROR_EOF == ret) {
                    break;
                } else if (ret < 0) {
                    av_log(nullptr, AV_LOG_ERROR, "avcodec_receive_packet failed: %d\n", ret);
                    return ret;
                }
                av_log(nullptr, AV_LOG_INFO, "pkt_out size: %d\n", pkt_out->size);
                av_packet_unref(pkt_out);
            }
            if (read_end) {
                break;
            }
        }
        av_packet_unref(pkt_in);
    }

    av_frame_free(&frame);
    av_packet_free(&pkt_in);
    av_packet_free(&pkt_out);

    avcodec_close(dec_ctx);
    avcodec_close(enc_ctx);

    avformat_free_context(fmt_ctx);
    av_log(nullptr, AV_LOG_INFO, "done\n");


    return 0;
}