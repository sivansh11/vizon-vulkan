cmake_minimum_required(VERSION 3.10)

project(engine)

file(GLOB_RECURSE SRC_FILES ./*.cpp)

add_library(engine ${SRC_FILES})

find_package(Vulkan REQUIRED)

find_package(X11 REQUIRED)
if(!X11_XTest_FOUND)
    message(FATAL_ERROR "X11 extensions are required, but not found!")
endif()
if(!X11_Xfixes_LIB)
    message(FATAL_ERROR "X11 fixes extension is required, but not found!")
endif()

target_compile_definitions(engine PUBLIC
    VIZON_PROFILE_ENABLE
)

include_directories(engine
    Vulkan::Headers
    ../deps
    ../deps/imgui
    .
)

target_link_libraries(engine
    glfw
    glm
    stb_image
    spdlog
    volk
    EnTT
    assimp
    screen_capture_lite_static
    imgui
    ${X11_LIBRARIES}
    ${X11_Xfixes_LIB}
    # ${X11_XTest_LIB}
    ${X11_Xinerama_LIB}
)