#include "renderpass.hpp"

#include "core/log.hpp"

namespace gfx {

namespace vulkan {

RenderPass::Builder& RenderPass::Builder::addColorAttachment(const VkAttachmentDescription& attachmentDescription) {
    attachmentDescriptions.push_back(attachmentDescription);
    VkAttachmentReference attachmentRefrence{};
    attachmentRefrence.attachment = counter++;
    attachmentRefrence.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentsRefrences.push_back(attachmentRefrence);
    return *this;
}

RenderPass::Builder& RenderPass::Builder::setDepthAttachment(const VkAttachmentDescription& attachmentDescription) {
    if (depthAdded) {
        ERROR("Depth already added");
        std::terminate();
    } 
    attachmentDescriptions.push_back(attachmentDescription);
    VkAttachmentReference attachmentRefrence{};
    attachmentRefrence.attachment = counter++;
    attachmentRefrence.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachmentRefrence = attachmentRefrence;
    depthAdded = true;
    return *this;
}

core::ref<RenderPass> RenderPass::Builder::build(core::ref<Context> context) {
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = colorAttachmentsRefrences.size();
    subpass.pColorAttachments = colorAttachmentsRefrences.data();
    if (depthAdded) 
        subpass.pDepthStencilAttachment = &depthAttachmentRefrence;
    
    VkSubpassDependency dependencies[2]{};
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[0].dstStageMask |= colorAttachmentsRefrences.size() > 0 ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : 0;
	dependencies[0].dstStageMask |= depthAdded ? VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT : 0;
	dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[0].dstAccessMask = colorAttachmentsRefrences.size() > 0 ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : 0;
	dependencies[0].dstAccessMask = depthAdded ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : 0;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    
    dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = dependencies[0].dstStageMask;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].srcAccessMask = dependencies[0].dstAccessMask;
	dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassCreateInfo{};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = attachmentDescriptions.size();
    renderPassCreateInfo.pAttachments = attachmentDescriptions.data();
    renderPassCreateInfo.subpassCount = 1;              // hard coded
    renderPassCreateInfo.pSubpasses = &subpass;         // hard coded
    renderPassCreateInfo.dependencyCount = 2;           // hard coded
    renderPassCreateInfo.pDependencies = dependencies;  // hard coded

    VkRenderPass renderPass{};

    if (vkCreateRenderPass(context->device(), &renderPassCreateInfo, nullptr, &renderPass) != VK_SUCCESS) {
        ERROR("Failed to create renderpass");
        std::terminate();
    }

    return core::make_ref<RenderPass>(context, renderPass);
}

RenderPass::RenderPass(core::ref<Context> context, VkRenderPass renderPass)
  : m_context(context), m_renderPass(renderPass) {
    TRACE("Created renderpass");
}

RenderPass::~RenderPass() {
    vkDestroyRenderPass(m_context->device(), m_renderPass, nullptr);
    TRACE("Destroyed renderpass");
}

void RenderPass::begin(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer, const VkRect2D renderArea, const std::vector<VkClearValue>& clearValues) {
    VkRenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.framebuffer = framebuffer;
    renderPassBeginInfo.renderPass = m_renderPass;
    renderPassBeginInfo.renderArea = renderArea;
    renderPassBeginInfo.clearValueCount = clearValues.size();
    renderPassBeginInfo.pClearValues = clearValues.data();
    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);  // hardcoded subpass contents inline
}

void RenderPass::end(VkCommandBuffer commandBuffer) {
    vkCmdEndRenderPass(commandBuffer);
}

} // namespace vulkan

} // namespace gfx
