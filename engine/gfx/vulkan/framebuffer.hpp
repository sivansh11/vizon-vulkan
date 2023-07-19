#ifndef GFX_VULKAN_FRAMEBUFFER_HPP
#define GFX_VULKAN_FRAMEBUFFER_HPP

#include "context.hpp"
#include "renderpass.hpp"

namespace gfx {

namespace vulkan {

class Framebuffer {
public:
    struct Builder {
        Builder& addAttachmentView(VkImageView view);
        core::ref<Framebuffer> build(core::ref<Context> context, VkRenderPass renderPass, uint32_t width, uint32_t height);

        std::vector<VkImageView> attachments{};
    };

    Framebuffer(core::ref<Context> context, VkFramebuffer framebuffer);
    ~Framebuffer();

    VkFramebuffer& framebuffer() { return m_framebuffer; }
    
private:
    core::ref<Context> m_context;
    VkFramebuffer m_framebuffer;
};

} // namespace vulkan

} // namespace gfx

#endif