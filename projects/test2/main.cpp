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

#include <glm/glm.hpp>
#include "glm/gtx/string_cast.hpp"
#include <entt/entt.hpp>

#include <memory>
#include <iostream>
#include <chrono>

int main(int argc, char **argv) {
    auto window = core::make_ref<core::Window>("test2", 1200, 800);
    auto context = core::make_ref<gfx::vulkan::Context>(window, 2, true);

    auto [width, height] = window->getSize();

    auto main_imageColor = gfx::vulkan::Image::Builder{}
        .build2D(context, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    auto main_imageDepth = gfx::vulkan::Image::Builder{}
        .build2D(context, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    auto main_framebuffer = gfx::vulkan::Framebuffer::Builder{}
        .addAttachmentView(main_imageColor->imageView())
        .addAttachmentView(main_imageDepth->imageView())
        .build(context, context->swapChainRenderPass(), width, height);

    auto swapChain_descriptorSetLayout = gfx::vulkan::DescriptorSetLayout::Builder{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL)
        .build(context);
    auto swapChain_descriptorSet = gfx::vulkan::DescriptorSet::Builder{}
        .build(context, swapChain_descriptorSetLayout);
    swapChain_descriptorSet->write()
        .pushImageInfo(0, 1, main_imageColor->descriptorInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
        .update();
    auto swapChain_pipeline = gfx::vulkan::Pipeline::Builder{}
        .addDefaultColorBlendAttachmentState()
        .addDescriptorSetLayout(swapChain_descriptorSetLayout)
        .addDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
        .addDynamicState(VK_DYNAMIC_STATE_SCISSOR)
        .addShader("../../assets/shaders/test2/swapchain/base.vert.spv")
        .addShader("../../assets/shaders/test2/swapchain/base.frag.spv")
        .build(context, context->swapChainRenderPass());

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

        if (auto startFrame = context->startFrame()) {
            auto [commandBuffer, currentIndex] = *startFrame;

            VkClearValue clearColor{};
            clearColor.color = {0, 0, 0, 0};    
            VkClearValue clearDepth{};
            clearDepth.depthStencil.depth = 1;

            VkViewport swapChain_viewPort{};
            swapChain_viewPort.x = 0;
            swapChain_viewPort.y = 0;
            swapChain_viewPort.width = static_cast<float>(context->swapChainExtent().width);
            swapChain_viewPort.height = static_cast<float>(context->swapChainExtent().height);
            swapChain_viewPort.minDepth = 0;
            swapChain_viewPort.maxDepth = 1;
            VkRect2D swapChain_scissor{};
            swapChain_scissor.offset = {0, 0};
            swapChain_scissor.extent = context->swapChainExtent();


            context->beginSwapChainRenderPass(commandBuffer, clearColor);
            context->endSwapChainRenderPass(commandBuffer);

            context->endFrame(commandBuffer);
        }
    }

    return 0;
}