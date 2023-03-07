#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
#include "libavutil/bprint.h"

#ifdef __cplusplus
}
#endif

int main(int argc, char *argv[]) {
    int ret = -1;
    if (argc <= 2) {
        printf("usage: ./a.out infile outfile\n");
        return ret;
    }
    AVFormatContext *in_fmt_ctx = avformat_alloc_context();
    if (!in_fmt_ctx) {
        printf("avformat_alloc_context failed: %d\n", ret);
        return ret;
    }
    ret = avformat_open_input(&in_fmt_ctx, argv[1], nullptr, nullptr);
    if (ret != 0) {
        printf("avformat_open_input failed: %d\n", ret);
        return ret;
    }

    AVCodecContext *in_codec_ctx = avcodec_alloc_context3(nullptr);
    if (!in_codec_ctx) {
        printf("avcodec_alloc_context3 failed: %d\n", ret);
        return ret;
    }
    ret = avcodec_parameters_to_context(in_codec_ctx, in_fmt_ctx->streams[0]->codecpar);
    if (ret != 0) {
        printf("avcodec_parameters_to_context failed: %d\n", ret);
        return ret;
    }
    AVCodec *in_codec = (AVCodec *)avcodec_find_decoder(in_codec_ctx->codec_id);
    if (!in_codec) {
        printf("avcodec_find_decoder failed: %d\n", ret);
        return ret;
    }
    ret = avcodec_open2(in_codec_ctx, in_codec, nullptr);
    if (ret != 0) {
        printf("avcodec_open2 failed: %d\n", ret);
        return 0;
    }

    AVFormatContext *out_fmt_ctx = nullptr;
    ret = avformat_alloc_output_context2(&out_fmt_ctx, nullptr, nullptr, argv[2]);
    if (ret < 0) {
        printf("avformat_alloc_output_context2 failed: %d\n", ret);
        return ret;
    }
    AVStream *stream = avformat_new_stream(out_fmt_ctx, nullptr);
    if (!stream) {
        printf("avformat_new_stream failed: %d\n", ret);
        return ret;
    }

    AVPacket *in_pkt = av_packet_alloc();
    AVFrame *in_frame = av_frame_alloc();

    AVPacket *out_pkt = av_packet_alloc();
    AVFrame *out_frame = av_frame_alloc();

    AVCodecContext *out_codec_ctx = nullptr;

    AVFilterGraph *filter_graph = nullptr;
    AVFilterContext *main_ctx = nullptr;
    AVFilterContext *split_ctx = nullptr;
    AVFilterContext *scale_ctx = nullptr;
    AVFilterContext *overlay_ctx = nullptr;
    AVFilterContext *result_ctx = nullptr;
    
    int end = 0;
    while (!end) {
        ret = av_read_frame(in_fmt_ctx, in_pkt);
        if (ret < 0 && ret != AVERROR_EOF) {
            printf("av_read_frame failed: %d\n", ret);
            return ret;
        }
        if (1 == in_pkt->stream_index) {
            av_packet_unref(in_pkt);
            continue;
        }

        ret = avcodec_send_packet(in_codec_ctx, in_pkt);
        if (ret != 0) {
            printf("avcodec_send_packet failed: %d\n", ret);
            return ret;
        }
        while (!end) {
            ret = avcodec_receive_frame(in_codec_ctx, in_frame);
            if (AVERROR_EOF == ret) {
                end = 1;
            } else if (AVERROR(EAGAIN) == ret) {
                break;
            } else if (ret != 0) {
                printf("avcodec_receive_frame failed: %d\n", ret);
                return ret;
            }
            if (!out_codec_ctx) {
                AVCodec *out_codec = (AVCodec *)avcodec_find_encoder(AV_CODEC_ID_H264);
                if (!out_codec) {
                    printf("avcodec_find_encoder failed: %d\n", ret);
                    return ret;
                }
                out_codec_ctx = avcodec_alloc_context3(out_codec);
                if (!out_codec_ctx) {
                    printf("avcodec_alloc_context3 failed: %d\n", ret);
                    return ret;
                }
                out_codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
                out_codec_ctx->bit_rate = in_codec_ctx->bit_rate;
                out_codec_ctx->framerate = in_codec_ctx->framerate;
                out_codec_ctx->gop_size = in_codec_ctx->gop_size;
                out_codec_ctx->max_b_frames = in_codec_ctx->max_b_frames;
                out_codec_ctx->profile = FF_PROFILE_H264_MAIN;

                out_codec_ctx->time_base = in_fmt_ctx->streams[0]->time_base; // frame->time_base;

                out_codec_ctx->width = in_frame->width;
                out_codec_ctx->height = in_frame->height;
                out_codec_ctx->sample_aspect_ratio = in_frame->sample_aspect_ratio;
                out_codec_ctx->pix_fmt = (AVPixelFormat)in_frame->format;
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

            if (!filter_graph) {

                AVFilterInOut *inputs = nullptr;
                AVFilterInOut *outputs = nullptr;
                AVFilterInOut *current = nullptr;

                filter_graph = avfilter_graph_alloc();
                if (!filter_graph) {
                    printf("avfilter_graph_alloc failed: %d\n", ret);
                    return ret;
                }

                AVRational tb = in_fmt_ctx->streams[0]->time_base;
                AVRational fr = av_guess_frame_rate(in_fmt_ctx, in_fmt_ctx->streams[0], nullptr);
                AVRational sar = in_frame->sample_aspect_ratio;

                AVBPrint main_args;
                av_bprint_init(&main_args, 0, AV_BPRINT_SIZE_AUTOMATIC);
                av_bprintf(&main_args, "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d",
                                in_frame->width, in_frame->height, in_frame->format, tb.num, tb.den, sar.num, sar.den, fr.num, fr.den);
                // 创建buffer滤镜ctx
                ret = avfilter_graph_create_filter(&main_ctx, avfilter_get_by_name("buffer"), "Parsed_buffer_0", main_args.str, nullptr, filter_graph);
                if (ret != 0) {
                    printf("avfilter_graph_create_filter failed: %d\n", ret);
                    return ret;
                }
                // 创建split滤镜ctx
                ret = avfilter_graph_create_filter(&split_ctx, avfilter_get_by_name("split"), "Parsed_split_1", "outputs=2", nullptr, filter_graph);
                if (ret != 0) {
                    printf("avfilter_graph_create_filter failed: %d\n", ret);
                    return ret;
                }
                // 创建scale滤镜ctx
                AVBPrint scale_args;
                av_bprint_init(&scale_args, 0, AV_BPRINT_SIZE_AUTOMATIC);
                av_bprintf(&scale_args, "%d:%d", in_frame->width/2, in_frame->height/2);
                ret = avfilter_graph_create_filter(&scale_ctx, avfilter_get_by_name("scale"), "Parsed_scale_2", scale_args.str, nullptr, filter_graph);
                if (ret != 0) {
                    printf("avfilter_graph_create_filter failed: %d\n", ret);
                    return ret;
                }
                // 创建overlay滤镜ctx
                AVBPrint overlay_args;
                av_bprint_init(&overlay_args, 0, AV_BPRINT_SIZE_AUTOMATIC);
                av_bprintf(&overlay_args, "%d:%d", in_frame->width/4*3, in_frame->height/4*3);
                ret = avfilter_graph_create_filter(&overlay_ctx, avfilter_get_by_name("overlay"), "Parsed_overlay_3", overlay_args.str, nullptr, filter_graph);
                if (ret != 0) {
                    printf("avfilter_graph_create_filter failed: %d\n", ret);
                    return ret;
                }
                // 创建buffersink滤镜ctx
                ret = avfilter_graph_create_filter(&result_ctx, avfilter_get_by_name("buffersink"), "Parsed_buffersink_4", nullptr, nullptr, filter_graph);
                if (ret != 0) {
                    printf("avfilter_graph_create_filter failed: %d\n", ret);
                    return ret;
                }
                // link filter
                ret = avfilter_link(main_ctx, 0, split_ctx, 0);
                if (ret != 0) {
                    printf("avfilter_link failed: %d\n", ret);
                    return ret;
                }
                ret = avfilter_link(split_ctx, 0, scale_ctx, 0);
                if (ret != 0) {
                    printf("avfilter_link failed: %d\n", ret);
                    return ret;
                }
                ret = avfilter_link(scale_ctx, 0, overlay_ctx, 1);
                if (ret != 0) {
                    printf("avfilter_link failed: %d\n", ret);
                    return ret;
                }
                ret = avfilter_link(split_ctx, 1, overlay_ctx, 0);
                if (ret != 0) {
                    printf("avfilter_link failed: %d\n", ret);
                    return ret;
                }
                ret = avfilter_link(overlay_ctx, 0, result_ctx, 0);
                if (ret != 0) {
                    printf("avfilter_link failed: %d\n", ret);
                    return ret;
                }
                ret = avfilter_graph_config(filter_graph, nullptr);
                if (ret != 0) {
                    printf("avfilter_graph_config failed: %d\n", ret);
                    return ret;
                }
            }
            if (!end) {
                ret = av_buffersrc_add_frame_flags(main_ctx, in_frame, AV_BUFFERSRC_FLAG_PUSH);
                if (ret != 0) {
                    printf("av_buffersrc_add_frame_flags failed: %d\n", ret);
                    return ret;
                }

                ret = av_buffersink_get_frame_flags(result_ctx, out_frame, AV_BUFFERSRC_FLAG_PUSH);
                if (ret != 0) {
                    printf("av_buffersink_get_frame_flags failed: %d\n", ret);
                    return ret;
                }
            }

            ret = avcodec_send_frame(out_codec_ctx, out_frame);
            for (;;) {
                ret = avcodec_receive_packet(out_codec_ctx, out_pkt);
                if (AVERROR(EAGAIN) == ret || AVERROR_EOF == ret) {
                    break;
                } else if (ret < 0) {
                    printf("%2d, avcodec_receive_packet failed\n", ret);
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

        }
        av_packet_unref(in_pkt);
    }

    av_write_trailer(out_fmt_ctx);
    
    av_packet_free(&in_pkt);
    av_frame_free(&in_frame);
    av_packet_free(&out_pkt);
    av_frame_free(&out_frame);

    avcodec_close(out_codec_ctx);
    avcodec_close(in_codec_ctx);

    avio_closep(&out_fmt_ctx->pb);
    avformat_free_context(out_fmt_ctx);
    avformat_free_context(in_fmt_ctx);

    avfilter_graph_free(&filter_graph);

    return 0;
}