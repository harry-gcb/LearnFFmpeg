#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"

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

    AVPacket *in_pkt = av_packet_alloc();
    AVFrame *in_frame = av_frame_alloc();
    AVFrame *out_frame = av_frame_alloc();

    const char* format_name = NULL;
    struct SwrContext *swr_ctx = NULL;
    uint8_t *out = NULL;
    int out_count;
    int out_size;
    int out_nb_samples;
    int tgt_fmt = AV_SAMPLE_FMT_S64;
    int tgt_freq = 44100;
    int tgt_channels;

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
            if (!frame_num) {
                printf("[%d] format=%d, sample_fmt=%s, sample_rate=%d, channel_layout=%d, pts=%d, nb_samples=%d, duration=%d\n",
                        frame_num, in_frame->format, av_get_sample_fmt_name((AVSampleFormat)in_frame->format), in_frame->sample_rate, (int)in_frame->channel_layout, (int)in_frame->pts, in_frame->nb_samples, (int)in_frame->pkt_duration);
                format_name = av_get_sample_fmt_name((AVSampleFormat)in_frame->format);
                printf("in_format:%s, sample_rate:%d .\n",format_name, in_frame->sample_rate); 
                format_name = av_get_sample_fmt_name((AVSampleFormat)tgt_fmt);
                printf("to_format:%s, sample_rate:%d .\n",format_name, tgt_freq);
            }
            if (!swr_ctx) {
                swr_ctx = swr_alloc_set_opts(swr_ctx, in_frame->channel_layout, (AVSampleFormat)tgt_fmt, tgt_freq,
                                             in_frame->channel_layout, (AVSampleFormat)in_frame->format, in_frame->sample_rate, 0, nullptr);
                if (!swr_ctx) {
                    printf("swr_alloc_set_opts failed\n");
                    return ret;
                }
                ret = swr_init(swr_ctx);
                if (ret != 0) {
                    printf("swr_init failed: %d\n", ret);
                    return 0;
                }
            }
            // 不改变声道布局
            tgt_channels = in_frame->channels;
            out_count = (int64_t)in_frame->nb_samples * tgt_freq / in_frame->sample_rate + 256;
            out_size  = av_samples_get_buffer_size(NULL, tgt_channels, out_count, (AVSampleFormat)tgt_fmt, 0);
            out = (uint8_t *)av_malloc(out_size);
            //注意，因为 音频可能有超过 9 声道的数据，所以要用 extended_data;
            const uint8_t **in = (const uint8_t **)in_frame->extended_data;
            out_nb_samples = swr_convert(swr_ctx, &out, out_count, in, in_frame->nb_samples);
            if( out_nb_samples < 0 ){
                av_log(NULL, AV_LOG_ERROR, "converte fail \n");
            }else{
                printf("out_count:%d, out_nb_samples:%d, frame->nb_samples:%d \n", out_count, out_nb_samples, in_frame->nb_samples);
            }
            //可以把 out 的内存直接丢给播放器播放。
            av_freep(&out);

            frame_num++;
            if( frame_num > 10 ){
                out = (uint8_t *)av_malloc(out_size);
                do {
                    //把剩下的样本数都刷出来。
                    out_nb_samples = swr_convert(swr_ctx, &out, out_count, NULL, 0);
                    printf("flush out_count:%d, out_nb_samples:%d  \n", out_count, out_nb_samples);
                }
                while(out_nb_samples);
                av_freep(&out);

                return 99;
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

    // avfilter_graph_free(&filter_graph);

    return ret;
}
