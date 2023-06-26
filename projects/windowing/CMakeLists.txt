cmake_minimum_required(VERSION 3.10)

project(windowing)

file(GLOB_RECURSE SRC_FILES ./*.cpp)

SET(PROJECT_NAME windowing)
SET_PROPERTY(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

add_executable(windowing ${SRC_FILES})

include_directories(windowing
    ../../engine
    ../../deps/screen_capture_lite/include
    .
)

target_link_libraries(windowing
    engine
)