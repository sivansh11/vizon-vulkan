cmake_minimum_required(VERSION 3.10)

project(hybrid)

file(GLOB_RECURSE SRC_FILES ./*.cpp)

SET(PROJECT_NAME hybrid)
SET_PROPERTY(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

add_executable(hybrid ${SRC_FILES})

include_directories(hybrid
    ../../engine
    .
    ../../deps/imgui
)

target_link_libraries(hybrid
    engine
)