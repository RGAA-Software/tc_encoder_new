cmake_minimum_required(VERSION 3.26)
project(tc_encoder)

set(CMAKE_CXX_STANDARD 20)

find_package(libyuv CONFIG REQUIRED)
find_package(FFMPEG REQUIRED)

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

message(----> ${FFMPEG_LIBRARIES})

add_library(tc_encoder ${SRCS})
target_link_libraries(tc_encoder yuv)
target_include_directories(tc_encoder PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories(tc_encoder PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries(tc_encoder ${FFMPEG_LIBRARIES} tc_opus_codec)

add_subdirectory(tests)
