cmake_minimum_required(VERSION 3.10)

project(x11_test)

file(GLOB_RECURSE SRC_FILES ./*.cpp)

SET(PROJECT_NAME x11_test)
SET_PROPERTY(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/OUTPUT/${PROJECT_NAME}")

add_executable(x11_test ${SRC_FILES})

include_directories(x11_test
    ../../engine
    .
    ../../deps/imgui
)

target_link_libraries(x11_test
    engine
    
)