cmake_minimum_required(VERSION 3.26)
project(tc_encoder)

set(CMAKE_CXX_STANDARD 20)

find_package(libyuv CONFIG REQUIRED)
find_package(FFMPEG REQUIRED)

set(SRCS
        video_encoder.cpp
        video_encoder_factory.cpp
        ffmpeg_video_encoder.cpp
)

add_library(tc_encoder ${SRCS})
target_link_libraries(tc_encoder yuv)
target_include_directories(tc_encoder PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories(tc_encoder PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries(tc_encoder ${FFMPEG_LIBRARIES})

add_subdirectory(tests)
