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
#include "gfx/vulkan/timer.hpp"

#include <glm/glm.hpp>
#include <entt/entt.hpp>

#include <memory>

class Renderer {
public:
    Renderer(std::shared_ptr<core::Window> window, std::shared_ptr<gfx::vulkan::context_t> context, std::shared_ptr<event::Dispatcher> dispatcher);
    ~Renderer();

    void recreateDimentionDependentResources();
    void render(std::shared_ptr<entt::registry> scene, core::CameraComponent& camera);  
    
private:
    std::shared_ptr<core::Window> window;
    std::shared_ptr<event::Dispatcher> dispatcher;
    std::shared_ptr<gfx::vulkan::context_t> context;

    std::shared_ptr<gfx::vulkan::descriptor_set_layout_t> depthDescriptorSetLayout0;
    std::vector<std::shared_ptr<gfx::vulkan::descriptor_set_t>> depthDescriptorSet0;
    std::vector<std::shared_ptr<gfx::vulkan::buffer_t>> depthSet0UniformBuffer;
    std::vector<void *> depthSet0UniformBufferMap;
    std::shared_ptr<gfx::vulkan::RenderPass> depthRenderPass;
    std::shared_ptr<gfx::vulkan::Image> depthImage;
    std::shared_ptr<gfx::vulkan::Framebuffer> depthFramebuffer;
    std::shared_ptr<gfx::vulkan::Pipeline> depthPipeline;
    std::shared_ptr<gfx::vulkan::Timer> depthTimer;

    std::shared_ptr<gfx::vulkan::descriptor_set_layout_t> descriptorSetLayout0;
    std::shared_ptr<gfx::vulkan::descriptor_set_layout_t> descriptorSetLayout1;
    std::vector<std::shared_ptr<gfx::vulkan::descriptor_set_t>> descriptorSet0;
    std::vector<std::shared_ptr<gfx::vulkan::descriptor_set_t>> descriptorSet1;
    std::vector<std::shared_ptr<gfx::vulkan::buffer_t>> set0UniformBuffer;
    std::vector<std::shared_ptr<gfx::vulkan::buffer_t>> set1UniformBuffer;
    std::vector<void *> set0UniformBufferMap;
    std::vector<void *> set1UniformBufferMap;
    std::shared_ptr<gfx::vulkan::RenderPass> deferredRenderPass;
    std::shared_ptr<gfx::vulkan::Image> gBufferAlbedoSpecImage;
    std::shared_ptr<gfx::vulkan::Image> gBufferNormalImage;
    std::shared_ptr<gfx::vulkan::Image> gBufferDepthImage;
    std::shared_ptr<gfx::vulkan::Framebuffer> gBufferFramebuffer;
    std::shared_ptr<gfx::vulkan::Pipeline> deferredPipeline;
    std::shared_ptr<gfx::vulkan::Timer> deferredTimer;  

    std::shared_ptr<gfx::vulkan::descriptor_set_layout_t> shadowDescriptorSetLayout0;
    std::vector<std::shared_ptr<gfx::vulkan::descriptor_set_t>> shadowDescriptorSet0;
    std::vector<std::shared_ptr<gfx::vulkan::buffer_t>> shadowSet0UniformBuffer;
    std::vector<void *> shadowSet0UniformBufferMap;
    std::shared_ptr<gfx::vulkan::RenderPass> shadowRenderPass;
    std::shared_ptr<gfx::vulkan::Image> shadowMapImage;
    std::shared_ptr<gfx::vulkan::Framebuffer> shadowFramebuffer;
    std::shared_ptr<gfx::vulkan::Pipeline> shadowPipeline;
    std::shared_ptr<gfx::vulkan::Timer> shadowTimer;  

    std::shared_ptr<gfx::vulkan::RenderPass> ssaoRenderPass;
    std::shared_ptr<gfx::vulkan::descriptor_set_layout_t> ssaoDescriptorSetLayout0;
    std::shared_ptr<gfx::vulkan::Image> ssaoImage;
    std::shared_ptr<gfx::vulkan::Image> noise;
    std::shared_ptr<gfx::vulkan::Framebuffer> ssaoFramebuffer;
    std::shared_ptr<gfx::vulkan::Pipeline> ssaoPipeline;
    std::vector<std::shared_ptr<gfx::vulkan::descriptor_set_t>> ssaoSet0;
    std::vector<std::shared_ptr<gfx::vulkan::buffer_t>> ssaoSet0UniformBuffer;
    std::vector<void *> ssaoSet0UniformBufferMap;
    std::shared_ptr<gfx::vulkan::Timer> ssaoTimer;  

    std::shared_ptr<gfx::vulkan::RenderPass> compositeRenderPass;
    std::shared_ptr<gfx::vulkan::Image> compositeImage;
    std::shared_ptr<gfx::vulkan::Framebuffer> compositeFramebuffer;
    std::shared_ptr<gfx::vulkan::descriptor_set_layout_t> compositeSetLayout0;
    std::shared_ptr<gfx::vulkan::Pipeline> compositePipeline;
    std::vector<std::shared_ptr<gfx::vulkan::descriptor_set_t>> compositeSet0;
    std::vector<std::shared_ptr<gfx::vulkan::buffer_t>> compositeSet0UniformBuffer;
    std::vector<void *> compositeSet0UniformBufferMap;
    std::shared_ptr<gfx::vulkan::Timer> compositeTimer;  

    std::shared_ptr<gfx::vulkan::descriptor_set_layout_t> swapChainPipelineDescriptorSetLayout;
    std::shared_ptr<gfx::vulkan::descriptor_set_t> swapChainPipelineDescriptorSet;
    std::shared_ptr<gfx::vulkan::Pipeline> swapChainPipeline;
    std::shared_ptr<gfx::vulkan::Timer> swapChainTimer;  

};

#endif