#ifndef GFX_VULKAN_RENDERPASS_HPP
#define GFX_VULKAN_RENDERPASS_HPP

#include  "context.hpp"

namespace gfx {

namespace vulkan {
    
class RenderPass {
public:
    struct Builder {
        Builder& addColorAttachment(const VkAttachmentDescription& attachmentDescription);
        Builder& setDepthAttachment(const VkAttachmentDescription& attachmentDescription);
        core::ref<RenderPass> build(core::ref<Context> context);

        bool depthAdded = false;
        uint32_t counter = 0;
        std::vector<VkAttachmentDescription> attachmentDescriptions{};
        std::vector<VkAttachmentReference> colorAttachmentsRefrences{};
        VkAttachmentReference depthAttachmentRefrence{};
    };

    RenderPass(core::ref<Context> context, VkRenderPass renderPass);
    ~RenderPass();

    void begin(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer, const VkRect2D renderArea, const std::vector<VkClearValue>& clearValues);
    void end(VkCommandBuffer commandBuffer);

    VkRenderPass& renderPass() { return m_renderPass; }

private:
    core::ref<Context> m_context;
    VkRenderPass m_renderPass;
};

} // namespace vulkan

} // namespace gfx

#endif