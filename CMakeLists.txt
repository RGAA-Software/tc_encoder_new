cmake_minimum_required(VERSION 3.26)
project(tc_encoder_new)

set(CMAKE_CXX_STANDARD 20)

#find_package(libyuv CONFIG REQUIRED)
#find_package(FFMPEG REQUIRED)

set(FFMPEG_LIB_PATH ${CMAKE_SOURCE_DIR}/deps/tc_3rdparty/ffmpeg/lib/x64)
include_directories(${CMAKE_SOURCE_DIR}/deps/tc_3rdparty/ffmpeg/include)
link_directories(${FFMPEG_LIB_PATH})

set(NVSDK_VERSION
    12
)

include(nvencoder/${NVSDK_VERSION}/CMakeLists.txt)

include_directories(
    nvencoder/${NVSDK_VERSION}
)

set(NVENCODER_SOURCES_LIST)

foreach(item ${NVENCODER_SOURCES})
    #message("Current item: ${item}")
    list(APPEND NVENCODER_SOURCES_LIST "${item}")
endforeach()

set(SRCS
    ${NVENCODER_SOURCES_LIST}
    video_encoder.cpp
    video_encoder_factory.cpp
    ffmpeg_video_encoder.cpp
    nvenc_video_encoder.cpp
)

add_subdirectory(frame_render)
add_subdirectory(legacy)

add_library(tc_encoder_new ${SRCS})
target_link_libraries(tc_encoder_new)#yuv
#target_include_directories(tc_encoder_new PRIVATE ${FFMPEG_INCLUDE_DIRS})
#target_link_directories(tc_encoder_new PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries(tc_encoder_new ${FFMPEG_LIB_PATH}/avcodec.lib ${FFMPEG_LIB_PATH}/avdevice.lib ${FFMPEG_LIB_PATH}/avfilter.lib
        ${FFMPEG_LIB_PATH}/avformat.lib ${FFMPEG_LIB_PATH}/avutil.lib ${FFMPEG_LIB_PATH}/swresample.lib ${FFMPEG_LIB_PATH}/swscale.lib
        ${FFMPEG_LIB_PATH}/postproc.lib tc_opus_codec_new frame_render yuv)#${FFMPEG_LIBRARIES}

#add_subdirectory(tests)
