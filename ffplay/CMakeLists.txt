cmake_minimum_required(VERSION "3.22")

project(ffplay)

aux_source_directory(${CMAKE_SOURCE_DIR} SRC_LIST)

include_directories($ENV{LIB_3RDPARTY_PATH}/ffmpeg/include)
include_directories($ENV{LIB_3RDPARTY_PATH}/SDL/include/SDL2)

link_directories($ENV{LIB_3RDPARTY_PATH}/ffmpeg/lib)
link_directories($ENV{LIB_3RDPARTY_PATH}/SDL/lib)

add_executable(ffplay ${SRC_LIST})

target_link_libraries(ffplay SDL2 avformat avcodec avdevice avfilter avutil swresample swscale lzma z m)