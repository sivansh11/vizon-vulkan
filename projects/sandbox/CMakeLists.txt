cmake_minimum_required(VERSION 3.10)

project(sandbox)

file(GLOB_RECURSE SRC_FILES ./*.cpp)

SET(PROJECT_NAME sandbox)
SET_PROPERTY(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

add_executable(sandbox ${SRC_FILES})

include_directories(sandbox
    ../../engine
    .
    ../../deps/imgui
)

target_link_libraries(sandbox
    engine
)