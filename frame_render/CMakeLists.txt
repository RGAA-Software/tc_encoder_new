﻿set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${GR_PROJECT_PATH}/deps)

aux_source_directory(. FRAME_RENDER_SRCS)

add_library(frame_render ${FRAME_RENDER_SRCS})