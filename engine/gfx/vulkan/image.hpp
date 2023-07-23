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
        Builder& setTiling(VkImageTiling imageTiling);
        Builder& setInitialLayout(VkImageLayout imageLayout);
        // setting compare op enables compare
        Builder& setCompareOp(VkCompareOp compareOp);
        core::ref<Image> build2D(core::ref<context_t> context, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags imageUsageFlags, VkMemoryPropertyFlags memoryTypeIndex);
        core::ref<Image> loadFromPath(core::ref<context_t> context, const std::filesystem::path& filePath, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);

        bool enableMipMaps{false};
        bool enableCompareOp{false};
        VkCompareOp compareOp = VK_COMPARE_OP_ALWAYS;
        VkImageTiling imageTiling = VK_IMAGE_TILING_OPTIMAL;
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

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
        VkDeviceSize size{};
        uint32_t width, height;
        uint32_t mipLevels{};
    };

    Image(core::ref<context_t> context, const ImageInfo& imageInfo);
    ~Image();

    VkDescriptorImageInfo descriptorInfo(VkImageLayout imageLayout) {
        return VkDescriptorImageInfo{
            .sampler = sampler(),
            .imageView = imageView(),
            .imageLayout = imageLayout
        };
    }

    // TODO: remove single time command from transition layout and make it take in a command buffer instead
    void transitionLayout(VkImageLayout oldLayout, VkImageLayout newLayout);
    void transitionLayout(VkCommandBuffer commandBuffer, VkImageLayout oldLayout, VkImageLayout newLayout);
    void genMipMaps(VkCommandBuffer commandBuffer, VkImageLayout oldLayout, VkImageLayout newLayout);
    void genMipMaps(VkImageLayout oldLayout, VkImageLayout newLayout);

    static void copyBufferToImage(core::ref<context_t> context, buffer_t& buffer, Image& image, VkBufferImageCopy bufferImageCopy);
    static void copyBufferToImage(VkCommandBuffer commandBuffer, buffer_t& buffer, Image& image, VkBufferImageCopy bufferImageCopy);

    void *map(VkDeviceSize poffset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    void unmap();

    // use flush after writing (NOTE: only required when not VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    void flush(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    // use invalidate before reading (NOTE: only required when not VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    void invalidate(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    

    VkImage& image() { return m_imageInfo.image; }
    VkImageView& imageView() { return m_imageInfo.imageView; }
    VkSampler& sampler() { return m_imageInfo.sampler; }
    VkFormat& format() { return m_imageInfo.format; }
    VkImageLayout& imageLayout() { return m_imageInfo.currentLayout; }
    VkDeviceSize size() { return m_imageInfo.size; }
    VkImageLayout& currentLayout() { return m_imageInfo.currentLayout; }
    std::pair<uint32_t, uint32_t> dimensions() { return {m_imageInfo.width, m_imageInfo.height}; }

    friend class RenderPass;

private:
    core::ref<context_t> m_context{};
    ImageInfo m_imageInfo{};
};

} // namespace vulkan

} // namespace gfx

#endif