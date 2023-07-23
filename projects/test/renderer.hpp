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
    std::shared_ptr<gfx::vulkan::renderpass_t> depthRenderPass;
    std::shared_ptr<gfx::vulkan::image_t> depthImage;
    std::shared_ptr<gfx::vulkan::framebuffer_t> depthFramebuffer;
    std::shared_ptr<gfx::vulkan::pipeline_t> depthPipeline;
    std::shared_ptr<gfx::vulkan::gpu_timer_t> depthTimer;

    std::shared_ptr<gfx::vulkan::descriptor_set_layout_t> descriptorSetLayout0;
    std::shared_ptr<gfx::vulkan::descriptor_set_layout_t> descriptorSetLayout1;
    std::vector<std::shared_ptr<gfx::vulkan::descriptor_set_t>> descriptorSet0;
    std::vector<std::shared_ptr<gfx::vulkan::descriptor_set_t>> descriptorSet1;
    std::vector<std::shared_ptr<gfx::vulkan::buffer_t>> set0UniformBuffer;
    std::vector<std::shared_ptr<gfx::vulkan::buffer_t>> set1UniformBuffer;
    std::vector<void *> set0UniformBufferMap;
    std::vector<void *> set1UniformBufferMap;
    std::shared_ptr<gfx::vulkan::renderpass_t> deferredRenderPass;
    std::shared_ptr<gfx::vulkan::image_t> gBufferAlbedoSpecImage;
    std::shared_ptr<gfx::vulkan::image_t> gBufferNormalImage;
    std::shared_ptr<gfx::vulkan::image_t> gBufferDepthImage;
    std::shared_ptr<gfx::vulkan::framebuffer_t> gBufferFramebuffer;
    std::shared_ptr<gfx::vulkan::pipeline_t> deferredPipeline;
    std::shared_ptr<gfx::vulkan::gpu_timer_t> deferredTimer;  

    std::shared_ptr<gfx::vulkan::descriptor_set_layout_t> shadowDescriptorSetLayout0;
    std::vector<std::shared_ptr<gfx::vulkan::descriptor_set_t>> shadowDescriptorSet0;
    std::vector<std::shared_ptr<gfx::vulkan::buffer_t>> shadowSet0UniformBuffer;
    std::vector<void *> shadowSet0UniformBufferMap;
    std::shared_ptr<gfx::vulkan::renderpass_t> shadowRenderPass;
    std::shared_ptr<gfx::vulkan::image_t> shadowMapImage;
    std::shared_ptr<gfx::vulkan::framebuffer_t> shadowFramebuffer;
    std::shared_ptr<gfx::vulkan::pipeline_t> shadowPipeline;
    std::shared_ptr<gfx::vulkan::gpu_timer_t> shadowTimer;  

    std::shared_ptr<gfx::vulkan::renderpass_t> ssaoRenderPass;
    std::shared_ptr<gfx::vulkan::descriptor_set_layout_t> ssaoDescriptorSetLayout0;
    std::shared_ptr<gfx::vulkan::image_t> ssaoImage;
    std::shared_ptr<gfx::vulkan::image_t> noise;
    std::shared_ptr<gfx::vulkan::framebuffer_t> ssaoFramebuffer;
    std::shared_ptr<gfx::vulkan::pipeline_t> ssaoPipeline;
    std::vector<std::shared_ptr<gfx::vulkan::descriptor_set_t>> ssaoSet0;
    std::vector<std::shared_ptr<gfx::vulkan::buffer_t>> ssaoSet0UniformBuffer;
    std::vector<void *> ssaoSet0UniformBufferMap;
    std::shared_ptr<gfx::vulkan::gpu_timer_t> ssaoTimer;  

    std::shared_ptr<gfx::vulkan::renderpass_t> compositeRenderPass;
    std::shared_ptr<gfx::vulkan::image_t> compositeImage;
    std::shared_ptr<gfx::vulkan::framebuffer_t> compositeFramebuffer;
    std::shared_ptr<gfx::vulkan::descriptor_set_layout_t> compositeSetLayout0;
    std::shared_ptr<gfx::vulkan::pipeline_t> compositePipeline;
    std::vector<std::shared_ptr<gfx::vulkan::descriptor_set_t>> compositeSet0;
    std::vector<std::shared_ptr<gfx::vulkan::buffer_t>> compositeSet0UniformBuffer;
    std::vector<void *> compositeSet0UniformBufferMap;
    std::shared_ptr<gfx::vulkan::gpu_timer_t> compositeTimer;  

    std::shared_ptr<gfx::vulkan::descriptor_set_layout_t> swapChainPipelineDescriptorSetLayout;
    std::shared_ptr<gfx::vulkan::descriptor_set_t> swapChainPipelineDescriptorSet;
    std::shared_ptr<gfx::vulkan::pipeline_t> swapChainPipeline;
    std::shared_ptr<gfx::vulkan::gpu_timer_t> swapChainTimer;  

};

#endif