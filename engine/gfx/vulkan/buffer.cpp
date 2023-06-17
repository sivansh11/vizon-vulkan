#include "buffer.hpp"

#include "core/log.hpp"

namespace gfx {

namespace vulkan {

std::shared_ptr<Buffer> Buffer::Builder::build(std::shared_ptr<Context> context, VkDeviceSize size, VkBufferUsageFlags bufferUsageFlags, VkMemoryPropertyFlags memoryTypeIndex) {
    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = bufferUsageFlags;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    if (vkCreateBuffer(context->device(), &bufferCreateInfo, nullptr, &buffer) != VK_SUCCESS) {
        ERROR("Failed to create buffer");
        std::terminate();
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(context->device(), buffer, &memoryRequirements);

    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = context->findMemoryType(memoryRequirements.memoryTypeBits, memoryTypeIndex);

    VkDeviceMemory deviceMemory;
    if (vkAllocateMemory(context->device(), &memoryAllocateInfo, nullptr, &deviceMemory) != VK_SUCCESS) {
        ERROR("Failed to allocate device memory");
        std::terminate();
    }

    vkBindBufferMemory(context->device(), buffer, deviceMemory, 0);

    return std::make_shared<Buffer>(context, buffer, deviceMemory);
}

Buffer::Buffer(std::shared_ptr<Context> context, VkBuffer buffer, VkDeviceMemory deviceMemory) 
  : m_context(context),
    m_buffer(buffer),
    m_deviceMemory(deviceMemory) {
    TRACE("Created buffer");
}

Buffer::~Buffer() {
    vkDestroyBuffer(m_context->device(), m_buffer, nullptr);
    vkFreeMemory(m_context->device(), m_deviceMemory, nullptr);
    TRACE("Destoryed buffer");
}

void *Buffer::map(VkDeviceSize offset, VkDeviceSize size) {
    void *data;
    vkMapMemory(m_context->device(), m_deviceMemory, offset, size, 0, &data);
    return data;
}

void Buffer::unmap() {
    vkUnmapMemory(m_context->device(), m_deviceMemory);
}

void Buffer::copy(std::shared_ptr<Context> context, Buffer& srcBuffer, Buffer& dstBuffer, const VkBufferCopy& bufferCopy) {
    VkCommandBuffer commandBuffer = context->startSingleUseCommandBuffer();

    vkCmdCopyBuffer(commandBuffer, srcBuffer.buffer(), dstBuffer.buffer(), 1, &bufferCopy);

    context->endSingleUseCommandBuffer(commandBuffer);
}

} // namespace vulkan

} // namespace gfx
