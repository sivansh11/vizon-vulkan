#ifndef GFX_VULKAN_BUFFER_HPP
#define GFX_VULKAN_BUFFER_HPP

#include "context.hpp"

#include <limits>

namespace gfx {

namespace vulkan {

class Buffer {
public:
    struct Builder {
        std::shared_ptr<Buffer> build(std::shared_ptr<Context> context, VkDeviceSize size, VkBufferUsageFlags bufferUsageFlags, VkMemoryPropertyFlags memoryTypeIndex);
    };  

    Buffer(std::shared_ptr<Context> context, VkBuffer buffer, VkDeviceMemory deviceMemory);
    ~Buffer();

    void *map(VkDeviceSize offset = 0, VkDeviceSize size = std::numeric_limits<uint64_t>::max());
    void unmap();

    // use flush after writing (NOTE: only required when not VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    void flush();
    // use invalidate before reading (NOTE: only required when not VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    void invalidate();
    
    static void copy(std::shared_ptr<Context> context, Buffer& srcBuffer, Buffer& dstBuffer, const VkBufferCopy& bufferCopy);

    VkBuffer& buffer() { return m_buffer; }
    VkDeviceMemory& deviceMemory() { return m_deviceMemory; }

private:
    std::shared_ptr<Context> m_context;
    VkBuffer m_buffer;
    VkDeviceMemory m_deviceMemory;
};

} // namespace vulkan

} // namespace gfx

#endif 