#include <cstdio>
#include <string>

struct FlvHeader {
    std::string Signature;
    unsigned char Version;
    unsigned char Flags;
    unsigned int DataOffset;
};

struct TagHeader {
    unsigned char TagType;
    unsigned int DataSize;
    unsigned int Timestamp;
    unsigned int StreamID;
};

struct AudioHeader {
    unsigned int SoundFormat;
    unsigned int SoundRate;
    unsigned int SoundSize;
    unsigned int SoundType;
};

struct VideoHeader {
    unsigned int FrameType;
    unsigned int CodecID;
};

#define FLV_HEADER_SIZE 9
#define PREV_TAG_SIZE 4
#define TAG_HEADER_SIZE 11
#define DATA_HEADER_SIZE 1

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage: ./a.out filename\n");
        return -1;
    }
    
    FILE *fp = nullptr;
    fp = fopen(argv[1], "r");
    if (nullptr == fp) {
        printf("open file failed\n");
        return -1;
    }
    unsigned char flv[FLV_HEADER_SIZE] = { 0 };
    int n = fread(flv, FLV_HEADER_SIZE, 1, fp);
    if (0 == n) {
        printf("read flv header failed\n");
        return -1;
    }
    FlvHeader flvHeader;
    flvHeader.Signature = std::string((const char *)flv, 3);
    flvHeader.Version = flv[3];
    flvHeader.Flags = flv[4];
    flvHeader.DataOffset = flv[5] << 24 | flv[6] << 16 | flv[7] << 8 | flv[8];
    printf("Flv Header: \n\tSignature: %s\n\tVersion: %x\n\tFlags: %x\n\tDataOffset: %d\n", 
            flvHeader.Signature.c_str(), flvHeader.Version, flvHeader.Flags, flvHeader.DataOffset);

    unsigned char prevTagSize[PREV_TAG_SIZE] = { 0 };
    n = fread(prevTagSize, PREV_TAG_SIZE, 1, fp);
    if (0 == n) {
        printf("read previouse tag size failed\n");
        return -1;
    }
    int tag_index = 0;
    unsigned int PreviousTagSize = 0;
    PreviousTagSize = prevTagSize[0] << 24 | prevTagSize[1] << 16 | prevTagSize[2] << 8 | prevTagSize[3];
    printf("index: %u, PreviousTagSize %u\n", tag_index++, PreviousTagSize);
    
    while (1) {
        unsigned char tag[TAG_HEADER_SIZE] = { 0 };
        n = fread(tag, sizeof(tag), 1, fp);
        if (0 == n) {
            printf("read tag failed\n");
            break;
        }
        TagHeader tagHeader;
        tagHeader.TagType = tag[0];
        tagHeader.DataSize = tag[1] << 16 | tag[2] << 8 | tag[3];
        tagHeader.Timestamp = tag[7] << 24 | tag[4] << 16 | tag[5] << 8 | tag[6];
        tagHeader.StreamID = tag[8] << 16 | tag[9] << 8 | tag[10];
        printf("index: %u, TagType: %x, DataSize: %d, Timestamp: %d, StreamID: %d, ", tag_index++, tagHeader.TagType, tagHeader.DataSize, tagHeader.Timestamp, tagHeader.StreamID);

        unsigned char data[DATA_HEADER_SIZE] = { 0 };
        n = fread(data, DATA_HEADER_SIZE, 1, fp);
        if (0 == n) {
            printf("read data failed\n");
            break;
        }
        if ((unsigned char)8 == tagHeader.TagType) { 
            AudioHeader audio;
            audio.SoundFormat = (data[0] & 0xf0) >> 4;
            audio.SoundRate = (data[0] & 0x0c) >> 2;
            audio.SoundSize = (data[0] & 0x02) >> 1;
            audio.SoundType = data[0] & 0x01;
            printf("SoundFormat: %u , SoundRate: %u, SoundSize: %u, SoundType: %u, ",  
                     audio.SoundFormat, audio.SoundRate, audio.SoundSize, audio.SoundType);
        } else if ((unsigned int)9 == tagHeader.TagType) {
            VideoHeader video;
            video.FrameType = (data[0] & 0xf0) >> 4;
            video.CodecID = (data[0] & 0x0f);
            printf("FrameType: %u , CodecID: %u, ",  
                     video.FrameType, video.CodecID);
        } else if ((unsigned int)18 == tagHeader.TagType) {

        } else {
            printf("unknown data\n");
            break;
        }

        n = fseek(fp, tagHeader.DataSize - DATA_HEADER_SIZE, SEEK_CUR);
        if (n < 0) {
            printf("seek file failed: %d\n", n);
            return -1;
        }


        unsigned char prevTagSize[PREV_TAG_SIZE] = { 0 };
        n = fread(prevTagSize, PREV_TAG_SIZE, 1, fp);
        if (0 == n) {
            printf("read previouse tag size failed\n");
            return -1;
        }
        unsigned int PreviousTagSize = 0;
        PreviousTagSize = prevTagSize[0] << 24 | prevTagSize[1] << 16 | prevTagSize[2] << 8 | prevTagSize[3];
        printf("PreviousTagSize %u\n", PreviousTagSize);
    }
        
    
    fclose(fp);
    return 0;

}