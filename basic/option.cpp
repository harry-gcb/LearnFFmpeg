#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/avformat.h"

#ifdef __cplusplus
}
#endif

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage: ./a.out filename\n");
        return 0;
    }
    int err = 0;
    AVDictionary *fmt_opts = nullptr;
    AVFormatContext *fmt_ctx = nullptr;
    fmt_ctx = avformat_alloc_context();
    if (!fmt_ctx) {
        av_log(nullptr, AV_LOG_ERROR, "avformat_alloc_context: %d\n", AVERROR(ENOMEM));
        return 0;
    }
    AVDictionaryEntry *entry = nullptr;
    av_dict_set(&fmt_opts, "formatprobesize", "10485760", AV_DICT_MATCH_CASE);
    av_dict_set(&fmt_opts, "export_all", "1", AV_DICT_MATCH_CASE);
    av_dict_set(&fmt_opts, "export_666", "1", AV_DICT_MATCH_CASE);

    // get first item
    if ((entry = av_dict_get(fmt_opts, "", nullptr, AV_DICT_IGNORE_SUFFIX))) {
        av_log(nullptr, AV_LOG_INFO, "key: %s, value: %s\n", entry->key, entry->value);
    }

    if ((err = avformat_open_input(&fmt_ctx, argv[1], nullptr, &fmt_opts)) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "avformat_open_input failed: %d\n", err);
    } else {
        av_log(nullptr, AV_LOG_INFO, "open success\n");
        av_log(nullptr, AV_LOG_INFO, "duration: %lu\n", fmt_ctx->duration);
    }

    // get first item
    if ((entry = av_dict_get(fmt_opts, "", nullptr, AV_DICT_IGNORE_SUFFIX))) {
        av_log(nullptr, AV_LOG_INFO, "key: %s, value: %s\n", entry->key, entry->value);
    }
    av_dict_free(&fmt_opts);
    return 0;
}