cmake_minimum_required(VERSION 3.26)
project(tc_encoder)

set(CMAKE_CXX_STANDARD 20)

set(SRCS
    TCEncoder.cpp
)

add_library(tc_encoder ${SRCS})
