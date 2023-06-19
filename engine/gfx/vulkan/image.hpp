#ifndef GFX_VULKAN_IMAGE_HPP
#define GFX_VULKAN_IMAGE_HPP

#include "context.hpp"
#include "buffer.hpp"

#include <filesystem>

namespace gfx {

namespace vulkan {

class Image {
public:
    struct Builder {
        Builder& mipMaps();
        // setting compare op enables compare
        Builder& setCompareOp(VkCompareOp compareOp);
        std::shared_ptr<Image> build2D(std::shared_ptr<Context> context, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags imageUsageFlags, VkMemoryPropertyFlags memoryTypeIndex);
        std::shared_ptr<Image> loadFromPath(std::shared_ptr<Context> context, const std::filesystem::path& filePath, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);

        bool enableMipMaps{false};
        bool enableCompareOp{false};
        VkCompareOp compareOp = VK_COMPARE_OP_ALWAYS;

    private:
        VkImageAspectFlags getImageAspect(VkFormat format);
    };

    struct ImageInfo {
        VkImage image{};
        VkDeviceMemory deviceMemory{};
        VkFormat format{};
        VkImageLayout currentLayout{};
        VkImageView imageView{};
        VkSampler sampler{};
        uint32_t width, height;
        uint32_t mipLevels{};
    };

    Image(std::shared_ptr<Context> context, const ImageInfo& imageInfo);
    ~Image();

    // TODO: remove single time command from transition layout and make it take in a command buffer instead
    void transitionLayout(VkImageLayout newLayout);
    void transitionLayout(VkCommandBuffer commandBuffer, VkImageLayout newLayout);
    void genMipMaps();

    static void copyBufferToImage(std::shared_ptr<Context> context, Buffer& buffer, Image& image, VkBufferImageCopy bufferImageCopy);

    VkImage& image() { return m_imageInfo.image; }
    VkImageView& imageView() { return m_imageInfo.imageView; }
    VkSampler& sampler() { return m_imageInfo.sampler; }
    VkFormat& format() { return m_imageInfo.format; }
    VkImageLayout& imageLayout() { return m_imageInfo.currentLayout; }

    friend class RenderPass;

private:
    std::shared_ptr<Context> m_context{};
    ImageInfo m_imageInfo{};
};

} // namespace vulkan

} // namespace gfx

#endif