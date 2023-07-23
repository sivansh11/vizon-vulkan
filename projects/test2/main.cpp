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

#include <glm/glm.hpp>
#include "glm/gtx/string_cast.hpp"
#include <entt/entt.hpp>

#include "core/imgui_utils.hpp"

#include <memory>
#include <iostream>
#include <chrono>

struct gpu_mesh_t {
    core::ref<gfx::vulkan::buffer_t> _vertex_buffer;
    core::ref<gfx::vulkan::buffer_t> _index_buffer;
    core::ref<gfx::vulkan::DescriptorSet> _material_descriptor_set;
};

int main(int argc, char **argv) {
    auto window = core::make_ref<core::Window>("test2", 1200, 800);
    auto ctx = core::make_ref<gfx::vulkan::Context>(window, 2, true);

    core::ImGui_init(window, ctx);

    auto model = core::load_model_from_path("../../assets/models/Sponza/glTF/Sponza.gltf");

    std::vector<gpu_mesh_t> gpu_meshes;

    auto material_descriptor_set_layout = gfx::vulkan::DescriptorSetLayout::Builder{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(ctx);

    for (auto mesh : model._meshes) {
        gpu_mesh_t gpu_mesh{};
        core::ref<gfx::vulkan::buffer_t> staging_buffer;

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
    }

    auto [width, height] = window->getSize();

    auto renderPass = gfx::vulkan::RenderPass::Builder{}
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
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,                  // no prepass
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
        })
        .build(ctx);
    auto colorImage = gfx::vulkan::Image::Builder{}
        .build2D(ctx, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    auto depthImage = gfx::vulkan::Image::Builder{}
        .build2D(ctx, width, height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    auto framebuffer = gfx::vulkan::Framebuffer::Builder{}
        .addAttachmentView(colorImage->imageView())
        .addAttachmentView(depthImage->imageView())
        .build(ctx, renderPass->renderPass(), width, height);
    
    auto swapchainDescriptorSetLayout = gfx::vulkan::DescriptorSetLayout::Builder{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(ctx);
    
    auto swapchainDescriptorSet = swapchainDescriptorSetLayout->newDescriptorSet();
    swapchainDescriptorSet->write()
        .pushImageInfo(0, 1, colorImage->descriptorInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
        .update();

    auto swapchainPipeline = gfx::vulkan::Pipeline::Builder{}
        .addDefaultColorBlendAttachmentState()
        .addDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
        .addDynamicState(VK_DYNAMIC_STATE_SCISSOR)
        .addDescriptorSetLayout(swapchainDescriptorSetLayout)
        .addShader("../../assets/shaders/test2/swapchain/base.vert.spv")
        .addShader("../../assets/shaders/test2/swapchain/base.frag.spv")
        .build(ctx, ctx->swapChainRenderPass());    

    float targetFPS = 60.f;
    auto lastTime = std::chrono::system_clock::now();
    while (!window->shouldClose()) {
        window->pollEvents();
        
        auto currentTime = std::chrono::system_clock::now();
        auto timeDifference = currentTime - lastTime;
        if (timeDifference.count() / 1e6 < 1000.f / targetFPS) {
            continue;
        }
        lastTime = currentTime;
        float dt = timeDifference.count() / float(1e6);

        if (auto startFrame = ctx->startFrame()) {
            auto [commandBuffer, currentIndex] = *startFrame;

            VkClearValue clearColor{};
            clearColor.color = {0, 0, 0, 0};    
            VkClearValue clearDepth{};
            clearDepth.depthStencil.depth = 1;

            VkViewport swapChainViewPort{};
            swapChainViewPort.x = 0;
            swapChainViewPort.y = 0;
            swapChainViewPort.width = static_cast<float>(ctx->swapChainExtent().width);
            swapChainViewPort.height = static_cast<float>(ctx->swapChainExtent().height);
            swapChainViewPort.minDepth = 0;
            swapChainViewPort.maxDepth = 1;
            VkRect2D swapChainScissor{};
            swapChainScissor.offset = {0, 0};
            swapChainScissor.extent = ctx->swapChainExtent();

            renderPass->begin(commandBuffer, framebuffer->framebuffer(), VkRect2D{
                .offset = {0, 0},
                .extent = ctx->swapChainExtent(),
            }, {
                clearColor,
                clearDepth,
            });

            renderPass->end(commandBuffer);

            ctx->beginSwapChainRenderPass(commandBuffer, clearColor);
            vkCmdSetViewport(commandBuffer, 0, 1, &swapChainViewPort);
            vkCmdSetScissor(commandBuffer, 0, 1, &swapChainScissor);
            swapchainPipeline->bind(commandBuffer);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, swapchainPipeline->pipelineLayout(), 0, 1, &swapchainDescriptorSet->descriptorSet(), 0, nullptr);
            vkCmdDraw(commandBuffer, 6, 1, 0, 0);

            core::ImGui_newframe();

            ImGui::Begin("hi");
            ImGui::End();

            core::ImGui_endframe(commandBuffer);
            ctx->endSwapChainRenderPass(commandBuffer);

            ctx->endFrame(commandBuffer);
        }
        core::clear_frame_function_times();
    }

    ctx->waitIdle();

    core::ImGui_shutdown();

    return 0;
}