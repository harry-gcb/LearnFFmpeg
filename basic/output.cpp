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
    if (argc < 3) {
        printf("usage: ./a.out infile outfile\n");
        return -1;
    }
    int ret = 0;
    AVFormatContext *in_fmt_ctx = avformat_alloc_context();
    if (!in_fmt_ctx) {
        printf("avformat_alloc_context failed\n");
        return -1;
    }

    ret = avformat_open_input(&in_fmt_ctx, argv[1], nullptr, nullptr);
    if (ret < 0) {
        printf("avformat_open_input failed: %d\n", ret);
        return -1;
    }

    AVCodecContext *in_codec_ctx = avcodec_alloc_context3(nullptr);
    if (!in_codec_ctx) {
        printf("avcodec_alloc_context3 failed\n");
        return -1;
    }
    ret = avcodec_parameters_to_context(in_codec_ctx, in_fmt_ctx->streams[0]->codecpar);
    if (ret < 0) {
        printf("avcodec_parameters_to_context failed: %d\n", ret);
        return -1;
    }
    AVCodec *in_codec = (AVCodec *)avcodec_find_decoder(in_codec_ctx->codec_id);
    if (!in_codec) {
        printf("avcodec_find_in_codec failed\n");
        return -1;
    }
    ret = avcodec_open2(in_codec_ctx, in_codec, nullptr);
    if (ret < 0) {
        printf("avcodec_open2 failed\n");
        return -1;
    }
    // open output file muxer
    AVFormatContext *out_fmt_ctx = nullptr;
    ret = avformat_alloc_output_context2(&out_fmt_ctx, nullptr, nullptr, argv[2]);
    if (ret < 0 || !out_fmt_ctx) {
        printf("avformat_alloc_output_context2 failed: %d\n", ret);
        return -1;
    }
    // add one stream to muxer
    AVStream *stream = avformat_new_stream(out_fmt_ctx, nullptr);
    stream->time_base = in_fmt_ctx->streams[0]->time_base;

    AVCodecContext *out_codec_ctx = nullptr;
    AVFrame *frame = av_frame_alloc();
    AVPacket *in_pkt = av_packet_alloc();
    AVPacket *out_pkt = av_packet_alloc();

    int pkt_size = 0;
    int eof = 0;
    while (!eof) {
        ret = av_read_frame(in_fmt_ctx, in_pkt);
        if (1 == in_pkt->stream_index) {
            av_packet_unref(in_pkt);
            continue;
        }
        if (ret != 0 && ret != AVERROR_EOF) {
            printf("av_read_frame in_pkt failed: %d\n", ret);
            break;
        }
retry:
        ret = avcodec_send_packet(in_codec_ctx, in_pkt);
        if (AVERROR(EAGAIN) == ret) {
            printf("avcodec_send_packet in_pkt failed: %d\n", ret);
            goto retry;
        }
        while (!eof) {
            ret = avcodec_receive_frame(in_codec_ctx, frame);
            if (AVERROR(EAGAIN) == ret) {
                break;
            } else if (AVERROR_EOF == ret) {
                eof = 1;
            } else if (ret < 0) {
                printf("avcodec_receive_frame failed: %d\n", ret);
                return ret;
            }
            if (!out_codec_ctx) {
                AVCodec *out_codec = (AVCodec *)avcodec_find_encoder(AV_CODEC_ID_H264);
                out_codec_ctx = avcodec_alloc_context3(out_codec);
                out_codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
                out_codec_ctx->bit_rate = 400000;
                out_codec_ctx->framerate = in_codec_ctx->framerate;
                out_codec_ctx->gop_size = 10;
                out_codec_ctx->max_b_frames = 5;
                out_codec_ctx->profile = FF_PROFILE_H264_MAIN;

                out_codec_ctx->time_base = in_fmt_ctx->streams[0]->time_base; // frame->time_base;
                out_codec_ctx->width = frame->width;
                out_codec_ctx->height = frame->height;
                out_codec_ctx->sample_aspect_ratio = frame->sample_aspect_ratio;
                out_codec_ctx->pix_fmt = (AVPixelFormat)frame->format;
                out_codec_ctx->color_range = frame->color_range;
                out_codec_ctx->color_primaries = frame->color_primaries;
                out_codec_ctx->color_trc = frame->color_trc;
                out_codec_ctx->colorspace = frame->colorspace;
                out_codec_ctx->chroma_sample_location = frame->chroma_location;

                out_codec_ctx->field_order = AV_FIELD_PROGRESSIVE;

                ret = avcodec_parameters_from_context(stream->codecpar, out_codec_ctx);
                if (ret < 0) {
                    printf("avcodec_parameters_from_context failed: %d\n", ret);
                    return ret;
                }

                ret = avcodec_open2(out_codec_ctx, out_codec, nullptr);
                if (ret < 0) {
                    printf("avcodec_open2 failed: %d\n", ret);
                    return ret;
                }

                ret = avio_open2(&out_fmt_ctx->pb, argv[2], AVIO_FLAG_WRITE, &out_fmt_ctx->interrupt_callback, nullptr);
                if (ret < 0) {
                    printf("avio_open2 failed: %d\n", ret);
                    return ret;
                }

                ret = avformat_write_header(out_fmt_ctx, nullptr);
                if (ret < 0) {
                    printf("avformat_write_header failed: %d\n", ret);
                    return ret;
                }
            }
            ret = avcodec_send_frame(out_codec_ctx, eof ? nullptr : frame);
            for (;;) {
                ret = avcodec_receive_packet(out_codec_ctx, out_pkt);
                if (AVERROR(EAGAIN) == ret || AVERROR_EOF == ret) {
                    break;
                } else if (ret < 0) {
                    printf("avcodec_receive_packet failed: %d\n", ret);
                }
                pkt_size += out_pkt->size;
                // printf("out_pkt size : %d, pkt_size: %d\n", out_pkt->size, pkt_size);
                out_pkt->stream_index = stream->index;
                out_pkt->pts = av_rescale_q_rnd(out_pkt->pts, in_fmt_ctx->streams[0]->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
                out_pkt->dts = av_rescale_q_rnd(out_pkt->dts, in_fmt_ctx->streams[0]->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
                out_pkt->duration = av_rescale_q_rnd(out_pkt->duration, in_fmt_ctx->streams[0]->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));

                ret = av_interleaved_write_frame(out_fmt_ctx, out_pkt);
                if (ret < 0) {
                    printf("av_interleaved_write_frame failed: %d\n", ret);
                    return 0;
                } 

                av_packet_unref(out_pkt);
            }
        }
        av_packet_unref(in_pkt);
    }

    av_write_trailer(out_fmt_ctx);

    av_frame_free(&frame);
    av_packet_free(&in_pkt);
    av_packet_free(&out_pkt);

    avcodec_close(in_codec_ctx);
    avcodec_close(out_codec_ctx);

    avio_closep(&out_fmt_ctx->pb);
    avformat_free_context(out_fmt_ctx);
    avformat_free_context(in_fmt_ctx);

    printf("done\n");
    return 0;
}