#ifndef RENDER_SYSTEM_BASE_HPP
#define RENDER_SYSTEM_BASE_HPP

#include "gfx/vulkan/context.hpp"
#include "gfx/vulkan/pipeline.hpp"
#include "gfx/vulkan/descriptor.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "gfx/vulkan/image.hpp"
#include "gfx/vulkan/renderpass.hpp"
#include "gfx/vulkan/framebuffer.hpp"
#include "gfx/vulkan/timer.hpp"

#include "core/core.hpp"

#include <glm/glm.hpp>

struct camera_uniform_t {
    glm::mat4 projection_view;
    glm::mat4 view;
    glm::mat4 inverse_view;
    glm::mat4 projection;
    glm::mat4 inverse_projection;
};

struct model_uniform_t {
    glm::mat4 model;
    glm::mat4 inverse_model;
};

struct gpu_mesh_data_t {
    core::ref<gfx::vulkan::buffer_t> vertex_buffer;
    core::ref<gfx::vulkan::buffer_t> index_buffer;
    uint32_t index_count;
    core::ref<gfx::vulkan::descriptor_set_t> per_mesh_descriptor_set;  // material and model inv model
};

struct cmd_draw_t {
    gpu_mesh_data_t gpu_mesh_data;
};

#endif