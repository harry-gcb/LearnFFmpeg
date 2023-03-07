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

#define return_errret(val, message) \
    if ((val)) {  \
        printf("%2d, %s\n", ret, message);  \
        return ret; \
    }

#define print_errret(val, message) \
    if ((val)) {  \
        printf("%2d, %s\n", ret, message);  \
    }

int main(int argc, char *argv[]) {
    int ret = -1;
    return_errret(argc < 3, "usage: ./a.out infile outfile");

    AVFormatContext *in_fmt_ctx = avformat_alloc_context();
    return_errret(!in_fmt_ctx, "avformat_alloc_context failed");

    ret = avformat_open_input(&in_fmt_ctx, argv[1], nullptr, nullptr);
    return_errret(ret < 0, "avformat_alloc_context failed");

    AVCodecContext *in_codec_ctx = avcodec_alloc_context3(nullptr);
    return_errret(!in_codec_ctx, "avcodec_alloc_context3 failed");

    ret = avcodec_parameters_to_context(in_codec_ctx, in_fmt_ctx->streams[0]->codecpar);
    return_errret(ret != 0, "avcodec_parameters_to_context failed");

    AVCodec *in_codec = (AVCodec *)avcodec_find_decoder(in_codec_ctx->codec_id);
    return_errret(!in_codec, "avcodec_find_decoder failed");

    ret = avcodec_open2(in_codec_ctx, in_codec, nullptr);
    return_errret(ret != 0, "avcodec_open2 failed");

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

    AVFilterGraph *filter_graph = nullptr;
    AVFilterContext *main_ctx = nullptr;
    AVFilterContext *scale_ctx = nullptr;
    AVFilterContext *result_ctx = nullptr;

    AVCodecContext *out_codec_ctx = nullptr;

    AVFrame *in_frame = av_frame_alloc();
    AVFrame *out_frame = av_frame_alloc();
    AVPacket *in_pkt = av_packet_alloc();
    AVPacket *out_pkt = av_packet_alloc();
   
    int eof = 0;
    while (!eof) {
        ret = av_read_frame(in_fmt_ctx, in_pkt);
        return_errret((ret != 0 && ret != AVERROR_EOF), "av_read_frame failed");

        if (1 == in_pkt->stream_index) {
            av_packet_unref(in_pkt);
            continue;
        }

        ret = avcodec_send_packet(in_codec_ctx, in_pkt);
        print_errret(ret != 0, "avcodec_send_packet in_pkt failed");

        while (!eof) {
            ret = avcodec_receive_frame(in_codec_ctx, in_frame);
            if (AVERROR(EAGAIN) == ret) {
                break;
            } else if (AVERROR_EOF == ret) {
                printf("EOF\n");
                eof = 1;
            } else if (ret < 0) {
                printf("%d, avcodec_receive_frame failed", ret);
                return ret;
            }
            if (!out_codec_ctx) {
                AVCodec *encoder = (AVCodec *)avcodec_find_encoder(AV_CODEC_ID_H264);
                out_codec_ctx = avcodec_alloc_context3(encoder);
                out_codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
                out_codec_ctx->bit_rate = 400000;
                out_codec_ctx->framerate = in_codec_ctx->framerate;
                out_codec_ctx->gop_size = 10;
                out_codec_ctx->max_b_frames = 5;
                out_codec_ctx->profile = FF_PROFILE_H264_MAIN;

                out_codec_ctx->time_base = in_fmt_ctx->streams[0]->time_base; // frame->time_base;
                out_codec_ctx->width = in_frame->width / 2;
                out_codec_ctx->height = in_frame->height / 2;
                out_codec_ctx->sample_aspect_ratio = in_frame->sample_aspect_ratio;
                out_codec_ctx->pix_fmt = (AVPixelFormat)in_frame->format;
                out_codec_ctx->color_range = in_frame->color_range;
                out_codec_ctx->color_primaries = in_frame->color_primaries;
                out_codec_ctx->color_trc = in_frame->color_trc;
                out_codec_ctx->colorspace = in_frame->colorspace;
                out_codec_ctx->chroma_sample_location = in_frame->chroma_location;

                out_codec_ctx->field_order = AV_FIELD_PROGRESSIVE;

                ret = avcodec_parameters_from_context(stream->codecpar, out_codec_ctx);
                if (ret < 0) {
                    printf("avcodec_parameters_from_context failed: %d\n", ret);
                    return ret;
                }

                ret = avcodec_open2(out_codec_ctx, encoder, nullptr);
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
            if (!filter_graph) {
                // 初始化滤镜
                filter_graph = avfilter_graph_alloc();
                return_errret(!filter_graph, "avfilter_graph_alloc failed");

                // 因为 filter 的输入是 AVFrame ，所以 filter 的时间基就是 AVFrame 的时间基
                AVRational tb = in_fmt_ctx->streams[0]->time_base;
                AVRational fr = av_guess_frame_rate(in_fmt_ctx, in_fmt_ctx->streams[0], NULL);
                AVRational sar = in_frame->sample_aspect_ratio;

                // 创建buffer滤镜ctx
                AVBPrint main_args;
                av_bprint_init(&main_args, 0, AV_BPRINT_SIZE_AUTOMATIC);
                av_bprintf(&main_args, "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d",
                                in_frame->width, in_frame->height, in_frame->format, tb.num, tb.den, sar.num, sar.den, fr.num, fr.den);
                ret = avfilter_graph_create_filter(&main_ctx, avfilter_get_by_name("buffer"), "Parsed_buffer_0_666", main_args.str,nullptr, filter_graph);
                return_errret(ret != 0, "avfilter_graph_create_filter failed\n");
                
                // 创建scale滤镜ctx
                AVBPrint scale_args;
                av_bprint_init(&scale_args, 0, AV_BPRINT_SIZE_AUTOMATIC);
                av_bprintf(&scale_args, "%d:%d", in_frame->width/2, in_frame->height/2);
                ret = avfilter_graph_create_filter(&scale_ctx, avfilter_get_by_name("scale"), "Parsed_scale_1_777", scale_args.str, nullptr, filter_graph);
                return_errret(ret != 0, "avfilter_graph_create_filter failed");

                ret = avfilter_graph_create_filter(&result_ctx, avfilter_get_by_name("buffersink"), "Parsed_buffer_2_888", nullptr, nullptr, filter_graph);
                return_errret(ret != 0, "avfilter_graph_create_filter failed");

                ret = avfilter_link(main_ctx, 0, scale_ctx, 0);
                return_errret(ret != 0, "avfilter_link failed");
                ret = avfilter_link(scale_ctx, 0, result_ctx, 0);
                return_errret(ret != 0, "avfilter_link failed");

                ret =avfilter_graph_config(filter_graph, nullptr);
                return_errret(ret != 0, "return_errret failed");
            }
            if (!eof) {
                ret = av_buffersrc_add_frame_flags(main_ctx, in_frame, AV_BUFFERSRC_FLAG_PUSH);
                return_errret(ret != 0, "av_buffersrc_add_frame_flags failed");

                ret = av_buffersink_get_frame_flags(result_ctx, out_frame, AV_BUFFERSRC_FLAG_PUSH);
                return_errret(ret != 0, "av_buffersink_get_frame_flags failed");
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
                return_errret(ret != 0, "av_interleaved_write_frame failed");

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

    printf("done\n");

    return ret;
}