#include "buffer.hpp"

#include "core/log.hpp"

namespace gfx {

namespace vulkan {

core::ref<Buffer> Buffer::Builder::build(core::ref<Context> context, VkDeviceSize size, VkBufferUsageFlags bufferUsageFlags, VkMemoryPropertyFlags memoryTypeIndex) {
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

    VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .pNext = NULL,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
        .deviceMask = 0};


    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = context->findMemoryType(memoryRequirements.memoryTypeBits, memoryTypeIndex);
    memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;

    VkDeviceMemory deviceMemory;
    if (vkAllocateMemory(context->device(), &memoryAllocateInfo, nullptr, &deviceMemory) != VK_SUCCESS) {
        ERROR("Failed to allocate device memory");
        std::terminate();
    }

    vkBindBufferMemory(context->device(), buffer, deviceMemory, 0);

    return core::make_ref<Buffer>(context, buffer, deviceMemory);
}

Buffer::Buffer(core::ref<Context> context, VkBuffer buffer, VkDeviceMemory deviceMemory) 
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

void Buffer::invalidate(VkDeviceSize offset, VkDeviceSize size) {
    VkMappedMemoryRange mappedMemoryRange{};
    mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedMemoryRange.memory = m_deviceMemory;
    mappedMemoryRange.offset = offset;
    mappedMemoryRange.size = size;
    vkInvalidateMappedMemoryRanges(m_context->device(), 1, &mappedMemoryRange);
}

void Buffer::flush(VkDeviceSize offset, VkDeviceSize size) {
    VkMappedMemoryRange mappedMemoryRange{};
    mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedMemoryRange.memory = m_deviceMemory;
    mappedMemoryRange.offset = offset;
    mappedMemoryRange.size = size;
    vkFlushMappedMemoryRanges(m_context->device(), 1, &mappedMemoryRange);
}

void *Buffer::map(VkDeviceSize offset, VkDeviceSize size) {
    if (m_mapped) return m_mapped;
    vkMapMemory(m_context->device(), m_deviceMemory, offset, size, 0, &m_mapped);
    return m_mapped;
}

void Buffer::unmap() {
    if (!m_mapped) return;
    vkUnmapMemory(m_context->device(), m_deviceMemory);
}

void Buffer::copy(core::ref<Context> context, Buffer& srcBuffer, Buffer& dstBuffer, const VkBufferCopy& bufferCopy) {
    VkCommandBuffer commandBuffer = context->startSingleUseCommandBuffer();

    vkCmdCopyBuffer(commandBuffer, srcBuffer.buffer(), dstBuffer.buffer(), 1, &bufferCopy);

    context->endSingleUseCommandBuffer(commandBuffer);
}

} // namespace vulkan

} // namespace gfx
