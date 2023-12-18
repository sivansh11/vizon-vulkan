cmake_minimum_required(VERSION 3.10)

project(hiz)

file(GLOB_RECURSE SRC_FILES ./*.cpp)

SET(PROJECT_NAME hiz)
SET_PROPERTY(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

add_executable(hiz ${SRC_FILES})

include_directories(hiz
    ../../engine
    .
    ../../deps/imgui
)

target_link_libraries(hiz
    engine
)