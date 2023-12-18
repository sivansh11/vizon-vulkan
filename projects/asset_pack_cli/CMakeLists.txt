cmake_minimum_required(VERSION 3.10)

project(asset_pack_cli)

file(GLOB_RECURSE SRC_FILES ./*.cpp)

SET(PROJECT_NAME asset_pack_cli)
SET_PROPERTY(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

add_executable(asset_pack_cli ${SRC_FILES})

include_directories(asset_pack_cli
    ../../engine
    .
    ../../deps/imgui
)

target_link_libraries(asset_pack_cli
    engine
)