cmake_minimum_required(VERSION 3.10)

project(imgui)

file(GLOB SRC_FILE imgui/*.cpp)

set(BACKEND_DIR imgui/backends)

set(BACKEND_SRC ${BACKEND_DIR}/imgui_impl_glfw.cpp ${BACKEND_DIR}/imgui_impl_vulkan.cpp)

add_library(imgui
    ${SRC_FILE}
    ${BACKEND_SRC}
)

add_compile_definitions(imgui
    IMGUI_IMPL_VULKAN_NO_PROTOTYPES
)

include_directories(
    PUBLIC ../glfw/include
)

target_include_directories(imgui
    PUBLIC imgui
)