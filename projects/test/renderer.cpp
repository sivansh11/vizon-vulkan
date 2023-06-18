#include "renderer.hpp"

#include "core/log.hpp"
#include "core/mesh.hpp"
#include "core/material.hpp"
#include "core/model.hpp"
#include "core/components.hpp"

#include "ubos.hpp"

#include <entt/entt.hpp>

Renderer::Renderer(std::shared_ptr<core::Window> window, std::shared_ptr<gfx::vulkan::Context> context, std::shared_ptr<event::Dispatcher> dispatcher)
  : window(window), dispatcher(dispatcher) {
    Renderer::context = context;

	auto [width, height] = window->getSize();

	descriptorSetLayout0 = gfx::vulkan::DescriptorSetLayout::Builder{}
		.addLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS)
		.build(context);

	descriptorSetLayout1 = gfx::vulkan::DescriptorSetLayout::Builder{}
		.addLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS)
		.build(context);

	for (uint32_t i = 0; i < context->MAX_FRAMES_IN_FLIGHT; i++) {
		set0UniformBuffer.push_back(gfx::vulkan::Buffer::Builder{}
			.build(context, sizeof(Set0UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
		set0UniformBufferMap.push_back(set0UniformBuffer[i]->map());

		set1UniformBuffer.push_back(gfx::vulkan::Buffer::Builder{}
			.build(context, sizeof(Set1UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
		set1UniformBufferMap.push_back(set1UniformBuffer[i]->map());

		descriptorSet0.push_back(gfx::vulkan::DescriptorSet::Builder{}	
			.build(context, descriptorSetLayout0));
		
		descriptorSet0[i]->write()
			.pushBufferInfo(0, 1, VkDescriptorBufferInfo{
				.buffer = set0UniformBuffer[i]->buffer(),
				.offset = 0,
				.range = sizeof(Set0UBO)
			})
			.update();

		descriptorSet1.push_back(gfx::vulkan::DescriptorSet::Builder{}
			.build(context, descriptorSetLayout1));

		descriptorSet1[i]->write()
			.pushBufferInfo(0, 1, VkDescriptorBufferInfo{
				.buffer = set1UniformBuffer[i]->buffer(),
				.offset = 0,
				.range = sizeof(Set1UBO)
			})
			.update();
	}

	deferredRenderPass = gfx::vulkan::RenderPass::Builder{}	
		// albedo spec
		.addColorAttachment(VkAttachmentDescription{
			.format = VK_FORMAT_R8G8B8A8_SRGB,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
		})
		// normal
		.addColorAttachment(VkAttachmentDescription{
			.format = VK_FORMAT_R16G16B16A16_SNORM,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
		})
		// depth
		.setDepthAttachment(VkAttachmentDescription{
            .format = VK_FORMAT_D32_SFLOAT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
        })
		.build(context);

	deferredPipeline = gfx::vulkan::Pipeline::Builder{}
		.addShader("../../assets/shaders/deferred/base.vert.spv")
		.addShader("../../assets/shaders/deferred/base.frag.spv")
		.addDescriptorSetLayout(descriptorSetLayout0)
		.addDescriptorSetLayout(descriptorSetLayout1)
		.addDescriptorSetLayout(core::Material::getMaterialDescriptorSetLayout())
		.addDefaultColorBlendAttachmentState()
		.addDefaultColorBlendAttachmentState()
		.addDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
		.addDynamicState(VK_DYNAMIC_STATE_SCISSOR)
		.addVertexInputBindingDescription(0, sizeof(core::Vertex), VK_VERTEX_INPUT_RATE_VERTEX)
		.addVertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::Vertex, position))
		.addVertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::Vertex, normal))
		.addVertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(core::Vertex, uv))
		.addVertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::Vertex, tangent))
		.addVertexInputAttributeDescription(0, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::Vertex, biTangent))
		.build(context, deferredRenderPass->renderPass());

	gBufferAlbedoSpecImage = gfx::vulkan::Image::Builder{}
		.build2D(context, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	
	gBufferNormalImage = gfx::vulkan::Image::Builder{}
		.build2D(context, width, height, VK_FORMAT_R16G16B16A16_SNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	
	gBufferDepthImage = gfx::vulkan::Image::Builder{}
		.build2D(context, width, height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	gBufferFramebuffer = gfx::vulkan::Framebuffer::Builder{}
		.addAttachmentView(gBufferAlbedoSpecImage->imageView())
		.addAttachmentView(gBufferNormalImage->imageView())
		.addAttachmentView(gBufferDepthImage->imageView())
		.build(context, deferredRenderPass->renderPass(), width, height);

	shadowDescriptorSetLayout0 = gfx::vulkan::DescriptorSetLayout::Builder{}
		.addLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS)
		.build(context);

	for (int i = 0; i < context->MAX_FRAMES_IN_FLIGHT; i++) {
		shadowDescriptorSet0.push_back(gfx::vulkan::DescriptorSet::Builder{}
			.build(context, shadowDescriptorSetLayout0));
		shadowSet0UniformBuffer.push_back(gfx::vulkan::Buffer::Builder{}
			.build(context, sizeof(ShadowSet0UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
		shadowSet0UniformBufferMap.push_back(shadowSet0UniformBuffer[i]->map());	
		shadowDescriptorSet0[i]->write()
			.pushBufferInfo(0, 1, VkDescriptorBufferInfo{
				.buffer = shadowSet0UniformBuffer[i]->buffer(),
				.offset = 0,
				.range = sizeof(ShadowSet0UBO)
			})
			.update();
	}

	shadowRenderPass = gfx::vulkan::RenderPass::Builder{}
		.setDepthAttachment(VkAttachmentDescription{
			.format = VK_FORMAT_D32_SFLOAT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
		})
		.build(context);
	
	shadowMapImage = gfx::vulkan::Image::Builder{}
		.setCompareOp(VK_COMPARE_OP_LESS)
		.build2D(context, 1024, 1024, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	shadowFramebuffer = gfx::vulkan::Framebuffer::Builder{}
		.addAttachmentView(shadowMapImage->imageView())
		.build(context, shadowRenderPass->renderPass(), 1024, 1024);

	shadowPipeline = gfx::vulkan::Pipeline::Builder{}
		.addShader("../../assets/shaders/shadow/base.vert.spv")
		.addShader("../../assets/shaders/shadow/base.frag.spv")
		.addDescriptorSetLayout(shadowDescriptorSetLayout0)
		.addDescriptorSetLayout(descriptorSetLayout1)
		.addDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
		.addDynamicState(VK_DYNAMIC_STATE_SCISSOR)
		.addVertexInputBindingDescription(0, sizeof(core::Vertex), VK_VERTEX_INPUT_RATE_VERTEX)
		.addVertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::Vertex, position))
		.build(context, shadowRenderPass->renderPass());

	ssaoRenderPass = gfx::vulkan::RenderPass::Builder{}
		.addColorAttachment(VkAttachmentDescription{
			.format = VK_FORMAT_R8_UNORM,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
		})
		.build(context);

	ssaoDescriptorSetLayout0 = gfx::vulkan::DescriptorSetLayout::Builder{}
		.addLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.addLayoutBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.addLayoutBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.addLayoutBinding(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(context);

	noise = gfx::vulkan::Image::Builder{}
		.loadFromPath(context, "../../assets/textures/noise.jpg");

	ssaoImage = gfx::vulkan::Image::Builder{}
		.build2D(context, width, height, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	ssaoFramebuffer = gfx::vulkan::Framebuffer::Builder{}
		.addAttachmentView(ssaoImage->imageView())
		.build(context, ssaoRenderPass->renderPass(), width, height);

	ssaoPipeline = gfx::vulkan::Pipeline::Builder{}
		.addShader("../../assets/shaders/ssao/base.vert.spv")
		.addShader("../../assets/shaders/ssao/base.frag.spv")
		.addDefaultColorBlendAttachmentState()
		.addDescriptorSetLayout(ssaoDescriptorSetLayout0)
		.addDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
		.addDynamicState(VK_DYNAMIC_STATE_SCISSOR)
		.build(context, ssaoRenderPass->renderPass());

	for (int i = 0; i < context->MAX_FRAMES_IN_FLIGHT; i++) {
		ssaoSet0.push_back(gfx::vulkan::DescriptorSet::Builder{}
			.build(context, ssaoDescriptorSetLayout0));
		
		ssaoSet0UniformBuffer.push_back(gfx::vulkan::Buffer::Builder{}
			.build(context, sizeof(SsaoSet0UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
		
		ssaoSet0UniformBufferMap.push_back(ssaoSet0UniformBuffer[i]->map());

		ssaoSet0[i]->write()
			.pushImageInfo(0, 1, VkDescriptorImageInfo{
				.sampler = gBufferDepthImage->sampler(),
				.imageView = gBufferDepthImage->imageView(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushImageInfo(1, 1, VkDescriptorImageInfo{
				.sampler = gBufferNormalImage->sampler(),
				.imageView = gBufferNormalImage->imageView(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushImageInfo(2, 1, VkDescriptorImageInfo{
				.sampler = noise->sampler(),
				.imageView = noise->imageView(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushBufferInfo(3, 1, VkDescriptorBufferInfo{
				.buffer = ssaoSet0UniformBuffer[i]->buffer(),
				.offset = 0,
				.range = sizeof(SsaoSet0UBO)
			})
			.update();
	}

	compositeRenderPass = gfx::vulkan::RenderPass::Builder{}
		.addColorAttachment(VkAttachmentDescription{
			.format = VK_FORMAT_R8G8B8A8_SRGB,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
		})
		.build(context);

	compositeImage = gfx::vulkan::Image::Builder{}
		.build2D(context, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	compositeFramebuffer = gfx::vulkan::Framebuffer::Builder{}
		.addAttachmentView(compositeImage->imageView())
		.build(context, compositeRenderPass->renderPass(), width, height);
	
	compositeSetLayout0 = gfx::vulkan::DescriptorSetLayout::Builder{}
		.addLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.addLayoutBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.addLayoutBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.addLayoutBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.addLayoutBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.addLayoutBinding(5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(context);

	for (int i = 0; i < context->MAX_FRAMES_IN_FLIGHT; i++) {
		compositeSet0.push_back(gfx::vulkan::DescriptorSet::Builder{}
			.build(context, compositeSetLayout0));
		
		compositeSet0UniformBuffer.push_back(gfx::vulkan::Buffer::Builder{}
			.build(context, sizeof(CompositeSet0UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT  | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
		
		compositeSet0UniformBufferMap.push_back(compositeSet0UniformBuffer[i]->map());

		compositeSet0[i]->write()
			.pushImageInfo(0, 1, VkDescriptorImageInfo{
				.sampler = gBufferAlbedoSpecImage->sampler(),
				.imageView = gBufferAlbedoSpecImage->imageView(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushImageInfo(1, 1, VkDescriptorImageInfo{
				.sampler = gBufferNormalImage->sampler(),
				.imageView = gBufferNormalImage->imageView(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushImageInfo(2, 1, VkDescriptorImageInfo{
				.sampler = gBufferDepthImage->sampler(),
				.imageView = gBufferDepthImage->imageView(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushImageInfo(3, 1, VkDescriptorImageInfo{
				.sampler = shadowMapImage->sampler(),
				.imageView = shadowMapImage->imageView(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushImageInfo(4, 1, VkDescriptorImageInfo{
				.sampler = ssaoImage->sampler(),
				.imageView = ssaoImage->imageView(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushBufferInfo(5, 1, VkDescriptorBufferInfo{
				.buffer = compositeSet0UniformBuffer[i]->buffer(),
				.offset = 0,
				.range = sizeof(CompositeSet0UBO)
			})
			.update();
	}

	compositePipeline = gfx::vulkan::Pipeline::Builder{}
		.addShader("../../assets/shaders/composite/base.vert.spv")
		.addShader("../../assets/shaders/composite/base.frag.spv")
		.addDefaultColorBlendAttachmentState()
		.addDescriptorSetLayout(compositeSetLayout0)
		.addDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
		.addDynamicState(VK_DYNAMIC_STATE_SCISSOR)
		.build(context, compositeRenderPass->renderPass());

	swapChainPipelineDescriptorSetLayout = gfx::vulkan::DescriptorSetLayout::Builder{}
		.addLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(context);
	
	swapChainPipelineDescriptorSet = gfx::vulkan::DescriptorSet::Builder{}
		.build(context, swapChainPipelineDescriptorSetLayout);
	swapChainPipelineDescriptorSet->write()
		.pushImageInfo(0, 1, VkDescriptorImageInfo{
			.sampler = compositeImage->sampler(),
			.imageView = compositeImage->imageView(),
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		})
		.update();

	swapChainPipeline = gfx::vulkan::Pipeline::Builder{}
		.addShader("../../assets/shaders/swapchain/base.vert.spv")
		.addShader("../../assets/shaders/swapchain/base.frag.spv")
		.addDefaultColorBlendAttachmentState()
		.addDescriptorSetLayout(swapChainPipelineDescriptorSetLayout)
		.addDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
		.addDynamicState(VK_DYNAMIC_STATE_SCISSOR)
		.build(context, context->swapChainRenderPass());	
	
    INFO("Created Renderer");
}

Renderer::~Renderer() {
	
	for (int i = 0; i < context->MAX_FRAMES_IN_FLIGHT; i++) {
		set0UniformBuffer[i]->unmap();
		set1UniformBuffer[i]->unmap();
		shadowSet0UniformBuffer[i]->unmap();
		compositeSet0UniformBuffer[i]->unmap();
		ssaoSet0UniformBuffer[i]->unmap();
	}	
    INFO("Destroyed Renderer");
}

void Renderer::recreateDimentionDependentResources() {
	vkDeviceWaitIdle(context->device());

	auto [width, height] = window->getSize();

	gBufferAlbedoSpecImage = gfx::vulkan::Image::Builder{}
		.build2D(context, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	
	gBufferNormalImage = gfx::vulkan::Image::Builder{}
		.build2D(context, width, height, VK_FORMAT_R16G16B16A16_SNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	
	gBufferDepthImage = gfx::vulkan::Image::Builder{}
		.build2D(context, width, height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	gBufferFramebuffer = gfx::vulkan::Framebuffer::Builder{}
		.addAttachmentView(gBufferAlbedoSpecImage->imageView())
		.addAttachmentView(gBufferNormalImage->imageView())
		.addAttachmentView(gBufferDepthImage->imageView())
		.build(context, deferredRenderPass->renderPass(), width, height);

	ssaoImage = gfx::vulkan::Image::Builder{}
		.build2D(context, width, height, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	ssaoFramebuffer = gfx::vulkan::Framebuffer::Builder{}
		.addAttachmentView(ssaoImage->imageView())
		.build(context, ssaoRenderPass->renderPass(), width, height);

	for (int i = 0; i < context->MAX_FRAMES_IN_FLIGHT; i++) {
		ssaoSet0[i]->write()
			.pushImageInfo(0, 1, VkDescriptorImageInfo{
				.sampler = gBufferDepthImage->sampler(),
				.imageView = gBufferDepthImage->imageView(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushImageInfo(1, 1, VkDescriptorImageInfo{
				.sampler = gBufferNormalImage->sampler(),
				.imageView = gBufferNormalImage->imageView(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushImageInfo(2, 1, VkDescriptorImageInfo{
				.sampler = noise->sampler(),
				.imageView = noise->imageView(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushBufferInfo(3, 1, VkDescriptorBufferInfo{
				.buffer = ssaoSet0UniformBuffer[i]->buffer(),
				.offset = 0,
				.range = sizeof(SsaoSet0UBO)
			})
			.update();
	}

	compositeImage = gfx::vulkan::Image::Builder{}
		.build2D(context, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	compositeFramebuffer = gfx::vulkan::Framebuffer::Builder{}
		.addAttachmentView(compositeImage->imageView())
		.build(context, compositeRenderPass->renderPass(), width, height);

	for (int i = 0; i < context->MAX_FRAMES_IN_FLIGHT; i++) {
		compositeSet0[i]->write()
			.pushImageInfo(0, 1, VkDescriptorImageInfo{
				.sampler = gBufferAlbedoSpecImage->sampler(),
				.imageView = gBufferAlbedoSpecImage->imageView(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushImageInfo(1, 1, VkDescriptorImageInfo{
				.sampler = gBufferNormalImage->sampler(),
				.imageView = gBufferNormalImage->imageView(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushImageInfo(2, 1, VkDescriptorImageInfo{
				.sampler = gBufferDepthImage->sampler(),
				.imageView = gBufferDepthImage->imageView(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushImageInfo(3, 1, VkDescriptorImageInfo{
				.sampler = shadowMapImage->sampler(),
				.imageView = shadowMapImage->imageView(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushImageInfo(4, 1, VkDescriptorImageInfo{
				.sampler = ssaoImage->sampler(),
				.imageView = ssaoImage->imageView(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushBufferInfo(5, 1, VkDescriptorBufferInfo{
				.buffer = compositeSet0UniformBuffer[i]->buffer(),
				.offset = 0,
				.range = sizeof(CompositeSet0UBO)
			})
			.update();
	}

	swapChainPipelineDescriptorSet->write()
		.pushImageInfo(0, 1, VkDescriptorImageInfo{
			.sampler = compositeImage->sampler(),
			.imageView = compositeImage->imageView(),
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		})
		.update();
}

void Renderer::render(std::shared_ptr<entt::registry> scene, core::CameraComponent& camera) {
	if (auto startFrame = context->startFrame()) {
		auto [commandBuffer, imageIndex] = *startFrame;

		VkClearValue clearColor{};
		clearColor.color = {0, 0, 0, 0};
		VkClearValue clearDepth{};
		clearDepth.depthStencil.depth = 1;
		
		deferredRenderPass->begin(commandBuffer, gBufferFramebuffer->framebuffer(), VkRect2D{
			.offset = {0, 0},
			.extent = context->swapChainExtent()
		}, {
			clearColor,
			clearColor,
			clearDepth
		});

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(context->swapChainExtent().width);
		viewport.height = static_cast<float>(context->swapChainExtent().height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		VkRect2D scissor{};
		scissor.offset = {0, 0};
		scissor.extent = context->swapChainExtent();
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
		deferredPipeline->bind(commandBuffer);
		
		Set0UBO set0UBO{};
		set0UBO.projection = camera.getProjection();
		set0UBO.view = camera.getView();
		
		Set1UBO set1UBO{};
		core::Transform transform{};
		set1UBO.model = transform.mat4();
		set1UBO.invModelT = glm::transpose(glm::inverse(glm::mat3{set1UBO.model}));

		std::memcpy(set0UniformBufferMap[context->currentFrame()], &set0UBO, sizeof(Set0UBO));
		std::memcpy(set1UniformBufferMap[context->currentFrame()], &set1UBO, sizeof(Set1UBO));

		VkDescriptorSet sets[] = { descriptorSet0[context->currentFrame()]->descriptorSet(), descriptorSet1[context->currentFrame()]->descriptorSet() };

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, deferredPipeline->pipelineLayout(), 0, 2, sets, 0, nullptr);
		for (auto [ent, model] : scene->view<core::Model>().each()) {
			model.draw(commandBuffer, deferredPipeline->pipelineLayout());
		}
		deferredRenderPass->end(commandBuffer);

		shadowRenderPass->begin(commandBuffer, shadowFramebuffer->framebuffer(), VkRect2D{
			.offset = {0, 0},
			.extent = {1024, 1024}
		}, {
			clearDepth
		});
		VkViewport shadowViewport{};
		shadowViewport.x = 0.0f;
		shadowViewport.y = 0.0f;
		shadowViewport.width = 1024;
		shadowViewport.height = 1024;
		shadowViewport.minDepth = 0.0f;
		shadowViewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &shadowViewport);
		VkRect2D shadowScissor{};
		shadowScissor.offset = {0, 0};
		shadowScissor.extent = {1024, 1024};
		vkCmdSetScissor(commandBuffer, 0, 1, &shadowScissor);
		shadowPipeline->bind(commandBuffer);
		ShadowSet0UBO shadowSet0UBO{};
		// dlc.position = {.01, 12, 6};
        // dlc.color = {.8, .8, .8};
        // dlc.ambience = {.4, .4, .4};
        // dlc.term = {.3, .3, .1};
        // dlc.multiplier = 2;
        // dlc.orthoProj = 15;
        // dlc.far = 43;
        // dlc.near = 0.1;
		// glm::mat4 lightProjection = glm::ortho(-dlc.orthoProj, dlc.orthoProj, -dlc.orthoProj, dlc.orthoProj, dlc.near, dlc.far);  
		// glm::mat4 lightView = glm::lookAt(dlc.position * dlc.multiplier, 
		// 					glm::vec3( 0.0f, 0.0f,  0.0f), 
		// 					glm::vec3( 0.0f, 1.0f,  0.0f));  
		// glm::mat4 lightSpace = lightProjection * lightView;
		glm::mat4 lightProjection = glm::ortho(-15.f, 15.f, -15.f, 15.f, 0.1f, 43.f);  
		glm::mat4 lightView = glm::lookAt(glm::vec3{.01, 12, 6} * 3.f, 
								glm::vec3( 0.0f, 0.0f,  0.0f), 
								glm::vec3( 0.0f, 1.0f,  0.0f));  
		glm::mat4 lightSpace = lightProjection * lightView;
		shadowSet0UBO.lightSpace = lightSpace;
		std::memcpy(shadowSet0UniformBufferMap[context->currentFrame()], &shadowSet0UBO, sizeof(ShadowSet0UBO));
		{
			VkDescriptorSet sets[] = { shadowDescriptorSet0[context->currentFrame()]->descriptorSet(), descriptorSet1[context->currentFrame()]->descriptorSet() };
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline->pipelineLayout(), 0, 2, sets, 0, nullptr);
		}
		for (auto [ent, model] : scene->view<core::Model>().each()) {
			model.draw(commandBuffer, shadowPipeline->pipelineLayout(), false);
		}
		shadowRenderPass->end(commandBuffer);

		ssaoRenderPass->begin(commandBuffer, ssaoFramebuffer->framebuffer(), VkRect2D{
			.offset = {0, 0},
			.extent = context->swapChainExtent()
		}, {
			clearColor,
			clearColor,
			clearDepth
		});

		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
		ssaoPipeline->bind(commandBuffer);
		SsaoSet0UBO ssaoSet0UBO{};
		ssaoSet0UBO.view = camera.getView();
		ssaoSet0UBO.projection = camera.getProjection();
		ssaoSet0UBO.invView = glm::inverse(ssaoSet0UBO.view);
		ssaoSet0UBO.invProjection = glm::inverse(ssaoSet0UBO.projection);
		ssaoSet0UBO.radius = 0.1;
		ssaoSet0UBO.bias = 0.025;
		std::memcpy(ssaoSet0UniformBufferMap[context->currentFrame()], &ssaoSet0UBO, sizeof(SsaoSet0UBO));
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ssaoPipeline->pipelineLayout(), 0, 1, &ssaoSet0[context->currentFrame()]->descriptorSet(), 0, nullptr);
		vkCmdDraw(commandBuffer, 6, 1, 0, 0);
		ssaoRenderPass->end(commandBuffer);

		compositeRenderPass->begin(commandBuffer, compositeFramebuffer->framebuffer(), VkRect2D{
			.offset = {0, 0},
			.extent = context->swapChainExtent()
		}, {
			clearColor,
			clearColor,
			clearDepth
		});

		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
		compositePipeline->bind(commandBuffer);
		CompositeSet0UBO compositeSet0UBO{};
		compositeSet0UBO.lightSpace = lightSpace;
		compositeSet0UBO.view = camera.getView();
		compositeSet0UBO.projection = camera.getProjection();
		compositeSet0UBO.invView = glm::inverse(compositeSet0UBO.view);
		compositeSet0UBO.invProjection = glm::inverse(compositeSet0UBO.projection);
		DirectionalLight directionalLight{};
		directionalLight.ambience = {0.3, 0.3, 0.3};
		compositeSet0UBO.directionalLight = directionalLight;
		std::memcpy(compositeSet0UniformBufferMap[context->currentFrame()], &compositeSet0UBO, sizeof(CompositeSet0UBO));
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipeline->pipelineLayout(), 0, 1, &compositeSet0[context->currentFrame()]->descriptorSet(), 0, nullptr);
		vkCmdDraw(commandBuffer, 6, 1, 0, 0);
		compositeRenderPass->end(commandBuffer);

		context->beginSwapChainRenderPass(commandBuffer, clearColor);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, swapChainPipeline->pipelineLayout(), 0, 1, &swapChainPipelineDescriptorSet->descriptorSet(), 0, nullptr);
		swapChainPipeline->bind(commandBuffer);
		vkCmdDraw(commandBuffer, 6, 1, 0, 0);
		context->endSwapChainRenderPass(commandBuffer);

		if (context->endFrame(commandBuffer))
			recreateDimentionDependentResources();		
	} else {
		recreateDimentionDependentResources();
	}
}