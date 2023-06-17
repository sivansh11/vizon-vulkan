#include "framebuffer.hpp"

#include "core/log.hpp"

namespace gfx {

namespace vulkan {

Framebuffer::Builder& Framebuffer::Builder::addAttachmentView(VkImageView view) {
    attachments.push_back(view);
    return *this;
}

std::shared_ptr<Framebuffer> Framebuffer::Builder::build(std::shared_ptr<Context> context, VkRenderPass renderPass, uint32_t width, uint32_t height) {
    VkFramebufferCreateInfo framebufferCreateInfo{};
    framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferCreateInfo.renderPass = renderPass;
    framebufferCreateInfo.attachmentCount = attachments.size();
    framebufferCreateInfo.pAttachments = attachments.data();
    framebufferCreateInfo.width = width;
    framebufferCreateInfo.height = height;
    framebufferCreateInfo.layers = 1;

    VkFramebuffer framebuffer{};

    if (vkCreateFramebuffer(context->device(), &framebufferCreateInfo, nullptr, &framebuffer) != VK_SUCCESS) {
        ERROR("Failed to create framebuffer");
        std::terminate();
    }

    return std::make_shared<Framebuffer>(context, framebuffer);
}

Framebuffer::Framebuffer(std::shared_ptr<Context> context, VkFramebuffer framebuffer) 
  : m_context(context), m_framebuffer(framebuffer) {
    TRACE("Created framebuffer");
}

Framebuffer::~Framebuffer() {
    vkDestroyFramebuffer(m_context->device(), m_framebuffer, nullptr);
    TRACE("Destroyed framebuffer");
}

} // namespace vulkan

} // namespace gfx