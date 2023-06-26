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
    auto window = std::make_shared<core::Window>("VIZON-vulkan", 800, 600);
    auto context = std::make_shared<gfx::vulkan::Context>(window, 2, true);
    auto dispatcher = std::make_shared<event::Dispatcher>();
    auto [width, height] = window->getSize();
    core::Material::init(context);

    entt::registry scene;

    EditorCamera editorCamera{window};
    
    auto globalDescriptorSetLayout = gfx::vulkan::DescriptorSetLayout::Builder{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS)
        .build(context);

    auto perObjectDescriptorSetLayout = gfx::vulkan::DescriptorSetLayout::Builder{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS)
        .build(context);    

    auto renderpass = gfx::vulkan::RenderPass::Builder{}
        .addColorAttachment(VkAttachmentDescription{
            .format = VK_FORMAT_R8G8B8A8_SRGB,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
        })
        .setDepthAttachment(VkAttachmentDescription{
            .format = VK_FORMAT_D32_SFLOAT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
        })
        .build(context);
    
    auto pipeline = gfx::vulkan::Pipeline::Builder{}
        .addDefaultColorBlendAttachmentState()
        .addDescriptorSetLayout(globalDescriptorSetLayout)
        .addDescriptorSetLayout(perObjectDescriptorSetLayout)
        .addDescriptorSetLayout(core::Material::getMaterialDescriptorSetLayout())
        .addDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
        .addDynamicState(VK_DYNAMIC_STATE_SCISSOR)
        .addVertexInputBindingDescription(0, sizeof(core::Vertex), VK_VERTEX_INPUT_RATE_VERTEX)
		.addVertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::Vertex, position))
		.addVertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::Vertex, normal))
		.addVertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(core::Vertex, uv))
		.addVertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::Vertex, tangent))
		.addVertexInputAttributeDescription(0, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::Vertex, biTangent))
        .addShader("../../projects/windowing/shaders/diffuse_only/base.vert.spv")
        .addShader("../../projects/windowing/shaders/diffuse_only/base.frag.spv")
        .build(context, renderpass->renderPass());

    auto imageColor = gfx::vulkan::Image::Builder{} 
        .build2D(context, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    auto imageDepth = gfx::vulkan::Image::Builder{}
        .build2D(context, width, height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    auto framebuffer = gfx::vulkan::Framebuffer::Builder{}
        .addAttachmentView(imageColor->imageView())
        .addAttachmentView(imageDepth->imageView())
        .build(context, renderpass->renderPass(), width, height);
    
    std::vector<std::shared_ptr<gfx::vulkan::Buffer>> globalUniformBufferObject;
    std::vector<std::shared_ptr<gfx::vulkan::DescriptorSet>> globalUniformBufferDescriptorSet;
    // std::vector<std::shared_ptr<gfx::vulkan::Buffer>> perObjectUniformBufferObject;
    // std::vector<std::shared_ptr<gfx::vulkan::DescriptorSet>> perObjectUniformBufferDescriptorSet;
    for (int i = 0; i < context->MAX_FRAMES_IN_FLIGHT; i++) {
        globalUniformBufferDescriptorSet.push_back(gfx::vulkan::DescriptorSet::Builder{}
            .build(context, globalDescriptorSetLayout));
        globalUniformBufferObject.push_back(gfx::vulkan::Buffer::Builder{}
            .build(context, sizeof(GlobalUniformBufferStruct), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
        globalUniformBufferDescriptorSet[i]->write()
            .pushBufferInfo(0, 1, globalUniformBufferObject[i]->descriptorInfo())
            .update();

        // perObjectUniformBufferDescriptorSet.push_back(gfx::vulkan::DescriptorSet::Builder{}
        //     .build(context, perObjectDescriptorSetLayout));
        // perObjectUniformBufferObject.push_back(gfx::vulkan::Buffer::Builder{}
        //     .build(context, sizeof(PerObjectUniformBufferStruct), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
        // perObjectUniformBufferDescriptorSet[i]->write()
        //     .pushBufferInfo(0, 1, perObjectUniformBufferObject[i]->descriptorInfo())
        //     .update();
    }

    auto swapChain_descriptorSetLayout = gfx::vulkan::DescriptorSetLayout::Builder{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL_GRAPHICS)
        .build(context);

    auto swapChain_descriptor = gfx::vulkan::DescriptorSet::Builder{}
        .build(context, swapChain_descriptorSetLayout);

    auto swapChain_pipeline = gfx::vulkan::Pipeline::Builder{}
        .addDefaultColorBlendAttachmentState()
        .addDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
        .addDynamicState(VK_DYNAMIC_STATE_SCISSOR)
        .addDescriptorSetLayout(swapChain_descriptorSetLayout)
        .addShader("../../assets/shaders/swapchain/base.vert.spv")
        .addShader("../../assets/shaders/swapchain/base.frag.spv")
        .build(context, context->swapChainRenderPass());

    swapChain_descriptor->write()
        .pushImageInfo(0, 1, imageColor->descriptorInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
        .update();

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
            auto mesh = scene.emplace<std::shared_ptr<core::Mesh>>(ent) = std::make_shared<core::Mesh>(context, vertices, indices);
            auto wind = scene.emplace<std::shared_ptr<Window>>(ent) = std::make_shared<Window>(context, "code");
            auto mat = scene.emplace<std::shared_ptr<core::Material>>(ent) = std::make_shared<core::Material>();
            mat->diffuse = wind->image();
            mesh->material = mat;
            mat->descriptorSet = gfx::vulkan::DescriptorSet::Builder{}
                .build(context, core::Material::getMaterialDescriptorSetLayout());
            // mat->update();
            mat->descriptorSet->write()
                .pushImageInfo(0, 1, mat->diffuse->descriptorInfo(VK_IMAGE_LAYOUT_GENERAL))
                .update();

            auto& descriptor = scene.emplace<std::vector<std::shared_ptr<gfx::vulkan::DescriptorSet>>>(ent);

            auto& buffer = scene.emplace<std::vector<std::shared_ptr<gfx::vulkan::Buffer>>>(ent);
            for (int i = 0; i < context->MAX_FRAMES_IN_FLIGHT; i++) {
                descriptor.push_back(gfx::vulkan::DescriptorSet::Builder{}
                    .build(context, perObjectDescriptorSetLayout));
                buffer.push_back(gfx::vulkan::Buffer::Builder{}
                    .build(context, sizeof(PerObjectUniformBufferStruct), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
                
                descriptor[i]->write()
                    .pushBufferInfo(0, 1, buffer[i]->descriptorInfo())
                    .update();
            }
        }

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

        if (auto startFrame = context->startFrame()) {
            auto [commandBuffer, currentIndex] = *startFrame;

            VkClearValue clearColor{};
            clearColor.color = {0, 0, 0, 0};
            VkClearValue clearDepth{};
            clearDepth.depthStencil.depth = 1;

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(context->swapChainExtent().width);
            viewport.height = static_cast<float>(context->swapChainExtent().height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = context->swapChainExtent();

            GlobalUniformBufferStruct globalUniformBuffer{};
            globalUniformBuffer.projection = editorCamera.getProjection();
            globalUniformBuffer.view = editorCamera.getView();
            globalUniformBuffer.invProjection = glm::inverse(globalUniformBuffer.projection);
            globalUniformBuffer.invView = glm::inverse(globalUniformBuffer.view);
            std::memcpy(globalUniformBufferObject[currentIndex]->map(), &globalUniformBuffer, sizeof(GlobalUniformBufferStruct));        

            // windowing.update(commandBuffer);
            for (auto [ent, window] : scene.view<std::shared_ptr<Window>>().each()) {
                window->update(commandBuffer);
            }


            renderpass->begin(commandBuffer, framebuffer->framebuffer(), VkRect2D{
                .offset = {0, 0},
                .extent = context->swapChainExtent(),
            }, {
                clearColor,
                clearDepth
            });
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
            pipeline->bind(commandBuffer);

            // VkDescriptorSet sets[] = { globalUniformBufferDescriptorSet[currentIndex]->descriptorSet(), perObjectUniformBufferDescriptorSet[currentIndex]->descriptorSet() };
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout(), 0, 1, &globalUniformBufferDescriptorSet[currentIndex]->descriptorSet(), 0, nullptr);
            for (auto [ent, mesh, transform, descriptor, buffer] : scene.view<std::shared_ptr<core::Mesh>, 
                                                                              core::Transform,
                                                                              std::vector<std::shared_ptr<gfx::vulkan::DescriptorSet>>, 
                                                                              std::vector<std::shared_ptr<gfx::vulkan::Buffer>>>().each()) {
                PerObjectUniformBufferStruct perObjectUniformBuffer{};
                perObjectUniformBuffer.model = transform.mat4();
                perObjectUniformBuffer.invModel = glm::inverse(perObjectUniformBuffer.model);
                std::memcpy(buffer[currentIndex]->map(), &perObjectUniformBuffer, sizeof(PerObjectUniformBufferStruct));
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout(), 1, 1, &descriptor[currentIndex]->descriptorSet(), 0, nullptr);
                mesh->draw(commandBuffer, pipeline->pipelineLayout(), true);
            }   

            renderpass->end(commandBuffer);

            context->beginSwapChainRenderPass(commandBuffer, clearColor);
    		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
            swapChain_pipeline->bind(commandBuffer);

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, swapChain_pipeline->pipelineLayout(), 0, 1, &swapChain_descriptor->descriptorSet(), 0, nullptr);
            vkCmdDraw(commandBuffer, 6, 1, 0, 0);
            context->endSwapChainRenderPass(commandBuffer);

            context->endFrame(commandBuffer);
        }

    }

    vkDeviceWaitIdle(context->device());

    core::Material::destroy();

    return 0;
}