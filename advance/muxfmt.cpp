#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/avformat.h"

#ifdef __cplusplus
}
#endif

int main(int argc, char *argv[]) {
    void *ifmt_opaque = nullptr;
    const AVInputFormat *ifmt = nullptr;
    while ((ifmt = av_demuxer_iterate(&ifmt_opaque))) {
        printf("ifmt name: %s\n", ifmt->name);
    }

    void *ofmt_opaque = nullptr;
    const AVOutputFormat *ofmt = nullptr;
    while ((ofmt = av_muxer_iterate(&ofmt_opaque))) {
        printf("ofmt name: %s\n", ofmt->name);
    }
    return 0;
}