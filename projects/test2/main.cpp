#include "core/window.hpp"
#include "core/core.hpp"
#include "core/event.hpp"
#include "core/log.hpp"
#include "core/mesh.hpp"
#include "core/material.hpp"
#include "core/model.hpp"

#include "gfx/vulkan/context.hpp"
#include "gfx/vulkan/pipeline.hpp"
#include "gfx/vulkan/descriptor.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "gfx/vulkan/image.hpp"
#include "gfx/vulkan/renderpass.hpp"
#include "gfx/vulkan/framebuffer.hpp"
#include "gfx/vulkan/timer.hpp"

#include <glm/glm.hpp>
#include "glm/gtx/string_cast.hpp"
#include <entt/entt.hpp>

#include "core/imgui_utils.hpp"

#include "editor_camera.hpp"

#include <memory>
#include <iostream>
#include <algorithm>
#include <chrono>

struct gpu_mesh_t {
    core::ref<gfx::vulkan::buffer_t> _vertex_buffer;
    core::ref<gfx::vulkan::buffer_t> _index_buffer;
    core::ref<gfx::vulkan::descriptor_set_t> _material_descriptor_set;
    uint32_t _index_count;
};

struct set_0_t {
    glm::mat4 view;
    glm::mat4 inv_view;
    glm::mat4 projection;
    glm::mat4 inv_projection;
};

struct set_1_t {
    glm::mat4 model;
    glm::mat4 inv_model;
};

int main(int argc, char **argv) {
    auto window = core::make_ref<core::Window>("test2", 1200, 800);
    auto ctx = core::make_ref<gfx::vulkan::context_t>(window, 2, true, true);

    core::ImGui_init(window, ctx);

    std::vector<gpu_mesh_t> gpu_meshes;
    std::vector<core::ref<gfx::vulkan::image_t>> loaded_images;

    auto material_descriptor_set_layout = gfx::vulkan::descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(ctx);

    auto [width, height] = window->getSize();

    auto renderpass = gfx::vulkan::renderpass_builder_t{}
        .add_color_attachment(VkAttachmentDescription{
            .format = VK_FORMAT_R8G8B8A8_SRGB,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
        })
        .set_depth_attachment(VkAttachmentDescription{
            .format = VK_FORMAT_D32_SFLOAT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,                  // no prepass
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
        })
        .build(ctx);
    auto color_image = gfx::vulkan::image_builder_t{}
        .build2D(ctx, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    auto depth_image = gfx::vulkan::image_builder_t{}
        .build2D(ctx, width, height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    auto framebuffer = gfx::vulkan::framebuffer_builder_t{}
        .add_attachment_view(color_image->image_view())
        .add_attachment_view(depth_image->image_view())
        .build(ctx, renderpass->renderpass(), width, height);
    
    auto descriptor_set_0_layout = gfx::vulkan::descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT)
        .build(ctx);
    auto descriptor_set_1_layout = gfx::vulkan::descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT)
        .build(ctx);

    auto pipeline = gfx::vulkan::pipeline_builder_t{}
        .add_shader("../../assets/shaders/test2/diffuse_only/shader.vert.spv")
        .add_shader("../../assets/shaders/test2/diffuse_only/shader.frag.spv")
        .add_descriptor_set_layout(descriptor_set_0_layout)
        .add_descriptor_set_layout(descriptor_set_1_layout)
        .add_descriptor_set_layout(material_descriptor_set_layout)
        .add_default_color_blend_attachment_state()
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
        .add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .add_vertex_input_binding_description(0, sizeof(core::vertex_t), VK_VERTEX_INPUT_RATE_VERTEX)
		.add_vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, position))
		.add_vertex_input_attribute_description(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, normal))
		.add_vertex_input_attribute_description(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(core::vertex_t, uv))
		.add_vertex_input_attribute_description(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, tangent))
		.add_vertex_input_attribute_description(0, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, bi_tangent))
        .build(ctx, renderpass->renderpass());

    std::vector<core::ref<gfx::vulkan::buffer_t>> set_0_uniform_buffer;
    std::vector<core::ref<gfx::vulkan::buffer_t>> set_1_uniform_buffer;
    std::vector<core::ref<gfx::vulkan::descriptor_set_t>> descriptor_set_0;
    std::vector<core::ref<gfx::vulkan::descriptor_set_t>> descriptor_set_1;
    
    for (uint32_t i = 0; i < ctx->MAX_FRAMES_IN_FLIGHT; i++) {
        set_0_uniform_buffer.push_back(gfx::vulkan::buffer_builder_t{}
            .build(ctx, sizeof(set_0_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
        set_1_uniform_buffer.push_back(gfx::vulkan::buffer_builder_t{}
            .build(ctx, sizeof(set_1_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

        descriptor_set_0.push_back(descriptor_set_0_layout->new_descriptor_set());
        descriptor_set_1.push_back(descriptor_set_1_layout->new_descriptor_set());

        descriptor_set_0[i]->write()
            .pushBufferInfo(0, 1, set_0_uniform_buffer[i]->descriptor_info())
            .update();
        descriptor_set_1[i]->write()
            .pushBufferInfo(0, 1, set_1_uniform_buffer[i]->descriptor_info())
            .update();
    }

    auto gpu_timer = core::make_ref<gfx::vulkan::gpu_timer_t>(ctx);
    
    auto swapchain_descriptor_set_layout = gfx::vulkan::descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(ctx);
    
    auto swapchain_descriptor_set = swapchain_descriptor_set_layout->new_descriptor_set();
    swapchain_descriptor_set->write()
        .pushImageInfo(0, 1, color_image->descriptor_info(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
        .update();

    auto swapchain_pipeline = gfx::vulkan::pipeline_builder_t{}
        .add_default_color_blend_attachment_state()
        .add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
        .add_descriptor_set_layout(swapchain_descriptor_set_layout)
        .add_shader("../../assets/shaders/test2/swapchain/base.vert.spv")
        .add_shader("../../assets/shaders/test2/swapchain/base.frag.spv")
        .build(ctx, ctx->swapchain_renderpass());    


    auto model = core::load_model_from_path("../../assets/models/Sponza/glTF/Sponza.gltf");
    {
        for (auto mesh : model._meshes) {
            gpu_mesh_t gpu_mesh{};
            core::ref<gfx::vulkan::buffer_t> staging_buffer;

            // vertex buffer
            staging_buffer = gfx::vulkan::buffer_builder_t{}
                .build(ctx,
                    mesh._vertices.size() * sizeof(core::vertex_t),
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

            gpu_mesh._vertex_buffer = gfx::vulkan::buffer_builder_t{}
                .build(ctx,
                    mesh._vertices.size() * sizeof(core::vertex_t),
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            std::memcpy(staging_buffer->map(), mesh._vertices.data(), mesh._vertices.size() * sizeof(core::vertex_t));
            staging_buffer->unmap();

            gfx::vulkan::buffer_t::copy(ctx, *staging_buffer, *gpu_mesh._vertex_buffer, VkBufferCopy{
                .size = mesh._vertices.size() * sizeof(core::vertex_t)
            });

            // index buffer
            staging_buffer = gfx::vulkan::buffer_builder_t{}
                .build(ctx,
                    mesh._indices.size() * sizeof(uint32_t),
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

            gpu_mesh._index_buffer = gfx::vulkan::buffer_builder_t{}
                .build(ctx,
                    mesh._indices.size() * sizeof(uint32_t),
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            std::memcpy(staging_buffer->map(), mesh._indices.data(), mesh._indices.size() * sizeof(uint32_t));
            staging_buffer->unmap();

            gfx::vulkan::buffer_t::copy(ctx, *staging_buffer, *gpu_mesh._index_buffer, VkBufferCopy{
                .size = mesh._indices.size() * sizeof(uint32_t)
            });
            gpu_mesh._index_count = mesh._indices.size();

            // material
            gpu_mesh._material_descriptor_set = material_descriptor_set_layout->new_descriptor_set();
            auto it = std::find_if(mesh._material._texture_infos.begin(), mesh._material._texture_infos.end(), [](const core::texture_info_t& texture_info) {
                return texture_info._texture_type == core::texture_type_t::e_diffuse_map;
            });
            if (it != mesh._material._texture_infos.end()) {
                auto image = gfx::vulkan::image_builder_t{}
                    .loadFromPath(ctx, it->_file_path);
                loaded_images.push_back(image);
                gpu_mesh._material_descriptor_set->write()
                    .pushImageInfo(0, 1, image->descriptor_info(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
                    .update();
            } else {
                assert(false);
            }
            gpu_meshes.push_back(gpu_mesh);
        }
    }

    EditorCamera editor_camera{window};

    float target_FPS = 60.f;
    auto last_time = std::chrono::system_clock::now();
    while (!window->shouldClose()) {
        window->pollEvents();
        
        auto current_time = std::chrono::system_clock::now();
        auto time_difference = current_time - last_time;
        if (time_difference.count() / 1e6 < 1000.f / target_FPS) {
            continue;
        }
        last_time = current_time;
        float dt = time_difference.count() / float(1e6);

        editor_camera.onUpdate(dt);

        set_0_t set_0;
        set_0.view = editor_camera.getView();
        set_0.projection = editor_camera.getProjection();
        set_0.inv_view = glm::inverse(set_0.view);
        set_0.inv_projection = glm::inverse(set_0.projection);

        set_1_t set_1;
        set_1.model = glm::mat4{1};
        set_1.inv_model = glm::inverse(set_1.model);

        if (auto start_frame = ctx->start_frame()) {
            auto [command_buffer, current_index] = *start_frame;

            VkClearValue clear_color{};
            clear_color.color = {0, 0, 0, 0};    
            VkClearValue clear_depth{};
            clear_depth.depthStencil.depth = 1;

            VkViewport swapchain_viewport{};
            swapchain_viewport.x = 0;
            swapchain_viewport.y = 0;
            swapchain_viewport.width = static_cast<float>(ctx->swapchain_extent().width);
            swapchain_viewport.height = static_cast<float>(ctx->swapchain_extent().height);
            swapchain_viewport.minDepth = 0;
            swapchain_viewport.maxDepth = 1;
            VkRect2D swapchain_scissor{};
            swapchain_scissor.offset = {0, 0};
            swapchain_scissor.extent = ctx->swapchain_extent();

            gpu_timer->begin(command_buffer);
            renderpass->begin(command_buffer, framebuffer->framebuffer(), VkRect2D{
                .offset = {0, 0},
                .extent = ctx->swapchain_extent(),
            }, {
                clear_color,
                clear_depth,
            }); 
            vkCmdSetViewport(command_buffer, 0, 1, &swapchain_viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &swapchain_scissor);

            std::memcpy(set_0_uniform_buffer[current_index]->map(), &set_0, sizeof(set_0));
            std::memcpy(set_1_uniform_buffer[current_index]->map(), &set_1, sizeof(set_1));

            VkDescriptorSet sets[] = { descriptor_set_0[current_index]->descriptor_set(), descriptor_set_1[current_index]->descriptor_set() };
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline_layout(), 0, 2, sets, 0, nullptr);
            pipeline->bind(command_buffer);
            for (auto gpu_mesh : gpu_meshes) {
                vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline_layout(), 2, 1, &gpu_mesh._material_descriptor_set->descriptor_set(), 0, nullptr);
                VkDeviceSize offsets{0};
                vkCmdBindVertexBuffers(command_buffer, 0, 1, &gpu_mesh._vertex_buffer->buffer(), &offsets);
                vkCmdBindIndexBuffer(command_buffer, gpu_mesh._index_buffer->buffer(), offsets, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(command_buffer, gpu_mesh._index_count, 1, 0, 0, 0);
            }
            gpu_timer->end(command_buffer);
            renderpass->end(command_buffer);

            ctx->begin_swapchain_renderpass(command_buffer, clear_color);
            vkCmdSetViewport(command_buffer, 0, 1, &swapchain_viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &swapchain_scissor);
            swapchain_pipeline->bind(command_buffer);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, swapchain_pipeline->pipeline_layout(), 0, 1, &swapchain_descriptor_set->descriptor_set(), 0, nullptr);
            vkCmdDraw(command_buffer, 6, 1, 0, 0);

            core::ImGui_newframe();

            ImGui::Begin("debug");
            if (auto t = gpu_timer->getTime()) {
                ImGui::Text("Diffuse only pass took: %f", *t);
            } else {
                ImGui::Text("Diffuse only pass took: undefined");
            }
            ImGui::End();

            core::ImGui_endframe(command_buffer);
            ctx->end_swapchain_renderpass(command_buffer);

            ctx->end_frame(command_buffer);
        }
        core::clear_frame_function_times();
    }

    ctx->wait_idle();

    core::ImGui_shutdown();

    return 0;
}