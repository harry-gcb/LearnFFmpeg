#include <cstdio>
#include <algorithm>
#include <fstream>

#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"

#ifdef __cplusplus
}
#endif

#define AVIO_CTX_BUFFER_SIZE 4096

typedef struct Buffer {
    uint8_t *ptr;
    uint8_t *ori_ptr;
    size_t size;
    size_t ori_size;
} Buffer;


static uint8_t *read_whole_file(const char *filename, size_t *length) {
    int ret = 0;
    int size = 0;
    uint8_t *data = nullptr;
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        printf("read file failed: %s\n", filename);
        return data;
    }
    ret = fseek(fp, 0, SEEK_END);
    if (ret != 0) {
        printf("fseek failed: %d\n", ret);
        return data;
    }
    size = ftell(fp);
    data = (uint8_t *)malloc((size) * sizeof(uint8_t));
    if (!data) {
        printf("malloc failed\n");
        return data;
    }
    ret = fseek(fp, 0, SEEK_SET);
    if (ret != 0) {
        printf("fseek failed: %d\n", ret);
        return data;
    }
    
    *length = fread(data, 1, size, fp);
    fclose(fp);
    return data;
}

static int read_packet(void *opaque, uint8_t *buf, int buf_size) {
    Buffer *buffer = (Buffer *)opaque;
    // printf("buffer: 0x%p, buf: 0x%p, buf_size: %d, size: %u\n", buffer->ptr, buf, buf_size, buffer->size);
    buf_size = (int)std::min((int)buffer->size, buf_size);
    if (!buf_size) {
        printf("no buf_size pass to read_packet: %d, %zd\n", buf_size, buffer->size);
        return EAGAIN;
    }
    
    memcpy(buf, buffer->ptr, buf_size);
    buffer->ptr += buf_size;
    buffer->size -= buf_size;
    return buf_size;
}

static int64_t seek_packet(void *opaque, int64_t offset, int whence) {
    int64_t ret = -1;
    Buffer *buffer = (Buffer *)opaque;
    switch (whence) {
        case AVSEEK_SIZE:
            ret = buffer->ori_size;
            break;
        case SEEK_SET:
            buffer->ptr = buffer->ori_ptr + offset;
            buffer->size = buffer->ori_size - offset;
            ret = (int64_t)buffer->ptr;
            break;
    }
    return ret;
}

int main(int argc, char *argv[]) {
    int ret = 0;
    if (argc < 3) {
       printf("usage: ./a.out argv[1] argv[2]\n");
       return ret;
    }

    AVFormatContext *in_fmt_ctx = avformat_alloc_context();
    if (!in_fmt_ctx) {
        printf("avformat_alloc_context failed\n");
        return ret;
    }

    Buffer buffer = { 0 };
    size_t length = 0;
    uint8_t *input = read_whole_file(argv[1], &length);
    buffer.ptr = input;
    buffer.ori_ptr = input;
    buffer.size = length;
    buffer.ori_size = length;
    uint8_t *avio_ctx_buffer = nullptr;
    int avio_ctx_buffer_size = AVIO_CTX_BUFFER_SIZE;
    avio_ctx_buffer = (uint8_t *)av_malloc(avio_ctx_buffer_size);
    if (!avio_ctx_buffer) {
        printf("av_malloc failed: %d\n", ret);
        return ret;
    }

    AVIOContext *avio_ctx = nullptr;
    avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size, 0, &buffer, &read_packet, nullptr, &seek_packet);
    if (!avio_ctx) {
        printf("avio_alloc_context failed\n");
        return 0;
    }

    in_fmt_ctx->pb = avio_ctx;
    AVInputFormat *iformat = (AVInputFormat *)av_find_input_format("mp4");

    ret = avformat_open_input(&in_fmt_ctx, nullptr, iformat, nullptr);
    if (ret != 0) {
        printf("avformat_open_input failed: %d\n", ret);
        return ret;
    }

    ret = avformat_find_stream_info(in_fmt_ctx, nullptr);
    if (ret < 0) {
        printf("avformat_find_stream_info failed: %d\n",ret);
        return ret;
    }

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
    stream->time_base = in_fmt_ctx->streams[0]->time_base;

    AVCodecContext *out_codec_ctx = nullptr;

    AVPacket *in_pkt = av_packet_alloc();
    AVFrame *in_frame = av_frame_alloc();

    AVPacket *out_pkt = av_packet_alloc();
    AVFrame *out_frame = av_frame_alloc();

    int file_eof = 0;
    int frame_num = 0;
    while (!file_eof) {
        ret = av_read_frame(in_fmt_ctx, in_pkt);
        if (1 == in_pkt->stream_index) {
            av_packet_unref(in_pkt);
            continue;
        }
        if (AVERROR_EOF == ret || 0 == ret) {
            ret = avcodec_send_packet(in_codec_ctx, AVERROR_EOF == ret ? nullptr : in_pkt);
        } else if (ret != 0) {
            printf("av_read_frame failed: %d\n", ret);
            return ret;
        }
        if (ret != 0) {
            printf("avcodec_send_packet failed: %d\n", ret);
        }
        for (;;) {
            ret = avcodec_receive_frame(in_codec_ctx, in_frame);
            if (AVERROR(EAGAIN) == ret) {
                break;
            } else if (AVERROR_EOF == ret) {
                ret = avcodec_send_frame(out_codec_ctx, nullptr);
                for (;;) {
                    ret = avcodec_receive_packet(out_codec_ctx, out_pkt);
                    if (AVERROR(EAGAIN) == ret) {
                        printf("avcodec_receive_frame failed\n");
                        return ret;
                    }
                    if (AVERROR_EOF == ret) {
                        break;
                    }
                    out_pkt->stream_index = stream->index;
                    //转换 AVPacket 的时间基为 输出流的时间基。
                    out_pkt->pts = av_rescale_q_rnd(out_pkt->pts, in_fmt_ctx->streams[0]->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
                    out_pkt->dts = av_rescale_q_rnd(out_pkt->dts, in_fmt_ctx->streams[0]->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
                    out_pkt->duration = av_rescale_q_rnd(out_pkt->duration, in_fmt_ctx->streams[0]->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));

                    ret = av_interleaved_write_frame(out_fmt_ctx, out_pkt);
                    if (ret < 0) {
                        printf("av_interleaved_write_frame faile %d \n",ret);
                        return ret;
                    }
                    av_packet_unref(out_pkt);
                }
                av_write_trailer(out_fmt_ctx);
                file_eof = 1;
                break;
            } else if (ret >= 0) {
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
                
                    //编码器的时间基要取 AVFrame 的时间基，因为 AVFrame 是输入。AVFrame 的时间基就是 流的时间基。
                    out_codec_ctx->time_base = in_fmt_ctx->streams[0]->time_base;
                    out_codec_ctx->width = in_fmt_ctx->streams[0]->codecpar->width;
                    out_codec_ctx->height = in_fmt_ctx->streams[0]->codecpar->height;
                    out_codec_ctx->sample_aspect_ratio = stream->sample_aspect_ratio = in_frame->sample_aspect_ratio;
                    out_codec_ctx->pix_fmt = (AVPixelFormat)in_frame->format;
                    out_codec_ctx->color_range            = in_frame->color_range;
                    out_codec_ctx->color_primaries        = in_frame->color_primaries;
                    out_codec_ctx->color_trc              = in_frame->color_trc;
                    out_codec_ctx->colorspace             = in_frame->colorspace;
                    out_codec_ctx->chroma_sample_location = in_frame->chroma_location;

                    out_codec_ctx->field_order = AV_FIELD_PROGRESSIVE;
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
                ret = avcodec_send_frame(out_codec_ctx, in_frame);
                if (ret != 0) {
                    printf("avcodec_send_frame failed: %d\n", ret);
                    return ret;
                }
                for (;;) {
                    ret = avcodec_receive_packet(out_codec_ctx, out_pkt);
                    if (AVERROR(EAGAIN) == ret) {
                        break;
                    }
                    if (ret != 0) {
                        printf("avcodec_receive_packet failed: %d\n", ret);
                        return ret;
                    }
                    out_pkt->stream_index = stream->index;
                    //转换 AVPacket 的时间基为 输出流的时间基。
                    out_pkt->pts = av_rescale_q_rnd(out_pkt->pts, in_fmt_ctx->streams[0]->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
                    out_pkt->dts = av_rescale_q_rnd(out_pkt->dts, in_fmt_ctx->streams[0]->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
                    out_pkt->duration = av_rescale_q_rnd(out_pkt->duration, in_fmt_ctx->streams[0]->time_base, stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));

                    ret = av_interleaved_write_frame(out_fmt_ctx, out_pkt);
                    if (ret < 0) {
                        printf("av_interleaved_write_frame faile %d \n",ret);
                        return ret;
                    }
                    av_packet_unref(out_pkt);
                }
            } else {
                printf("avcodec_receive_frame failed: %d\n", ret);
                return ret;
            }
        }
        av_packet_unref(in_pkt);
    }

    av_free(avio_ctx_buffer);
    avio_context_free(&avio_ctx);
    av_frame_free(&in_frame);
    av_frame_free(&out_frame);
    av_packet_free(&in_pkt);
    av_packet_free(&out_pkt);

    avcodec_close(in_codec_ctx);
    avcodec_close(out_codec_ctx);

    avformat_free_context(in_fmt_ctx);
    avformat_free_context(out_fmt_ctx);

    printf("done\n");
    return ret;
} 
