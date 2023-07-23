#ifndef GFX_VULKAN_BUFFER_HPP
#define GFX_VULKAN_BUFFER_HPP

#include "context.hpp"

#include <limits>

namespace gfx {

namespace vulkan {

class buffer_t {
public:

    buffer_t(core::ref<Context> context, VkBuffer buffer, VkDeviceMemory deviceMemory);
    ~buffer_t();

    VkDescriptorBufferInfo descriptor_info(VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE) {
        return VkDescriptorBufferInfo{
            .buffer = buffer(),
            .offset = offset,
            .range = range
        };
    } 

    void *map(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    void unmap();

    // use flush after writing (NOTE: only required when not VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    void flush(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    void invalidate(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    
    static void copy(core::ref<Context> context, buffer_t& srcBuffer, buffer_t& dstBuffer, const VkBufferCopy& bufferCopy);

    VkBuffer& buffer() { return m_buffer; }
    VkDeviceMemory& device_memory() { return m_deviceMemory; }

private:
    void *m_mapped{nullptr};
    core::ref<Context> m_context;
    VkBuffer m_buffer;
    VkDeviceMemory m_deviceMemory;
};

struct buffer_builder_t {
    core::ref<buffer_t> build(core::ref<Context> context, VkDeviceSize size, VkBufferUsageFlags bufferUsageFlags, VkMemoryPropertyFlags memoryTypeIndex);
};  

} // namespace vulkan

// class buffer_t {
// public:
//     struct buffer_info_t {
//         VkDeviceSize _size /* vulkan buffer size */;
//         VkBufferUsageFlags _buffer_usage /* vulkan buffer usage flags */;
//         VkMemoryPropertyFlags _memory_property /* vulkan device memory property flags */;
//         VkBuffer _buffer /* vulkan buffer handle */;
//         VkDeviceMemory _deviceMemory /* vulkan device memory handle */;
//     };
    
//     static buffer_t make_buffer(core::ref<vulkan::Context> context, VkDeviceSize size, VkBufferUsageFlags buffer_usage, VkMemoryPropertyFlags memory_property);
//     static core::ref<buffer_t> make_buffer_ref(core::ref<vulkan::Context> context, VkDeviceSize size, VkBufferUsageFlags buffer_usage, VkMemoryPropertyFlags memory_property);

//     buffer_t(core::ref<vulkan::Context> context, buffer_info_t buffer_info);
//     ~buffer_t();

// private:
//     core::ref<vulkan::Context> _context;
//     buffer_info_t _buffer_info;
// };

} // namespace gfx

#endif 