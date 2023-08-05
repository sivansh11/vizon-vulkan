#ifndef GFX_VULKAN_IMAGE_HPP
#define GFX_VULKAN_IMAGE_HPP

#include "context.hpp"
#include "buffer.hpp"

#include <filesystem>

namespace gfx {

namespace vulkan {

class image_t;

struct image_builder_t {
    image_builder_t& mip_maps();
    image_builder_t& set_tiling(VkImageTiling image_tiling);
    image_builder_t& set_initial_layout(VkImageLayout image_layout);
    // setting compare op enables compare
    image_builder_t& set_compare_op(VkCompareOp compare_op);
    core::ref<image_t> build2D(core::ref<context_t> context, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags image_usage_flags, VkMemoryPropertyFlags memory_type_index);
    core::ref<image_t> loadFromPath(core::ref<context_t> context, const std::filesystem::path& file_path, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);

    bool enable_mip_maps{false};
    bool enable_compare_op{false};
    VkCompareOp compare_op = VK_COMPARE_OP_ALWAYS;
    VkImageTiling image_tiling = VK_IMAGE_TILING_OPTIMAL;
    VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;

private:
    VkImageAspectFlags get_image_aspect(VkFormat format);
};

struct image_info_t {
    VkImage image{};
    VkDeviceMemory device_memory{};
    VkFormat format{};
    VkImageLayout current_layout{};
    VkImageView image_view{};
    VkSampler sampler{};
    VkDeviceSize size{};
    uint32_t width, height;
    uint32_t mip_levels{};
};

class image_t {
public:
    image_t(core::ref<context_t> context, const image_info_t& image_info);
    ~image_t();

    VkDescriptorImageInfo descriptor_info(VkImageLayout image_layout) {
        return VkDescriptorImageInfo{
            .sampler = sampler(),
            .imageView = image_view(),
            .imageLayout = image_layout
        };
    }

    void transition_layout(VkImageLayout old_layout, VkImageLayout new_layout);
    void transition_layout(VkCommandBuffer commandbuffer, VkImageLayout old_layout, VkImageLayout new_layout);
    void genMipMaps(VkCommandBuffer commandbuffer, VkImageLayout old_layout, VkImageLayout new_layout);
    void genMipMaps(VkImageLayout old_layout, VkImageLayout new_layout);

    static void copy_buffer_to_image(core::ref<context_t> context, buffer_t& buffer, image_t& image, VkImageLayout image_layout, VkBufferImageCopy buffer_image_copy);
    static void copy_buffer_to_image(VkCommandBuffer commandbuffer, buffer_t& buffer, image_t& image, VkImageLayout image_layout, VkBufferImageCopy buffer_image_copy);

    void *map(VkDeviceSize poffset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    void unmap();

    // use flush after writing (NOTE: only required when not VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    void flush(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    // use invalidate before reading (NOTE: only required when not VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    void invalidate(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    

    VkImage& image() { return _imageInfo.image; }
    VkImageView& image_view() { return _imageInfo.image_view; }
    VkSampler& sampler() { return _imageInfo.sampler; }
    VkFormat& format() { return _imageInfo.format; }
    VkImageLayout& image_layout() { return _imageInfo.current_layout; }
    VkDeviceSize size() { return _imageInfo.size; }
    VkImageLayout& current_layout() { return _imageInfo.current_layout; }
    std::pair<uint32_t, uint32_t> dimensions() { return {_imageInfo.width, _imageInfo.height}; }

    friend class renderpass_t;

private:
    core::ref<context_t> _context{};
    image_info_t _imageInfo{};
};

} // namespace vulkan

} // namespace gfx

#endif