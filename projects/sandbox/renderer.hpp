#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "gfx/vulkan/context.hpp"
#include "gfx/vulkan/pipeline.hpp"
#include "gfx/vulkan/descriptor.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "gfx/vulkan/image.hpp"
#include "gfx/vulkan/renderpass.hpp"
#include "gfx/vulkan/framebuffer.hpp"
#include "gfx/vulkan/timer.hpp"

#include "core/model.hpp"

#include "editor_camera.hpp"

#include <glm/glm.hpp>


namespace renderer {

    struct gpu_mesh_t {
        core::ref<gfx::vulkan::buffer_t> vertex_buffer;
        core::ref<gfx::vulkan::buffer_t> index_buffer;
        uint32_t index_count;
        uint32_t vertex_count;
        // ideally seperate descriptor ser for model & inv model matrices etc and for material so u can sort materials and reduce bindings
        // but here no model and inv model, only mat with diffuse
        core::ref<gfx::vulkan::descriptor_set_t> material_descriptor_set;  
        core::aabb_t aabb;
    };

    struct draw_data_info_t {
        gpu_mesh_t gpu_mesh;
    };

    void init(core::ref<core::window_t> window, core::ref<gfx::vulkan::context_t> context);
    void destroy();

    core::ref<gfx::vulkan::descriptor_set_layout_t> get_material_descriptor_set_layout();

    core::ref<gfx::vulkan::image_t> render_depth(VkCommandBuffer commandbuffer, uint32_t current_index, const editor_camera_t& editor_camera, const std::vector<draw_data_info_t> draw_data_infos);
    
    core::ref<gfx::vulkan::image_t> voxelize_scene(VkCommandBuffer commandbuffer, uint32_t current_index, const editor_camera_t& editor_camera, const std::vector<draw_data_info_t> draw_data_infos);

    core::ref<gfx::vulkan::image_t> voxel_visualization(VkCommandBuffer commandbuffer, uint32_t current_index, const editor_camera_t& editor_camera);

    void imgui_display();

    // core::ref<gfx::vulkan::image_t> get_output_image();

    void render(VkCommandBuffer commandbuffer, uint32_t current_index, const editor_camera_t& editor_camera, const std::vector<draw_data_info_t> draw_data_infos);

} // namespace renderer

#endif