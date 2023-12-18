#include "core/window.hpp"
#include "core/core.hpp"
#include "core/event.hpp"
#include "core/log.hpp"
#include "core/mesh.hpp"
#include "core/material.hpp"
#include "core/model.hpp"

#include "renderer.hpp"

#include <glm/glm.hpp>
#include "glm/gtx/string_cast.hpp"
#include <entt/entt.hpp>

#include "core/imgui_utils.hpp"

#include "editor_camera.hpp"

#include <memory>
#include <iostream>
#include <algorithm>
#include <chrono>



int main(int argc, char **argv) {
    auto window = core::make_ref<core::window_t>("test2", 1200, 800);
    auto context = core::make_ref<gfx::vulkan::context_t>(window, 2, true);
    core::ImGui_init(window, context);

    renderer::init(window, context); 

    editor_camera_t editor_camera{ window };

    std::vector<renderer::draw_data_info_t> draw_data_infos;
    std::vector<core::ref<gfx::vulkan::image_t>> loaded_images;

    auto default_missing_image = gfx::vulkan::image_builder_t{}
        .loadFromPath(context, "../../assets/textures/default.png");
    loaded_images.push_back(default_missing_image);

    auto model = core::load_model_from_path("../../assets/models/Sponza/glTF/Sponza.gltf");
    {
        for (auto mesh : model.meshes) {
            renderer::gpu_mesh_t gpu_mesh{};
            core::ref<gfx::vulkan::buffer_t> staging_buffer;

            staging_buffer = gfx::vulkan::buffer_builder_t{}
                .build(context,
                       mesh.vertices.size() * sizeof(core::vertex_t),
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                       );
            gpu_mesh.vertex_buffer = gfx::vulkan::buffer_builder_t{}
                .build(context,
                       mesh.vertices.size() * sizeof(core::vertex_t),
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                       );
                    
            std::memcpy(staging_buffer->map(), mesh.vertices.data(), mesh.vertices.size() * sizeof(core::vertex_t));
            staging_buffer->unmap();

            gfx::vulkan::buffer_t::copy(context, *staging_buffer, * gpu_mesh.vertex_buffer, VkBufferCopy{
                .size = mesh.vertices.size() * sizeof(core::vertex_t)
            });

            
            
            staging_buffer = gfx::vulkan::buffer_builder_t{}
                .build(context,
                       mesh.indices.size() * sizeof(uint32_t),
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                       );
            gpu_mesh.index_buffer = gfx::vulkan::buffer_builder_t{}
                .build(context,
                       mesh.indices.size() * sizeof(uint32_t),
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                       );
                    
            std::memcpy(staging_buffer->map(), mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));
            staging_buffer->unmap();

            gfx::vulkan::buffer_t::copy(context, *staging_buffer, * gpu_mesh.index_buffer, VkBufferCopy{
                .size = mesh.indices.size() * sizeof(uint32_t)
            });

            gpu_mesh.material_descriptor_set = renderer::get_material_descriptor_set_layout()->new_descriptor_set();
            auto it = std::find_if(mesh.material_description.texture_infos.begin(), mesh.material_description.texture_infos.end(), [](const core::texture_info_t& texture_info) {
                return texture_info.texture_type == core::texture_type_t::e_diffuse_map;
            });
            if (it != mesh.material_description.texture_infos.end()) {
                auto image = gfx::vulkan::image_builder_t{}
                    .loadFromPath(context, it->file_path);
                loaded_images.push_back(image);
                gpu_mesh.material_descriptor_set->write()
                    .pushImageInfo(0, 1, image->descriptor_info(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
                    .update();
            } else {
                gpu_mesh.material_descriptor_set->write()
                    .pushImageInfo(0, 1, default_missing_image->descriptor_info(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
                    .update();
            }


            gpu_mesh.vertex_count = mesh.vertices.size();
            gpu_mesh.index_count = mesh.indices.size();
            
            // material
            renderer::draw_data_info_t draw_data_info;
            draw_data_info.gpu_mesh = gpu_mesh;
            draw_data_info.gpu_mesh.aabb = mesh.aabb;
            draw_data_infos.push_back(draw_data_info);
        }
    }

    // auto imgui_dsl = gfx::vulkan::descriptor_set_layout_builder_t{}
    //     .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
    //     .build(context);
    // auto imgui_ds = imgui_dsl->new_descriptor_set();
    // imgui_ds->write()
    //     .pushImageInfo(0, 1, renderer::get_output_image()->descriptor_info(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
    //     .update();

    // auto test_image = gfx::vulkan::image_builder_t{}
    //     .loadFromPath(context, "../../assets/textures/noise.jpg");
    
    float target_FPS = 1000.f;
    auto last_time = std::chrono::system_clock::now();
    while (!window->should_close()) {
        window->poll_events();
        
        auto current_time = std::chrono::system_clock::now();
        auto time_difference = current_time - last_time;
        if (time_difference.count() / 1e6 < 1000.f / target_FPS) {
            continue;
        }
        last_time = current_time;
        float dt = time_difference.count() / float(1e6);

        editor_camera.update(dt);

        if (auto start_frame = context->start_frame()) {
            auto [commandbuffer, current_index] = *start_frame;

            VkClearValue clear_color{};
            clear_color.color = {0, 0, 0, 0};    
            VkClearValue clear_depth{};
            clear_depth.depthStencil.depth = 1;

            #if 0

            VkViewport swapchain_viewport{};
            swapchain_viewport.x = 0;
            swapchain_viewport.y = 0;
            swapchain_viewport.width = static_cast<float>(context->swapchain_extent().width);
            swapchain_viewport.height = static_cast<float>(context->swapchain_extent().height);
            swapchain_viewport.minDepth = 0;
            swapchain_viewport.maxDepth = 1;
            VkRect2D swapchain_scissor{};
            swapchain_scissor.offset = {0, 0};
            swapchain_scissor.extent = context->swapchain_extent();

            #endif

            // renderer.render(commandbuffer, current_index, editor_camera, draw_data_infos);
            renderer::render(commandbuffer, current_index, editor_camera, draw_data_infos);
            
            context->begin_swapchain_renderpass(commandbuffer, clear_color);
            
            core::ImGui_newframe();

            ImGui::Begin("view");
            // ImGui::Image(reinterpret_cast<ImTextureID>(reinterpret_cast<void *>(imgui_ds->descriptor_set())), ImGui::GetContentRegionAvail(), {0, 1}, {1, 0});
            ImGui::End();

            ImGui::Begin("debug");
            ImGui::Text("%f", ImGui::GetIO().Framerate);
            ImGui::End();

            renderer::imgui_display();

            core::ImGui_endframe(commandbuffer);
            context->end_swapchain_renderpass(commandbuffer);

            context->end_frame(commandbuffer);
        }
        core::clear_frame_function_times();
    }

    context->wait_idle();

    renderer::destroy();

    core::ImGui_shutdown();

    return 0;
}