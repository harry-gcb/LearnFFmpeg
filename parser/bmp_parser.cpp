#include <cstdio>
#include <iostream>

#ifdef __GNUC__
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#endif

PACK(
     struct BmpFileHeader {
     unsigned char FileType[2];
     unsigned int FileSize;
     unsigned short Reserved1;
     unsigned short Reserved2;
     unsigned int PixelDataOffset;
 }
 );

PACK(
struct BitmapHeader {
    unsigned int HeaderSize;
    unsigned int ImageWidth;
    unsigned int ImageHeight;
    unsigned short Planes;
    unsigned short BitsPerPixel;
    unsigned int Compression;
    unsigned int ImageSize;
    int XpixelsPerMeter;
    int YpixelsPerMeter;
    unsigned int TotalColors;
    unsigned int ImportantColors;
}
);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage: ./bmp_parser bmp_filename\n");
        return -1;
    }
    FILE *fp = nullptr;
    fp = fopen(argv[1], "r");
    if (nullptr == fp) {
        printf("open file failed\n");
        return -1;
    }

    BmpFileHeader fileHeader = { 0 };
    size_t n = fread(&fileHeader, sizeof(fileHeader), 1, fp);
    if (0 == n) {
        printf("read file header failed\n");
        return -1;
    }
    printf("File Header:\n\tFileType: %c%c\n\tFileSize: %d\n\tReserved1: %hu\n\tReserved2: %hu\n\tPixelDataOffset: %d\n", 
            fileHeader.FileType[0], fileHeader.FileType[1], fileHeader.FileSize, fileHeader.Reserved1, fileHeader.Reserved2, fileHeader.PixelDataOffset);
    BitmapHeader bmpHeader = { 0 };
    n = fread(&bmpHeader, sizeof(bmpHeader), 1, fp);
    if (0 == n) {
        printf("read bmp header failed\n");
        return -1;
    }
    printf("Bitmap Header:\n\tHeaderSize: %d\n\tImageWidth: %d\n\tImageHeight: %d\n\tPlanes: %hu\n\tBitsPerPixel: %hu\n\tCompression: %d\n\tImageSize: %d\n\tXpixelsPerMeter: %d\n\tYpixelsPerMeter: %d\n\tTotalColors: %d\n\tImportantColors: %d\n", 
            bmpHeader.HeaderSize, bmpHeader.ImageWidth, bmpHeader.ImageHeight, bmpHeader.Planes, bmpHeader.BitsPerPixel, bmpHeader.Compression, bmpHeader.ImageSize,bmpHeader.XpixelsPerMeter, bmpHeader.YpixelsPerMeter, bmpHeader.TotalColors, bmpHeader.ImportantColors);
    fclose(fp);
    return 0;

}