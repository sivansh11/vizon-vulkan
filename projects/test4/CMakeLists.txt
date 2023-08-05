cmake_minimum_required(VERSION 3.10)

project(test4)

file(GLOB_RECURSE SRC_FILES ./*.cpp)

SET(PROJECT_NAME test4)
SET_PROPERTY(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

add_executable(test4 ${SRC_FILES})

include_directories(test4
    ../../engine
    .
    ../../deps/imgui
)

target_link_libraries(test4
    engine
)