#include "core/window.hpp"
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

#include "editor_camera.hpp"
#include "ubos.hpp"
#include "window.hpp"

#include <glm/glm.hpp>
#include "glm/gtx/string_cast.hpp"
#include <entt/entt.hpp>
#include <ScreenCapture.h>

#include <memory>
#include <iostream>
#include <chrono>
#include <atomic>

int main(int argc, char **argv) {
    auto window = core::make_ref<core::Window>("VIZON-vulkan", 800, 600);
    auto context = core::make_ref<gfx::vulkan::context_t>(window, 2, true);
    auto dispatcher = core::make_ref<event::Dispatcher>();
    core::Material::init(context);

    entt::registry scene;

    EditorCamera editorCamera{window};
    
    auto globalDescriptorSetLayout = gfx::vulkan::descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS)
        .build(context);

    auto perObjectDescriptorSetLayout = gfx::vulkan::descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS)
        .build(context);    

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
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
        })
        .build(context);
    
    auto pipeline = gfx::vulkan::pipeline_builder_t{}
        .add_default_color_blend_attachment_state()
        .add_descriptor_set_layout(globalDescriptorSetLayout)
        .add_descriptor_set_layout(perObjectDescriptorSetLayout)
        .add_descriptor_set_layout(core::Material::getMaterialDescriptorSetLayout())
        .add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
        .add_vertex_input_binding_description(0, sizeof(core::Vertex), VK_VERTEX_INPUT_RATE_VERTEX)
		.add_vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::Vertex, position))
		.add_vertex_input_attribute_description(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::Vertex, normal))
		.add_vertex_input_attribute_description(0, 2, VK_FORMAT_R32G32_SFLOAT,    offsetof(core::Vertex, uv))
		.add_vertex_input_attribute_description(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::Vertex, tangent))
		.add_vertex_input_attribute_description(0, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::Vertex, biTangent))
        .add_shader("../../projects/windowing/shaders/diffuse_only/base.vert.spv")
        .add_shader("../../projects/windowing/shaders/diffuse_only/base.frag.spv")
        .build(context, renderpass->renderpass());

    auto [width, height] = window->getSize();
    auto imageColor = gfx::vulkan::image_builder_t{} 
        .build2D(context, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    auto imageDepth = gfx::vulkan::image_builder_t{}
        .build2D(context, width, height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    auto framebuffer = gfx::vulkan::framebuffer_builder_t{}
        .add_attachment_view(imageColor->image_view())
        .add_attachment_view(imageDepth->image_view())
        .build(context, renderpass->renderpass(), width, height);
    
    std::vector<std::shared_ptr<gfx::vulkan::buffer_t>> globalUniformBufferObject;
    std::vector<std::shared_ptr<gfx::vulkan::descriptor_set_t>> globalUniformBufferDescriptorSet;
    for (int i = 0; i < context->MAX_FRAMES_IN_FLIGHT; i++) {
        globalUniformBufferDescriptorSet.push_back(gfx::vulkan::descriptor_set_builder_t{}
            .build(context, globalDescriptorSetLayout));
        globalUniformBufferObject.push_back(gfx::vulkan::buffer_builder_t{}
            .build(context, sizeof(GlobalUniformBufferStruct), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
        globalUniformBufferDescriptorSet[i]->write()
            .pushBufferInfo(0, 1, globalUniformBufferObject[i]->descriptor_info())
            .update();
    }

    auto swapChain_descriptorSetLayout = gfx::vulkan::descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL_GRAPHICS)
        .build(context);

    auto swapChain_descriptor = gfx::vulkan::descriptor_set_builder_t{}
        .build(context, swapChain_descriptorSetLayout);

    auto swapChain_pipeline = gfx::vulkan::pipeline_builder_t{}
        .add_default_color_blend_attachment_state()
        .add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
        .add_descriptor_set_layout(swapChain_descriptorSetLayout)
        .add_shader("../../assets/shaders/swapchain/base.vert.spv")
        .add_shader("../../assets/shaders/swapchain/base.frag.spv")
        .build(context, context->swapchain_renderpass());

    swapChain_descriptor->write()
        .pushImageInfo(0, 1, imageColor->descriptor_info(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
        .update();

    // setting up the textured quad entity, atm this is very messy, should be made better/cleaner
    {
        auto ent = scene.create();
        auto& t = scene.emplace<core::Transform>(ent);
        t.rotation = {0, glm::half_pi<float>(), 0};
        t.translation = {3, 0, 0};
        t.scale = {-1, -1, 1};
        std::vector<core::Vertex> vertices{
            core::Vertex{{-1.0, -0.5, 0}, {}, {0, 0}},
            core::Vertex{{-1.0,  0.5, 0}, {}, {0, 1}},
            core::Vertex{{ 1.0,  0.5, 0}, {}, {1, 1}},

            core::Vertex{{-1.0, -0.5, 0}, {}, {0, 0}},
            core::Vertex{{ 1.0,  0.5, 0}, {}, {1, 1}},
            core::Vertex{{ 1.0, -0.5, 0}, {}, {1, 0}},
        };  
        std::vector<uint32_t> indices {
            0, 1, 2,
            3, 4, 5
        };
        auto mesh = scene.emplace<std::shared_ptr<core::Mesh>>(ent) = core::make_ref<core::Mesh>(context, vertices, indices);
        auto wind = scene.emplace<std::shared_ptr<Window>>(ent) = core::make_ref<Window>(context, "firefox");
        auto mat = scene.emplace<std::shared_ptr<core::Material>>(ent) = core::make_ref<core::Material>();
        mat->diffuse = wind->image();
        mat->descriptor_set = gfx::vulkan::descriptor_set_builder_t{}
            .build(context, core::Material::getMaterialDescriptorSetLayout());
        mat->descriptor_set->write()
            .pushImageInfo(0, 1, mat->diffuse->descriptor_info(VK_IMAGE_LAYOUT_GENERAL))
            .update();
        mesh->material = mat;
        auto& descriptor = scene.emplace<std::vector<std::shared_ptr<gfx::vulkan::descriptor_set_t>>>(ent);
        auto& buffer = scene.emplace<std::vector<std::shared_ptr<gfx::vulkan::buffer_t>>>(ent);
        for (int i = 0; i < context->MAX_FRAMES_IN_FLIGHT; i++) {
            descriptor.push_back(gfx::vulkan::descriptor_set_builder_t{}
                .build(context, perObjectDescriptorSetLayout));
            buffer.push_back(gfx::vulkan::buffer_builder_t{}
                .build(context, sizeof(PerObjectUniformBufferStruct), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
            descriptor[i]->write()
                .pushBufferInfo(0, 1, buffer[i]->descriptor_info())
                .update();
        }
    }

    context->add_resize_callback([&]() {
        auto [width, height] = window->getSize();
        imageColor = gfx::vulkan::image_builder_t{} 
            .build2D(context, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        imageDepth = gfx::vulkan::image_builder_t{}
            .build2D(context, width, height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        framebuffer = gfx::vulkan::framebuffer_builder_t{}
            .add_attachment_view(imageColor->image_view())
            .add_attachment_view(imageDepth->image_view())
            .build(context, renderpass->renderpass(), width, height);

        swapChain_descriptor->write()
            .pushImageInfo(0, 1, imageColor->descriptor_info(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
            .update();
    });

    float targetFPS = 60.f;
    auto lastTime = std::chrono::system_clock::now();
    while (!window->shouldClose()) {
        window->pollEvents();

        auto currentTime = std::chrono::system_clock::now();
        auto dt = currentTime - lastTime;
        if (dt.count() / 1e6 < 1000.f / targetFPS) {
            continue;
        }
        lastTime = currentTime;

        auto ms = dt.count() / float(1e6);

        editorCamera.onUpdate(dt.count() / 1e6);
        
        // INFO("Took {}ms, ----FPS is {}", dt.count() / 1e6, 1000.f / ms);
        // INFO("---------------------------------------------");

        editorCamera.onUpdate(dt.count() / 1e6);

        if (auto startFrame = context->start_frame()) {
            auto [commandBuffer, currentIndex] = *startFrame;

            VkClearValue clearColor{};
            clearColor.color = {0, 0, 0, 0};
            VkClearValue clearDepth{};
            clearDepth.depthStencil.depth = 1;

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(context->swapchain_extent().width);
            viewport.height = static_cast<float>(context->swapchain_extent().height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = context->swapchain_extent();

            GlobalUniformBufferStruct globalUniformBuffer{};
            globalUniformBuffer.projection = editorCamera.getProjection();
            globalUniformBuffer.view = editorCamera.getView();
            globalUniformBuffer.invProjection = glm::inverse(globalUniformBuffer.projection);
            globalUniformBuffer.invView = glm::inverse(globalUniformBuffer.view);
            std::memcpy(globalUniformBufferObject[currentIndex]->map(), &globalUniformBuffer, sizeof(GlobalUniformBufferStruct));        

            for (auto [ent, window] : scene.view<std::shared_ptr<Window>>().each()) {
                window->update(commandBuffer);
            }

            renderpass->begin(commandBuffer, framebuffer->framebuffer(), VkRect2D{
                .offset = {0, 0},
                .extent = context->swapchain_extent(),
            }, {
                clearColor,
                clearDepth
            });
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
            pipeline->bind(commandBuffer);

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline_layout(), 0, 1, &globalUniformBufferDescriptorSet[currentIndex]->descriptor_set(), 0, nullptr);
            for (auto [ent, mesh, transform, descriptor, buffer] : scene.view<std::shared_ptr<core::Mesh>, 
                                                                              core::Transform,
                                                                              std::vector<std::shared_ptr<gfx::vulkan::descriptor_set_t>>, 
                                                                              std::vector<std::shared_ptr<gfx::vulkan::buffer_t>>>().each()) {
                PerObjectUniformBufferStruct perObjectUniformBuffer{};
                perObjectUniformBuffer.model = transform.mat4();
                perObjectUniformBuffer.invModel = glm::inverse(perObjectUniformBuffer.model);
                std::memcpy(buffer[currentIndex]->map(), &perObjectUniformBuffer, sizeof(PerObjectUniformBufferStruct));
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline_layout(), 1, 1, &descriptor[currentIndex]->descriptor_set(), 0, nullptr);
                mesh->draw(commandBuffer, pipeline->pipeline_layout(), true);
            }   

            renderpass->end(commandBuffer);

            context->begin_swapchain_renderpass(commandBuffer, clearColor);
    		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
            swapChain_pipeline->bind(commandBuffer);

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, swapChain_pipeline->pipeline_layout(), 0, 1, &swapChain_descriptor->descriptor_set(), 0, nullptr);
            vkCmdDraw(commandBuffer, 6, 1, 0, 0);
            context->end_swapchain_renderpass(commandBuffer);

            context->end_frame(commandBuffer);
        }
    }

    vkDeviceWaitIdle(context->device());

    core::Material::destroy();

    return 0;
}