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

int main(int argc, char *argv[]) {
    int ret = -1;
    if (argc <= 1) {
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
    // av_dump_format(in_fmt_ctx, 0, argv[1], 0);
    AVCodecContext *in_codec_ctx = avcodec_alloc_context3(nullptr);
    if (!in_codec_ctx) {
        printf("avcodec_alloc_context3 failed\n");
        return ret;
    }
    ret = avcodec_parameters_to_context(in_codec_ctx, in_fmt_ctx->streams[1]->codecpar);
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

    AVFilterGraph *filter_graph = nullptr;
    AVFilterContext *main_ctx = nullptr;
    AVFilterContext *result_ctx = nullptr;

    AVPacket *in_pkt = av_packet_alloc();
    AVFrame *in_frame = av_frame_alloc();
    AVFrame *out_frame = av_frame_alloc();

    int eof = 0;
    int frame_num = 0;
    while (!eof) {
        ret = av_read_frame(in_fmt_ctx, in_pkt);
        if (ret != 0 && ret != AVERROR_EOF) {
            printf("av_read_frame failed: %d\n", ret);
            break;
        }
        if (0 == in_pkt->stream_index) {
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
            printf("[%d] format=%d, sample_fmt=%s, sample_rate=%d, channel_layout=%d, pts=%d, nb_samples=%d, duration=%d\n",
                    frame_num, in_frame->format, av_get_sample_fmt_name((AVSampleFormat)in_frame->format), in_frame->sample_rate, (int)in_frame->channel_layout, (int)in_frame->pts, in_frame->nb_samples, (int)in_frame->pkt_duration);
            if (!filter_graph) {


                AVFilterInOut *inputs = nullptr;
                AVFilterInOut *outputs = nullptr;
                filter_graph = avfilter_graph_alloc();
                if (!filter_graph) {
                    printf("avfilter_graph_alloc failed\n");
                    return ret;
                }
                // 因为 filter 的输入是 AVFrame ，所以 filter 的时间基就是 AVFrame 的时间基
                AVRational tb = in_fmt_ctx->streams[0]->time_base;

                AVBPrint args;
                av_bprint_init(&args, 0, AV_BPRINT_SIZE_AUTOMATIC);
                av_bprintf(&args, "abuffer=sample_rate=%d:sample_fmt=%s:channel_layout=%d:time_base=%d/%d[main];"
                                  "[main]aformat=sample_rates=%d:sample_fmts=%s:channel_layouts=%d[result];"
                                  "[result]abuffersink;",
                                  in_frame->sample_rate, av_get_sample_fmt_name((AVSampleFormat)in_frame->format), in_frame->channel_layout, tb.num, tb.den, 
                                  44100,"s64",AV_CH_FRONT_RIGHT);
                ret = avfilter_graph_parse2(filter_graph, args.str, &inputs, &outputs);
                if (ret != 0) {
                    printf("avfilter_graph_parse2 failed: %d\n", ret);
                    return ret;
                }
                ret = avfilter_graph_config(filter_graph, nullptr);
                if (ret != 0) {
                    printf("avfilter_graph_config failed: %d\n", ret);
                    return ret;
                }
                main_ctx = avfilter_graph_get_filter(filter_graph, "Parsed_abuffer_0");
                result_ctx = avfilter_graph_get_filter(filter_graph, "Parsed_abuffersink_2");

            }
            if (!eof) {
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
                printf("[%d] format=%d, sample_fmt=%s, sample_rate=%d, channel_layout=%lu, pts=%ld, nb_samples=%d, duration=%ld\n",
                       frame_num++, out_frame->format, av_get_sample_fmt_name((AVSampleFormat)out_frame->format), out_frame->sample_rate, out_frame->channel_layout, out_frame->pts, out_frame->nb_samples, out_frame->pkt_duration);
            }
            av_frame_unref(in_frame);
            av_frame_unref(out_frame);
        }
        av_packet_unref(in_pkt);
    }

    
    av_packet_free(&in_pkt);
    av_frame_free(&in_frame);
    av_frame_free(&out_frame);

    avcodec_close(in_codec_ctx);

    avformat_free_context(in_fmt_ctx);

    avfilter_graph_free(&filter_graph);

    return ret;
}
