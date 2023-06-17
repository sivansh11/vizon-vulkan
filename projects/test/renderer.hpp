#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "core/window.hpp"
#include "core/event.hpp"
#include "core/components.hpp"

#include "gfx/vulkan/context.hpp"
#include "gfx/vulkan/pipeline.hpp"
#include "gfx/vulkan/descriptor.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "gfx/vulkan/image.hpp"
#include "gfx/vulkan/renderpass.hpp"
#include "gfx/vulkan/framebuffer.hpp"

#include <glm/glm.hpp>
#include <entt/entt.hpp>

#include <memory>

class Renderer {
public:
    Renderer(std::shared_ptr<core::Window> window, std::shared_ptr<gfx::vulkan::Context> context, std::shared_ptr<event::Dispatcher> dispatcher);
    ~Renderer();

    void recreateDimentionDependentResources();
    void render(std::shared_ptr<entt::registry> scene, core::CameraComponent& camera);  
    
private:
    std::shared_ptr<core::Window> window;
    std::shared_ptr<event::Dispatcher> dispatcher;
    std::shared_ptr<gfx::vulkan::Context> context;

    std::shared_ptr<gfx::vulkan::DescriptorSetLayout> descriptorSetLayout0;
    std::shared_ptr<gfx::vulkan::DescriptorSetLayout> descriptorSetLayout1;
    std::vector<std::shared_ptr<gfx::vulkan::DescriptorSet>> descriptorSet0;
    std::vector<std::shared_ptr<gfx::vulkan::DescriptorSet>> descriptorSet1;

    std::vector<std::shared_ptr<gfx::vulkan::Buffer>> set0UniformBuffer;
    std::vector<std::shared_ptr<gfx::vulkan::Buffer>> set1UniformBuffer;
    std::vector<void *> set0UniformBufferMap;
    std::vector<void *> set1UniformBufferMap;

    std::shared_ptr<gfx::vulkan::RenderPass> deferredRenderPass;
    std::shared_ptr<gfx::vulkan::Image> gBufferAlbedoSpecImage;
    std::shared_ptr<gfx::vulkan::Image> gBufferNormalImage;
    std::shared_ptr<gfx::vulkan::Image> gBufferDepthImage;
    std::shared_ptr<gfx::vulkan::Framebuffer> gBufferFramebuffer;
    std::shared_ptr<gfx::vulkan::Pipeline> deferredPipeline;

    std::shared_ptr<gfx::vulkan::DescriptorSetLayout> shadowDescriptorSetLayout0;
    std::vector<std::shared_ptr<gfx::vulkan::DescriptorSet>> shadowDescriptorSet0;
    std::vector<std::shared_ptr<gfx::vulkan::Buffer>> shadowSet0UniformBuffer;
    std::vector<void *> shadowSet0UniformBufferMap;
    std::shared_ptr<gfx::vulkan::RenderPass> shadowRenderPass;
    std::shared_ptr<gfx::vulkan::Image> shadowMapImage;
    std::shared_ptr<gfx::vulkan::Framebuffer> shadowFramebuffer;
    std::shared_ptr<gfx::vulkan::Pipeline> shadowPipeline;

    std::shared_ptr<gfx::vulkan::RenderPass> compositeRenderPass;
    std::shared_ptr<gfx::vulkan::Image> compositeImage;
    std::shared_ptr<gfx::vulkan::Framebuffer> compositeFramebuffer;
    std::shared_ptr<gfx::vulkan::DescriptorSetLayout> compositeSetLayout0;
    std::shared_ptr<gfx::vulkan::Pipeline> compositePipeline;
    std::vector<std::shared_ptr<gfx::vulkan::DescriptorSet>> compositeSet0;
    std::vector<std::shared_ptr<gfx::vulkan::Buffer>> compositeSet0UniformBuffer;
    std::vector<void *> compositeSet0UniformBufferMap;

    std::shared_ptr<gfx::vulkan::DescriptorSetLayout> swapChainPipelineDescriptorSetLayout;
    std::shared_ptr<gfx::vulkan::DescriptorSet> swapChainPipelineDescriptorSet;
    std::shared_ptr<gfx::vulkan::Pipeline> swapChainPipeline;
};

#endif