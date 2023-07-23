cmake_minimum_required(VERSION 3.10)

project(deps)

add_subdirectory(stb_image)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(glfw)
add_subdirectory(glm)
add_subdirectory(spdlog)
add_subdirectory(volk)
add_subdirectory(entt)
add_subdirectory(assimp)
set(BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(BUILD_CSHARP OFF CACHE BOOL "" FORCE)
add_subdirectory(screen_capture_lite)
add_subdirectory(imgui)