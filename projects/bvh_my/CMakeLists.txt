
cmake_minimum_required(VERSION 3.10)

project(bvh_my)

file(GLOB_RECURSE SRC_FILES ./*.cpp)

SET(PROJECT_NAME bvh_my)
SET_PROPERTY(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

add_executable(bvh_my ${SRC_FILES})

include_directories(bvh_my
    ../../engine
    .
    ../../deps/imgui
)

target_link_libraries(bvh_my
    engine
)