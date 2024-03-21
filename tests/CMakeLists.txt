cmake_minimum_required(VERSION 3.26)
#message("=-=-=-=--= ${Boost_LIBRARIES}")
find_package(GTest CONFIG REQUIRED)
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})
find_package(libyuv CONFIG REQUIRED)

add_executable(test_encoder test.cpp)
target_link_libraries(test_encoder PRIVATE tc_common_new tc_encoder_new ${Boost_LIBRARIES} GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)
target_link_libraries(test_encoder PRIVATE ${OpenCV_LIBS} yuv)

add_custom_command(TARGET test_encoder POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/test.png"  "${CMAKE_CURRENT_BINARY_DIR}"
        #COMMAND ${CMAKE_COMMAND} -E copy_directory "${VLC_ROOT}/plugins"  "${CMAKE_BINARY_DIR}/plugins"
)