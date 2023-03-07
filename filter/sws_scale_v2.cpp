#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"

#ifdef __cplusplus
}
#endif

#define OUT_WIDTH 480
#define OUT_HEIGHT 320
#define OUT_FORMAT AV_PIX_FMT_YUV422P

int main(int argc, char *argv[]) {
    int ret = -1;
    if (argc <= 2) {
        printf("usage: ./a.out infile outfile\n");
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
    // av_dump_format(in_fmt_ctx, 0, argv[1], 0);
    AVCodecContext *in_codec_ctx = avcodec_alloc_context3(nullptr);
    if (!in_codec_ctx) {
        printf("avcodec_alloc_context3 failed\n");
        return ret;
    }
    ret = avcodec_parameters_to_context(in_codec_ctx, in_fmt_ctx->streams[0]->codecpar);
    if (ret != 0) {
        printf("avcodec_parameters_to_context failed: %d\n", ret);
        return ret;
    }
    AVCodec *in_codec = (AVCodec *)avcodec_find_decoder(in_codec_ctx->codec_id);
    if (!in_codec) {
        printf("avcodec_find_decoder failed\n");
        return ret;
    }
    ret = avcodec_open2(in_codec_ctx, in_codec, nullptr);
    if (ret != 0) {
        printf("avcodec_open2 failed: %d\n", ret);
        return ret;
    }

    AVFormatContext *out_fmt_ctx = nullptr;
    ret = avformat_alloc_output_context2(&out_fmt_ctx, nullptr, nullptr, argv[2]);
    if (ret != 0) {
        printf("avformat_alloc_output_context2 failed: %d\n", ret);
        return ret;
    }
    AVStream *stream = avformat_new_stream(out_fmt_ctx, nullptr);
    if (!stream) {
        printf("avformat_new_stream failed\n");
        return ret;
    }

    AVCodecContext *out_codec_ctx = nullptr;

    AVPacket *in_pkt = av_packet_alloc();
    AVFrame *in_frame = av_frame_alloc();

    AVPacket *out_pkt = av_packet_alloc();
    AVFrame *out_frame = av_frame_alloc();
    out_frame->format = OUT_FORMAT;
    out_frame->width = OUT_WIDTH;
    out_frame->height = OUT_HEIGHT;
    int buf_size = av_image_get_buffer_size((AVPixelFormat)out_frame->format, out_frame->width, out_frame->height, 1);
    struct SwsContext* img_convert_ctx = NULL;

    int eof = 0;
    int frame_num = 0;
    while (!eof) {
        ret = av_read_frame(in_fmt_ctx, in_pkt);
        if (ret != 0 && ret != AVERROR_EOF) {
            printf("av_read_frame failed: %d\n", ret);
            break;
        }
        if (1 == in_pkt->stream_index) {
            av_packet_unref(in_pkt);
            continue;
        }
        ret = avcodec_send_packet(in_codec_ctx, in_pkt);
        if (ret != 0) {
            printf("avcodec_send_packet failed: %d\n", ret);
            break;
        }
        while (!eof) {
            ret = avcodec_receive_frame(in_codec_ctx, in_frame);
            if (AVERROR_EOF == ret) {
                eof = 1;
            } else if (AVERROR(EAGAIN) == ret) {
                break;
            } else if (ret != 0) {
                printf("avcodec_receive_frame failed: %d\n", ret);
                return ret;
            }
            if (!out_codec_ctx) {
                AVCodec *out_codec = (AVCodec *)avcodec_find_encoder(AV_CODEC_ID_H264);
                if (!out_codec) {
                    printf("avcodec_find_encoder failed\n");
                    return ret;
                }
                out_codec_ctx = avcodec_alloc_context3(out_codec);
                if (!out_codec_ctx) {
                    printf("avcodec_alloc_context3 failed\n");
                    return ret;
                }
                out_codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
                out_codec_ctx->bit_rate = in_codec_ctx->bit_rate;
                out_codec_ctx->framerate = in_codec_ctx->framerate;
                out_codec_ctx->gop_size = in_codec_ctx->gop_size;
                out_codec_ctx->max_b_frames = in_codec_ctx->max_b_frames;
                out_codec_ctx->profile = FF_PROFILE_H264_MAIN;

                out_codec_ctx->time_base = in_fmt_ctx->streams[0]->time_base; // frame->time_base;

                out_codec_ctx->width = OUT_WIDTH;
                out_codec_ctx->height = OUT_HEIGHT;
                out_codec_ctx->pix_fmt = OUT_FORMAT;
                out_codec_ctx->sample_aspect_ratio = in_frame->sample_aspect_ratio;
                out_codec_ctx->color_range = in_frame->color_range;
                out_codec_ctx->color_primaries = in_frame->color_primaries;
                out_codec_ctx->color_trc = in_frame->color_trc;
                out_codec_ctx->colorspace = in_frame->colorspace;
                out_codec_ctx->chroma_sample_location = in_frame->chroma_location;

                out_codec_ctx->field_order = AV_FIELD_PROGRESSIVE;

                stream->time_base = out_codec_ctx->time_base;
                ret = avcodec_parameters_from_context(stream->codecpar, out_codec_ctx);
                if (ret != 0) {
                    printf("avcodec_parameters_from_context failed: %d\n", ret);
                    return ret;
                }
                ret = avcodec_open2(out_codec_ctx, out_codec, nullptr);
                if (ret != 0) {
                    printf("avcodec_open2 failed: %d\n", ret);
                    return ret;
                }
                ret = avio_open2(&out_fmt_ctx->pb, argv[2], AVIO_FLAG_WRITE, &out_fmt_ctx->interrupt_callback, nullptr);
                if (ret != 0) {
                    printf("avio_open2 failed: %d\n", ret);
                    return ret;
                }
                ret = avformat_write_header(out_fmt_ctx, nullptr);
                if (ret != 0) {
                    printf("avformat_write_header failed: %d\n", ret);
                    return ret;
                }
            }
            if (!img_convert_ctx) {
                img_convert_ctx = sws_getCachedContext(img_convert_ctx, in_frame->height, in_frame->height, (AVPixelFormat)in_frame->format,
                                                       out_frame->width, out_frame->height, (AVPixelFormat)out_frame->format, 
                                                       SWS_BICUBIC, nullptr, nullptr, nullptr);
                if (!img_convert_ctx) {
                    printf("sws_getCachedContext failed\n");
                    return ret;
                }   
            }
            uint8_t *buffer = (uint8_t *)av_malloc(buf_size);
            if (!buffer) {
                printf("av_malloc failed\n");
                return ret;
            }
            ret = av_image_fill_arrays(out_frame->data, out_frame->linesize, buffer, (AVPixelFormat)out_frame->format, out_frame->width, out_frame->height, 1);
            if (ret != 0) {
                printf("av_image_fill_arrays failed: %d\n", ret);
                return ret;
            }
            ret = sws_scale(img_convert_ctx, (const uint8_t* const*)in_frame->data, in_frame->linesize, 0, in_frame->height, out_frame->data, out_frame->linesize);
            if (ret != 0) {
                printf("sws_scale failed: %d\n", ret);
                return ret;
            }
            ret = avcodec_send_frame(out_codec_ctx, out_frame);
            for (;;) {
                ret = avcodec_receive_packet(out_codec_ctx, out_pkt);
                if (AVERROR(EAGAIN) == ret || AVERROR_EOF == ret) {
                    break;
                } else if (ret != 0) {
                    printf("avcodec_receive_packet failed: %d\n", ret);
                    return ret;
                }

                out_pkt->stream_index = stream->index;
                out_pkt->pts = av_rescale_q_rnd(out_pkt->pts, in_fmt_ctx->streams[0]->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
                out_pkt->dts = av_rescale_q_rnd(out_pkt->dts, in_fmt_ctx->streams[0]->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
                out_pkt->duration = av_rescale_q_rnd(out_pkt->duration, in_fmt_ctx->streams[0]->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));

                ret = av_interleaved_write_frame(out_fmt_ctx, out_pkt);
                if (ret != 0) {
                    printf("av_interleaved_write_frame failed: %d\n", ret);
                    return ret;
                } 
                av_packet_unref(out_pkt);
            }
            av_frame_unref(in_frame);
            av_frame_unref(out_frame);
        }
        av_packet_unref(in_pkt);
    }

    av_write_trailer(out_fmt_ctx);
    
    av_packet_free(&in_pkt);
    av_frame_free(&in_frame);
    av_packet_free(&out_pkt);
    av_frame_free(&out_frame);

    sws_freeContext(img_convert_ctx);

    avcodec_close(out_codec_ctx);
    avcodec_close(in_codec_ctx);

    avio_closep(&out_fmt_ctx->pb);
    avformat_free_context(out_fmt_ctx);
    avformat_free_context(in_fmt_ctx);

    return ret;
}