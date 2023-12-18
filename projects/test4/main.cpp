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
#include "gfx/vulkan/acceleration_structure.hpp"

#include <glm/glm.hpp>
#include "glm/gtx/string_cast.hpp"
#include <entt/entt.hpp>

#include "core/imgui_utils.hpp"

#include "editor_camera.hpp"

#include <memory>
#include <iostream>
#include <algorithm>
#include <chrono>

using namespace core;
using namespace gfx::vulkan;

struct acceleration_structure__T {
    VkAccelerationStructureKHR acceleration_structure;
    VkDeviceAddress device_address;
    ref<buffer_t> buffer;
};

struct gpu_data_t {
    ref<buffer_t> vertex_buffer;
    ref<buffer_t> index_buffer;
    ref<acceleration_structure_t> acceleration_structure;
};

int main(int argc, char **argv) {
    auto window = core::make_ref<core::window_t>("test3", 1200, 800);
    auto ctx = core::make_ref<gfx::vulkan::context_t>(window, 1, true, true);
    core::ImGui_init(window, ctx);
    auto [width, height] = window->get_dimensions();

    // temp buffer
    ref<buffer_t> staging_buffer;
    ref<buffer_t> scratch_buffer;

    std::vector<gpu_data_t> all_gpu_data;
    std::vector<VkAccelerationStructureInstanceKHR> blas_instances;


    model_t model = core::load_model_from_path("../../assets/models/cube/cube_scene.obj");

    mesh_t combined_mesh{};

    for (auto mesh : model.meshes) {
        combined_mesh.vertices.reserve(combined_mesh.vertices.size() + mesh.vertices.size());
        for (auto vertex : mesh.vertices) {
            combined_mesh.vertices.push_back(vertex);
        }
        uint32_t index_offset = combined_mesh.indices.size();
        combined_mesh.indices.reserve(combined_mesh.indices.size() + mesh.indices.size());
        for (auto index : mesh.indices) {
            combined_mesh.indices.push_back(index_offset + index);
        }
    }

    model.meshes.clear();
    model.meshes.push_back(combined_mesh);

    /* say I have 3 meshes each with its own vertex and index buffer
    do I first combine them (modify indecies so they are the local mesh index + offset)
    create the 3 blas with appropriate vertex and index offsets
    then create the tlas with the blas instances ? */

    {
        for (auto mesh : model.meshes) {
            gpu_data_t gpu_data{};

            // vertex data
            staging_buffer = buffer_builder_t{}
                .build(ctx,
                       mesh.vertices.size() * sizeof(vertex_t),
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            gpu_data.vertex_buffer = buffer_builder_t{}
                .build(ctx,
                       mesh.vertices.size() * sizeof(vertex_t),
                       VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            std::memcpy(staging_buffer->map(), mesh.vertices.data(), mesh.vertices.size() * sizeof(vertex_t));
            buffer_t::copy(ctx, *staging_buffer, *gpu_data.vertex_buffer, VkBufferCopy{
                .size = mesh.vertices.size() * sizeof(vertex_t),
            });

            // index data
            staging_buffer = buffer_builder_t{}
                .build(ctx,
                       mesh.indices.size() * sizeof(uint32_t),
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            gpu_data.index_buffer = buffer_builder_t{}
                .build(ctx,
                       mesh.indices.size() * sizeof(uint32_t),
                       VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            std::memcpy(staging_buffer->map(), mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));
            buffer_t::copy(ctx, *staging_buffer, *gpu_data.index_buffer, VkBufferCopy{
                .size = mesh.indices.size() * sizeof(uint32_t),
            });

            gpu_data.acceleration_structure = acceleration_structure_builder_t{}
                .triangles(gpu_data.vertex_buffer->device_address(), sizeof(vertex_t), mesh.vertices.size(), gpu_data.index_buffer->device_address(), mesh.indices.size())
                .build(ctx);

            VkAccelerationStructureInstanceKHR blas_instance{};
            blas_instance.transform = {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
            };
            blas_instance.instanceCustomIndex = 0;
            blas_instance.mask = 0xFF;
            blas_instance.instanceShaderBindingTableRecordOffset = 0;
            blas_instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            blas_instance.accelerationStructureReference = gpu_data.acceleration_structure->device_address();

            all_gpu_data.push_back(gpu_data);
            blas_instances.push_back(blas_instance);
        }
    }

    auto instance_buffer = buffer_builder_t{}
        .build(ctx,
               sizeof(VkAccelerationStructureInstanceKHR) * blas_instances.size(),
               VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    std::memcpy(instance_buffer->map(), blas_instances.data(), sizeof(VkAccelerationStructureInstanceKHR) * blas_instances.size());
    instance_buffer->unmap();

    auto tlas = acceleration_structure_builder_t{}
        .instances(instance_buffer->device_address(), blas_instances.size())
        .build(ctx);

    auto rt_renderpass = renderpass_builder_t{}
        .add_color_attachment(VkAttachmentDescription{
            .format = VK_FORMAT_R8G8B8A8_SNORM,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
        })
        .build(ctx);

    auto rt_image = image_builder_t{}
        .build2D(ctx, width, height, VK_FORMAT_R8G8B8A8_SNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    auto rt_framebuffer = framebuffer_builder_t{}
        .add_attachment_view(rt_image->image_view())
        .build(ctx, rt_renderpass->renderpass(), width, height);
    
    auto rt_descriptor_set_layout = descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .addLayoutBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .addLayoutBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .addLayoutBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(ctx);

    auto rt_descriptor_set = rt_descriptor_set_layout->new_descriptor_set();

    struct uniform_data_t {
        glm::mat4 view_inv;
        glm::mat4 projection_inv;
    } uniform_data;

    auto rt_uniform_buffer = buffer_builder_t{}
        .build(ctx, sizeof(uniform_data), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    rt_descriptor_set->write()
        .pushAccelerationStructureInfo(0, 1, VkWriteDescriptorSetAccelerationStructureKHR{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .accelerationStructureCount = 1,
            .pAccelerationStructures = &tlas->acceleration_structure(),
        })
        .pushBufferInfo(1, 1, rt_uniform_buffer->descriptor_info())
        .pushBufferInfo(2, 1, all_gpu_data[0].vertex_buffer->descriptor_info(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
        .pushBufferInfo(3, 1, all_gpu_data[0].index_buffer->descriptor_info(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
        .update();

    auto rt_pipeline = pipeline_builder_t{}
        .add_default_color_blend_attachment_state()
        .add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
        .add_descriptor_set_layout(rt_descriptor_set_layout)
        .add_shader("../../assets/shaders/test4/rt/shader.vert")
        .add_shader("../../assets/shaders/test4/rt/shader.frag")
        .build(ctx, rt_renderpass->renderpass());

    auto rt_timer = make_ref<gpu_timer_t>(ctx);

    auto swapchain_descriptor_set_layout = descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(ctx);

    auto swapchain_descriptor_set = swapchain_descriptor_set_layout->new_descriptor_set();

    swapchain_descriptor_set->write()
        .pushImageInfo(0, 1, rt_image->descriptor_info(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
        .update();
    
    auto swapchain_pipeline = pipeline_builder_t{}
        .add_default_color_blend_attachment_state()
        .add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
        .add_descriptor_set_layout(swapchain_descriptor_set_layout)
        .add_shader("../../assets/shaders/swapchain/base.vert")
        .add_shader("../../assets/shaders/swapchain/base.frag")
        .build(ctx, ctx->swapchain_renderpass());

    editor_camera_t editor_camera{window};

    float target_FPS = 60.f;
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

        editor_camera.onUpdate(dt);

        uniform_data.view_inv = glm::inverse(editor_camera.getView());
        uniform_data.projection_inv = glm::inverse(editor_camera.getProjection());
        std::memcpy(rt_uniform_buffer->map(), &uniform_data, sizeof(uniform_data));

        if (auto start_frame = ctx->start_frame()) {
            auto [command_buffer, current_index] = *start_frame;

            VkClearValue clear_color{};
            clear_color.color = {0, 0, 0, 0};    
            VkClearValue clear_depth{};
            clear_depth.depthStencil.depth = 1;
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(ctx->swapchain_extent().width);
            viewport.height = static_cast<float>(ctx->swapchain_extent().height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = ctx->swapchain_extent();   

            rt_timer->begin(command_buffer);
            rt_renderpass->begin(command_buffer, rt_framebuffer->framebuffer(), VkRect2D{
                .offset = {0, 0},
                .extent = ctx->swapchain_extent(),
            }, {
                clear_color,
            }); 

            vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    		vkCmdSetScissor(command_buffer, 0, 1, &scissor);
            rt_pipeline->bind(command_buffer);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rt_pipeline->pipeline_layout(), 0, 1, &rt_descriptor_set->descriptor_set(), 0, nullptr);
            vkCmdDraw(command_buffer, 6, 1, 0, 0);

            rt_timer->end(command_buffer);
            rt_renderpass->end(command_buffer);


            ctx->begin_swapchain_renderpass(command_buffer, clear_color);

            vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    		vkCmdSetScissor(command_buffer, 0, 1, &scissor);
            swapchain_pipeline->bind(command_buffer);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, swapchain_pipeline->pipeline_layout(), 0, 1, &swapchain_descriptor_set->descriptor_set(), 0, nullptr);
            vkCmdDraw(command_buffer, 6, 1, 0, 0);

            core::ImGui_newframe();
            ImGui::Begin("debug");
            if (auto t = rt_timer->get_time()) {
                ImGui::Text("%f", *t);
            } else {
                ImGui::Text("undefined");
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