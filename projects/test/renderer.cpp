#include "renderer.hpp"

#include "core/log.hpp"
#include "core/mesh.hpp"
#include "core/material.hpp"
#include "core/model.hpp"
#include "core/components.hpp"

#include "ubos.hpp"

#include <entt/entt.hpp>

Renderer::Renderer(std::shared_ptr<core::window_t> window, std::shared_ptr<gfx::vulkan::context_t> context, std::shared_ptr<event::Dispatcher> dispatcher)
  : window(window), dispatcher(dispatcher) {
    Renderer::context = context;

	auto [width, height] = window->get_dimensions();

	depthDescriptorSetLayout0 = gfx::vulkan::descriptor_set_layout_builder_t{}
		.addLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT)
		.build(context);
	
	for (int i = 0; i < context->MAX_FRAMES_IN_FLIGHT; i++) {
		depthDescriptorSet0.push_back(gfx::vulkan::descriptor_set_builder_t{}
			.build(context, depthDescriptorSetLayout0));
		depthSet0UniformBuffer.push_back(gfx::vulkan::buffer_builder_t{}
			.build(context, sizeof(DepthSet0UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
		depthSet0UniformBufferMap.push_back(depthSet0UniformBuffer[i]->map());

		depthDescriptorSet0[i]->write()
			.pushBufferInfo(0, 1, VkDescriptorBufferInfo{
				.buffer = depthSet0UniformBuffer[i]->buffer(),
				.offset = 0,
				.range = sizeof(DepthSet0UBO)
			})
			.update();
	}

	depthRenderPass = gfx::vulkan::renderpass_builder_t{}
		.set_depth_attachment(VkAttachmentDescription{
			.format = VK_FORMAT_D32_SFLOAT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
		})
		.build(context);
	
	depthImage = gfx::vulkan::image_builder_t{}
		.build2D(context, width, height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	depthFramebuffer = gfx::vulkan::framebuffer_builder_t{}
		.add_attachment_view(depthImage->image_view())
		.build(context, depthRenderPass->renderpass(), width, height);
	
	depthPipeline = gfx::vulkan::pipeline_builder_t{}
		.add_shader("../../assets/shaders/depth/base.vert.spv")
		.add_shader("../../assets/shaders/depth/base.frag.spv")
		.add_descriptor_set_layout(depthDescriptorSetLayout0)
		.add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
		.add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
		.add_vertex_input_binding_description(0, sizeof(core::Vertex), VK_VERTEX_INPUT_RATE_VERTEX)
		.add_vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::Vertex, position))
		.build(context, depthRenderPass->renderpass());

	depthTimer = std::make_shared<gfx::vulkan::gpu_timer_t>(context);
	
	descriptorSetLayout0 = gfx::vulkan::descriptor_set_layout_builder_t{}
		.addLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS)
		.build(context);

	descriptorSetLayout1 = gfx::vulkan::descriptor_set_layout_builder_t{}
		.addLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS)
		.build(context);

	for (uint32_t i = 0; i < context->MAX_FRAMES_IN_FLIGHT; i++) {
		set0UniformBuffer.push_back(gfx::vulkan::buffer_builder_t{}
			.build(context, sizeof(Set0UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
		set0UniformBufferMap.push_back(set0UniformBuffer[i]->map());

		set1UniformBuffer.push_back(gfx::vulkan::buffer_builder_t{}
			.build(context, sizeof(Set1UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
		set1UniformBufferMap.push_back(set1UniformBuffer[i]->map());

		descriptorSet0.push_back(gfx::vulkan::descriptor_set_builder_t{}	
			.build(context, descriptorSetLayout0));
		
		descriptorSet0[i]->write()
			.pushBufferInfo(0, 1, VkDescriptorBufferInfo{
				.buffer = set0UniformBuffer[i]->buffer(),
				.offset = 0,
				.range = sizeof(Set0UBO)
			})
			.update();

		descriptorSet1.push_back(gfx::vulkan::descriptor_set_builder_t{}
			.build(context, descriptorSetLayout1));

		descriptorSet1[i]->write()
			.pushBufferInfo(0, 1, VkDescriptorBufferInfo{
				.buffer = set1UniformBuffer[i]->buffer(),
				.offset = 0,
				.range = sizeof(Set1UBO)
			})
			.update();
	}

	deferredRenderPass = gfx::vulkan::renderpass_builder_t{}	
		// albedo spec
		.add_color_attachment(VkAttachmentDescription{
			.format = VK_FORMAT_R8G8B8A8_SRGB,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
		})
		// normal
		.add_color_attachment(VkAttachmentDescription{
			.format = VK_FORMAT_R16G16B16A16_SNORM,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
		})
		// depth
		.set_depth_attachment(VkAttachmentDescription{
            .format = VK_FORMAT_D32_SFLOAT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
        })
		.build(context);

	deferredPipeline = gfx::vulkan::pipeline_builder_t{}
		.add_shader("../../assets/shaders/deferred/base.vert.spv")
		.add_shader("../../assets/shaders/deferred/base.frag.spv")
		.add_descriptor_set_layout(descriptorSetLayout0)
		.add_descriptor_set_layout(descriptorSetLayout1)
		.add_descriptor_set_layout(core::Material::getMaterialDescriptorSetLayout())
		.add_default_color_blend_attachment_state()
		.add_default_color_blend_attachment_state()
		.set_depth_stencil_state(VkPipelineDepthStencilStateCreateInfo{
			// sType
			// pNext
			// flags
			// depthTestEnable
			// depthWriteEnable
			// depthCompareOp
			// depthBoundsTestEnable
			// stencilTestEnable
			// front
			// back
			// minDepthBounds
			// maxDepthBounds
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = VK_TRUE,
			.depthWriteEnable = VK_TRUE,
			.depthCompareOp = VK_COMPARE_OP_EQUAL,
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable = VK_FALSE,
			.front = {},
			.back = {},
			.minDepthBounds = 0,
			.maxDepthBounds = 1,
		})
		.add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
		.add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
		.add_vertex_input_binding_description(0, sizeof(core::Vertex), VK_VERTEX_INPUT_RATE_VERTEX)
		.add_vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::Vertex, position))
		.add_vertex_input_attribute_description(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::Vertex, normal))
		.add_vertex_input_attribute_description(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(core::Vertex, uv))
		.add_vertex_input_attribute_description(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::Vertex, tangent))
		.add_vertex_input_attribute_description(0, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::Vertex, biTangent))
		.build(context, deferredRenderPass->renderpass());

	gBufferAlbedoSpecImage = gfx::vulkan::image_builder_t{}
		.build2D(context, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	
	gBufferNormalImage = gfx::vulkan::image_builder_t{}
		.build2D(context, width, height, VK_FORMAT_R16G16B16A16_SNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	
	// gBufferDepthImage = gfx::vulkan::Image::Builder{}
	// 	.build2D(context, width, height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	gBufferFramebuffer = gfx::vulkan::framebuffer_builder_t{}
		.add_attachment_view(gBufferAlbedoSpecImage->image_view())
		.add_attachment_view(gBufferNormalImage->image_view())
		.add_attachment_view(depthImage->image_view())
		.build(context, deferredRenderPass->renderpass(), width, height);

	deferredTimer = std::make_shared<gfx::vulkan::gpu_timer_t>(context);

	shadowDescriptorSetLayout0 = gfx::vulkan::descriptor_set_layout_builder_t{}
		.addLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS)
		.build(context);

	for (int i = 0; i < context->MAX_FRAMES_IN_FLIGHT; i++) {
		shadowDescriptorSet0.push_back(gfx::vulkan::descriptor_set_builder_t{}
			.build(context, shadowDescriptorSetLayout0));
		shadowSet0UniformBuffer.push_back(gfx::vulkan::buffer_builder_t{}
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

	shadowRenderPass = gfx::vulkan::renderpass_builder_t{}
		.set_depth_attachment(VkAttachmentDescription{
			.format = VK_FORMAT_D32_SFLOAT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
		})
		.build(context);
	
	shadowMapImage = gfx::vulkan::image_builder_t{}
		.set_compare_op(VK_COMPARE_OP_LESS)
		.build2D(context, 1024, 1024, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	shadowFramebuffer = gfx::vulkan::framebuffer_builder_t{}
		.add_attachment_view(shadowMapImage->image_view())
		.build(context, shadowRenderPass->renderpass(), 1024, 1024);

	shadowPipeline = gfx::vulkan::pipeline_builder_t{}
		.add_shader("../../assets/shaders/shadow/base.vert.spv")
		.add_shader("../../assets/shaders/shadow/base.frag.spv")
		.add_descriptor_set_layout(shadowDescriptorSetLayout0)
		.add_descriptor_set_layout(descriptorSetLayout1)
		.add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
		.add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
		.add_vertex_input_binding_description(0, sizeof(core::Vertex), VK_VERTEX_INPUT_RATE_VERTEX)
		.add_vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::Vertex, position))
		.build(context, shadowRenderPass->renderpass());

	shadowTimer = std::make_shared<gfx::vulkan::gpu_timer_t>(context);

	ssaoRenderPass = gfx::vulkan::renderpass_builder_t{}
		.add_color_attachment(VkAttachmentDescription{
			.format = VK_FORMAT_R8_UNORM,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
		})
		.build(context);

	ssaoDescriptorSetLayout0 = gfx::vulkan::descriptor_set_layout_builder_t{}
		.addLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.addLayoutBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.addLayoutBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.addLayoutBinding(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(context);
	
	ssaoTimer = std::make_shared<gfx::vulkan::gpu_timer_t>(context);

	noise = gfx::vulkan::image_builder_t{}
		.loadFromPath(context, "../../assets/textures/noise.jpg");

	ssaoImage = gfx::vulkan::image_builder_t{}
		.build2D(context, width, height, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	ssaoFramebuffer = gfx::vulkan::framebuffer_builder_t{}
		.add_attachment_view(ssaoImage->image_view())
		.build(context, ssaoRenderPass->renderpass(), width, height);

	ssaoPipeline = gfx::vulkan::pipeline_builder_t{}
		.add_shader("../../assets/shaders/ssao/base.vert.spv")
		.add_shader("../../assets/shaders/ssao/base.frag.spv")
		.add_default_color_blend_attachment_state()
		.add_descriptor_set_layout(ssaoDescriptorSetLayout0)
		.add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
		.add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
		.build(context, ssaoRenderPass->renderpass());

	for (int i = 0; i < context->MAX_FRAMES_IN_FLIGHT; i++) {
		ssaoSet0.push_back(gfx::vulkan::descriptor_set_builder_t{}
			.build(context, ssaoDescriptorSetLayout0));
		
		ssaoSet0UniformBuffer.push_back(gfx::vulkan::buffer_builder_t{}
			.build(context, sizeof(SsaoSet0UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
		
		ssaoSet0UniformBufferMap.push_back(ssaoSet0UniformBuffer[i]->map());

		ssaoSet0[i]->write()
			.pushImageInfo(0, 1, VkDescriptorImageInfo{
				// .sampler = gBufferDepthImage->sampler(),
				.sampler = depthImage->sampler(),
				.imageView = depthImage->image_view(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushImageInfo(1, 1, VkDescriptorImageInfo{
				.sampler = gBufferNormalImage->sampler(),
				.imageView = gBufferNormalImage->image_view(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushImageInfo(2, 1, VkDescriptorImageInfo{
				.sampler = noise->sampler(),
				.imageView = noise->image_view(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushBufferInfo(3, 1, VkDescriptorBufferInfo{
				.buffer = ssaoSet0UniformBuffer[i]->buffer(),
				.offset = 0,
				.range = sizeof(SsaoSet0UBO)
			})
			.update();
	}

	compositeRenderPass = gfx::vulkan::renderpass_builder_t{}
		.add_color_attachment(VkAttachmentDescription{
			.format = VK_FORMAT_R8G8B8A8_SRGB,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
		})
		.build(context);

	compositeImage = gfx::vulkan::image_builder_t{}
		.build2D(context, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	compositeFramebuffer = gfx::vulkan::framebuffer_builder_t{}
		.add_attachment_view(compositeImage->image_view())
		.build(context, compositeRenderPass->renderpass(), width, height);
	
	compositeSetLayout0 = gfx::vulkan::descriptor_set_layout_builder_t{}
		.addLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.addLayoutBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.addLayoutBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.addLayoutBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.addLayoutBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.addLayoutBinding(5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(context);

	for (int i = 0; i < context->MAX_FRAMES_IN_FLIGHT; i++) {
		compositeSet0.push_back(gfx::vulkan::descriptor_set_builder_t{}
			.build(context, compositeSetLayout0));
		
		compositeSet0UniformBuffer.push_back(gfx::vulkan::buffer_builder_t{}
			.build(context, sizeof(CompositeSet0UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT  | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
		
		compositeSet0UniformBufferMap.push_back(compositeSet0UniformBuffer[i]->map());

		compositeSet0[i]->write()
			.pushImageInfo(0, 1, VkDescriptorImageInfo{
				.sampler = gBufferAlbedoSpecImage->sampler(),
				.imageView = gBufferAlbedoSpecImage->image_view(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushImageInfo(1, 1, VkDescriptorImageInfo{
				.sampler = gBufferNormalImage->sampler(),
				.imageView = gBufferNormalImage->image_view(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushImageInfo(2, 1, VkDescriptorImageInfo{
				// .sampler = gBufferDepthImage->sampler(),
				.sampler = depthImage->sampler(),
				.imageView = depthImage->image_view(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushImageInfo(3, 1, VkDescriptorImageInfo{
				.sampler = shadowMapImage->sampler(),
				.imageView = shadowMapImage->image_view(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushImageInfo(4, 1, VkDescriptorImageInfo{
				.sampler = ssaoImage->sampler(),
				.imageView = ssaoImage->image_view(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushBufferInfo(5, 1, VkDescriptorBufferInfo{
				.buffer = compositeSet0UniformBuffer[i]->buffer(),
				.offset = 0,
				.range = sizeof(CompositeSet0UBO)
			})
			.update();
	}

	compositePipeline = gfx::vulkan::pipeline_builder_t{}
		.add_shader("../../assets/shaders/composite/base.vert.spv")
		.add_shader("../../assets/shaders/composite/base.frag.spv")
		.add_default_color_blend_attachment_state()
		.add_descriptor_set_layout(compositeSetLayout0)
		.add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
		.add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
		.build(context, compositeRenderPass->renderpass());

	compositeTimer = std::make_shared<gfx::vulkan::gpu_timer_t>(context);

	swapChainPipelineDescriptorSetLayout = gfx::vulkan::descriptor_set_layout_builder_t{}
		.addLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(context);
	
	swapChainPipelineDescriptorSet = gfx::vulkan::descriptor_set_builder_t{}
		.build(context, swapChainPipelineDescriptorSetLayout);
	swapChainPipelineDescriptorSet->write()
		.pushImageInfo(0, 1, VkDescriptorImageInfo{
			.sampler = compositeImage->sampler(),
			.imageView = compositeImage->image_view(),
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		})
		.update();

	swapChainPipeline = gfx::vulkan::pipeline_builder_t{}
		.add_shader("../../assets/shaders/swapchain/base.vert.spv")
		.add_shader("../../assets/shaders/swapchain/base.frag.spv")
		.add_default_color_blend_attachment_state()
		.add_descriptor_set_layout(swapChainPipelineDescriptorSetLayout)
		.add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
		.add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
		.build(context, context->swapchain_renderpass());	
	
	swapChainTimer = std::make_shared<gfx::vulkan::gpu_timer_t>(context);

    INFO("Created Renderer");
}

Renderer::~Renderer() {
	
	for (int i = 0; i < context->MAX_FRAMES_IN_FLIGHT; i++) {
		depthSet0UniformBuffer[i]->unmap();
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

	auto [width, height] = window->get_dimensions();

	depthImage = gfx::vulkan::image_builder_t{}
		.build2D(context, width, height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	depthFramebuffer = gfx::vulkan::framebuffer_builder_t{}
		.add_attachment_view(depthImage->image_view())
		.build(context, depthRenderPass->renderpass(), width, height);

	gBufferAlbedoSpecImage = gfx::vulkan::image_builder_t{}
		.build2D(context, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	
	gBufferNormalImage = gfx::vulkan::image_builder_t{}
		.build2D(context, width, height, VK_FORMAT_R16G16B16A16_SNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	
	// gBufferDepthImage = gfx::vulkan::Image::Builder{}
	// 	.build2D(context, width, height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	gBufferFramebuffer = gfx::vulkan::framebuffer_builder_t{}
		.add_attachment_view(gBufferAlbedoSpecImage->image_view())
		.add_attachment_view(gBufferNormalImage->image_view())
		// .add_attachment_view(gBufferDepthImage->imageView())
		.add_attachment_view(depthImage->image_view())
		.build(context, deferredRenderPass->renderpass(), width, height);

	ssaoImage = gfx::vulkan::image_builder_t{}
		.build2D(context, width, height, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	ssaoFramebuffer = gfx::vulkan::framebuffer_builder_t{}
		.add_attachment_view(ssaoImage->image_view())
		.build(context, ssaoRenderPass->renderpass(), width, height);

	for (int i = 0; i < context->MAX_FRAMES_IN_FLIGHT; i++) {
		ssaoSet0[i]->write()
			.pushImageInfo(0, 1, VkDescriptorImageInfo{
				// .sampler = gBufferDepthImage->sampler(),
				.sampler = depthImage->sampler(),
				.imageView = depthImage->image_view(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushImageInfo(1, 1, VkDescriptorImageInfo{
				.sampler = gBufferNormalImage->sampler(),
				.imageView = gBufferNormalImage->image_view(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushImageInfo(2, 1, VkDescriptorImageInfo{
				.sampler = noise->sampler(),
				.imageView = noise->image_view(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushBufferInfo(3, 1, VkDescriptorBufferInfo{
				.buffer = ssaoSet0UniformBuffer[i]->buffer(),
				.offset = 0,
				.range = sizeof(SsaoSet0UBO)
			})
			.update();
	}

	compositeImage = gfx::vulkan::image_builder_t{}
		.build2D(context, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	compositeFramebuffer = gfx::vulkan::framebuffer_builder_t{}
		.add_attachment_view(compositeImage->image_view())
		.build(context, compositeRenderPass->renderpass(), width, height);

	for (int i = 0; i < context->MAX_FRAMES_IN_FLIGHT; i++) {
		compositeSet0[i]->write()
			.pushImageInfo(0, 1, VkDescriptorImageInfo{
				.sampler = gBufferAlbedoSpecImage->sampler(),
				.imageView = gBufferAlbedoSpecImage->image_view(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushImageInfo(1, 1, VkDescriptorImageInfo{
				.sampler = gBufferNormalImage->sampler(),
				.imageView = gBufferNormalImage->image_view(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushImageInfo(2, 1, VkDescriptorImageInfo{
				// .sampler = gBufferDepthImage->sampler(),
				.sampler = depthImage->sampler(),
				.imageView = depthImage->image_view(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushImageInfo(3, 1, VkDescriptorImageInfo{
				.sampler = shadowMapImage->sampler(),
				.imageView = shadowMapImage->image_view(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			})
			.pushImageInfo(4, 1, VkDescriptorImageInfo{
				.sampler = ssaoImage->sampler(),
				.imageView = ssaoImage->image_view(),
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
			.imageView = compositeImage->image_view(),
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		})
		.update();
}

void Renderer::render(std::shared_ptr<entt::registry> scene, core::CameraComponent& camera) {
	if (auto startFrame = context->start_frame()) {
		auto [commandBuffer, currentFrame] = *startFrame;

		VkClearValue clearColor{};
		clearColor.color = {0, 0, 0, 0};
		VkClearValue clearDepth{};
		clearDepth.depthStencil.depth = 1;
		
		depthTimer->begin(commandBuffer);
		depthRenderPass->begin(commandBuffer, depthFramebuffer->framebuffer(), VkRect2D{
			.offset = {0, 0},
			.extent = context->swapchain_extent()
		}, {
			clearDepth
		});
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(context->swapchain_extent().width);
		viewport.height = static_cast<float>(context->swapchain_extent().height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		VkRect2D scissor{};
		scissor.offset = {0, 0};
		scissor.extent = context->swapchain_extent();
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
		depthPipeline->bind(commandBuffer);
		core::Transform transform{};
		DepthSet0UBO depthSet0UBO{};
		depthSet0UBO.model = transform.mat4();
		depthSet0UBO.projection = camera.getProjection();
		depthSet0UBO.view = camera.getView();
		std::memcpy(depthSet0UniformBufferMap[context->current_frame()], &depthSet0UBO, sizeof(DepthSet0UBO));
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, depthPipeline->pipeline_layout(), 0, 1, &depthDescriptorSet0[context->current_frame()]->descriptor_set(), 0, nullptr);

		for (auto [ent, model] : scene->view<core::Model>().each()) {
			model.draw(commandBuffer, depthPipeline->pipeline_layout(), false);
		}
		depthTimer->end(commandBuffer);
		if (auto t = depthTimer->get_time()) {
			INFO("Depth Timer: {}", *t);
		}
		depthRenderPass->end(commandBuffer);

		deferredTimer->begin(commandBuffer);
		deferredRenderPass->begin(commandBuffer, gBufferFramebuffer->framebuffer(), VkRect2D{
			.offset = {0, 0},
			.extent = context->swapchain_extent()
		}, {
			clearColor,
			clearColor,
			clearDepth
		});
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);	
		deferredPipeline->bind(commandBuffer);
		
		Set0UBO set0UBO{};
		set0UBO.projection = camera.getProjection();
		set0UBO.view = camera.getView();
		
		Set1UBO set1UBO{};
		set1UBO.model = transform.mat4();
		set1UBO.invModelT = glm::transpose(glm::inverse(glm::mat3{set1UBO.model}));

		std::memcpy(set0UniformBufferMap[context->current_frame()], &set0UBO, sizeof(Set0UBO));
		std::memcpy(set1UniformBufferMap[context->current_frame()], &set1UBO, sizeof(Set1UBO));

		VkDescriptorSet sets[] = { descriptorSet0[context->current_frame()]->descriptor_set(), descriptorSet1[context->current_frame()]->descriptor_set() };

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, deferredPipeline->pipeline_layout(), 0, 2, sets, 0, nullptr);
		for (auto [ent, model] : scene->view<core::Model>().each()) {
			model.draw(commandBuffer, deferredPipeline->pipeline_layout());
		}
		deferredTimer->end(commandBuffer);
		if (auto t = deferredTimer->get_time()) {
			INFO("Deferred Timer: {}", *t);
		}
		deferredRenderPass->end(commandBuffer);

		shadowTimer->begin(commandBuffer);
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
		std::memcpy(shadowSet0UniformBufferMap[context->current_frame()], &shadowSet0UBO, sizeof(ShadowSet0UBO));
		{
			VkDescriptorSet sets[] = { shadowDescriptorSet0[context->current_frame()]->descriptor_set(), descriptorSet1[context->current_frame()]->descriptor_set() };
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline->pipeline_layout(), 0, 2, sets, 0, nullptr);
		}
		for (auto [ent, model] : scene->view<core::Model>().each()) {
			model.draw(commandBuffer, shadowPipeline->pipeline_layout(), false);
		}
		shadowTimer->end(commandBuffer);
		if (auto t = shadowTimer->get_time()) {
			INFO("Shadow Timer: {}", *t);
		}
		shadowRenderPass->end(commandBuffer);

		ssaoTimer->begin(commandBuffer);
		ssaoRenderPass->begin(commandBuffer, ssaoFramebuffer->framebuffer(), VkRect2D{
			.offset = {0, 0},
			.extent = context->swapchain_extent()
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
		std::memcpy(ssaoSet0UniformBufferMap[context->current_frame()], &ssaoSet0UBO, sizeof(SsaoSet0UBO));
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ssaoPipeline->pipeline_layout(), 0, 1, &ssaoSet0[context->current_frame()]->descriptor_set(), 0, nullptr);
		vkCmdDraw(commandBuffer, 6, 1, 0, 0);
		ssaoTimer->end(commandBuffer);
		if (auto t = ssaoTimer->get_time()) {
			INFO("SSAO Timer: {}", *t);
		}
		ssaoRenderPass->end(commandBuffer);

		compositeTimer->begin(commandBuffer);
		compositeRenderPass->begin(commandBuffer, compositeFramebuffer->framebuffer(), VkRect2D{
			.offset = {0, 0},
			.extent = context->swapchain_extent()
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
		std::memcpy(compositeSet0UniformBufferMap[context->current_frame()], &compositeSet0UBO, sizeof(CompositeSet0UBO));
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipeline->pipeline_layout(), 0, 1, &compositeSet0[context->current_frame()]->descriptor_set(), 0, nullptr);
		vkCmdDraw(commandBuffer, 6, 1, 0, 0);
		compositeTimer->end(commandBuffer);
		if (auto t = compositeTimer->get_time()) {
			INFO("Composite Timer: {}", *t);
		}
		compositeRenderPass->end(commandBuffer);

		swapChainTimer->begin(commandBuffer);
		context->begin_swapchain_renderpass(commandBuffer, clearColor);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, swapChainPipeline->pipeline_layout(), 0, 1, &swapChainPipelineDescriptorSet->descriptor_set(), 0, nullptr);
		swapChainPipeline->bind(commandBuffer);
		vkCmdDraw(commandBuffer, 6, 1, 0, 0);
		swapChainTimer->end(commandBuffer);
		if (auto t = swapChainTimer->get_time()) {
			INFO("SwapChain Timer: {}", *t);
		}
		context->end_swapchain_renderpass(commandBuffer);

		if (context->end_frame(commandBuffer))
			recreateDimentionDependentResources();		
	} else {
		recreateDimentionDependentResources();
	}
}