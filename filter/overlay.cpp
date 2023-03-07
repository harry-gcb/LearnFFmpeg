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

static AVFrame *get_frame_from_image(const char *filename, AVRational *logo_tb, AVRational *logo_fr);

int main(int argc, char *argv[]) {
    int ret = -1;
    if (argc <= 3) {
        printf("usage: ./a.out infile outfile logofile\n");
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
    AVFilterGraph *filter_graph = nullptr;
    AVFilterContext *main_ctx = nullptr;
    AVFilterContext *logo_ctx = nullptr;
    AVFilterContext *result_ctx = nullptr;

    AVPacket *in_pkt = av_packet_alloc();
    AVFrame *in_frame = av_frame_alloc();
    AVPacket *out_pkt = av_packet_alloc();
    AVFrame *out_frame = av_frame_alloc();

    AVRational logo_tb = { 0 };
    AVRational logo_fr = { 0 };
    AVFrame *logo_frame = get_frame_from_image(argv[3], &logo_tb, &logo_fr);
    if (!logo_frame) {
        printf("get_frame_from_image failed\n");
        return ret;
    }

    int64_t logo_next_pts = 0;
    int64_t main_next_pts = 0;

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
                filter_graph = avfilter_graph_alloc();
                if (!filter_graph) {
                    printf("avfilter_graph_alloc failed\n");
                    return ret;
                }
                // 因为 filter 的输入是 AVFrame ，所以 filter 的时间基就是 AVFrame 的时间基
                AVRational tb = in_fmt_ctx->streams[0]->time_base;
                AVRational fr = av_guess_frame_rate(in_fmt_ctx, in_fmt_ctx->streams[0], nullptr);
                AVRational sar = in_frame->sample_aspect_ratio;
                AVRational logo_sar = logo_frame->sample_aspect_ratio;

                AVBPrint args;
                av_bprint_init(&args, 0, AV_BPRINT_SIZE_AUTOMATIC);
                av_bprintf(&args, "buffer=video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d[main];"
                                  "buffer=video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d[logo];"
                                  "[main][logo]overlay=x=10:y=10[result];"
                                  "[result]format=yuv420p[result_2];"
                                  "[result_2]buffersink;",
                                  in_frame->width, in_frame->height, in_frame->format, tb.num, tb.den, sar.num, sar.den, fr.num, fr.den, 
                                  logo_frame->width, logo_frame->height, logo_frame->format, logo_tb.num, logo_tb.den, logo_sar.num, logo_sar.den, logo_fr.num, logo_fr.den);
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
                main_ctx = avfilter_graph_get_filter(filter_graph, "Parsed_buffer_0");
                logo_ctx = avfilter_graph_get_filter(filter_graph, "Parsed_buffer_1");
                result_ctx = avfilter_graph_get_filter(filter_graph, "Parsed_buffersink_4");

                ret = av_buffersrc_add_frame_flags(logo_ctx, logo_frame, AV_BUFFERSRC_FLAG_PUSH);
                if (ret != 0) {
                    printf("av_buffersrc_add_frame_flags failed: %d\n", ret);
                    return ret;
                }
                logo_next_pts = logo_frame->pts + logo_frame->pkt_duration;
                ret = av_buffersrc_close(logo_ctx, logo_next_pts, AV_BUFFERSRC_FLAG_PUSH);
                if (ret != 0) {
                    printf("av_buffersrc_close failed: %d\n", ret);
                    return ret;
                }
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

    av_frame_free(&logo_frame);

    avcodec_close(out_codec_ctx);
    avcodec_close(in_codec_ctx);

    avio_closep(&out_fmt_ctx->pb);
    avformat_free_context(out_fmt_ctx);
    avformat_free_context(in_fmt_ctx);

    avfilter_graph_free(&filter_graph);

    return ret;
}

static AVFrame *get_frame_from_image(const char *filename, AVRational *logo_tb, AVRational *logo_fr) {
    int ret = 0;
    AVDictionary *fmt_opts = nullptr;
    av_dict_set(&fmt_opts, "probesize", "5000000", 0);
    AVFormatContext *fmt_ctx = nullptr;
    ret = avformat_open_input(&fmt_ctx, filename, nullptr, &fmt_opts);
    if (ret != 0) {
        printf("avformat_open_input failed: %d\n", ret);
        return nullptr;
    }
    avformat_find_stream_info(fmt_ctx, nullptr);
    *logo_tb = fmt_ctx->streams[0]->time_base;
    *logo_fr = av_guess_frame_rate(fmt_ctx, fmt_ctx->streams[0], nullptr);

    AVCodecContext *codec_ctx = avcodec_alloc_context3(nullptr);
    if (!codec_ctx) {
        printf("avcodec_alloc_context3 failed\n");
        return nullptr;
    }
    ret = avcodec_parameters_to_context(codec_ctx, fmt_ctx->streams[0]->codecpar);
    if (ret != 0) {
        printf("avcodec_parameters_to_context failed: %d\n", ret);
        return nullptr;
    }

    AVDictionary *codec_opts = nullptr;
    av_dict_set(&codec_opts, "sub_text_format", "ass", 0);
    AVCodec *codec = (AVCodec *)avcodec_find_decoder(codec_ctx->codec_id);
    if (!codec) {
        printf("avcodec_find_decoder failed\n");
        return nullptr;
    }

    ret = avcodec_open2(codec_ctx, codec, &codec_opts);
    if (ret != 0) {
        printf("avcodec_open2 failed: %d\n", ret);
        return nullptr;
    }

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = nullptr;
    pkt.size = 0;
    ret = av_read_frame(fmt_ctx, &pkt);
    if (ret != 0) {
        printf("av_read_frame failed: %d\n", ret);
        return nullptr;
    }
    ret = avcodec_send_packet(codec_ctx, &pkt);
    if (ret != 0) {
        printf("avcodec_send_packet failed: %d\n", ret);
        return nullptr;
    }
    AVFrame *frame = av_frame_alloc();
    ret = avcodec_receive_frame(codec_ctx, frame);
    if (ret != 0) {
        printf("avcodec_receive_frame failed: %d\n", ret);
        return nullptr;
    }
    av_dict_free(&fmt_opts);
    av_dict_free(&codec_opts);
    return frame;
}