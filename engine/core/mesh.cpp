#include "mesh.hpp"

#include <cstring>

namespace core {

Mesh::Mesh(core::ref<gfx::vulkan::Context> context, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    assert(vertices.size() > 0);
    assert(indices.size() > 0);

    m_vertexCount = vertices.size();
    m_indexCount = indices.size();

    m_vertices = gfx::vulkan::Buffer::Builder{}
        .build(context, vertices.size() * sizeof(vertices[0]), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    core::ref<gfx::vulkan::Buffer> stagingBuffer = gfx::vulkan::Buffer::Builder{}
        .build(context, vertices.size() * sizeof(vertices[0]), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    auto map = stagingBuffer->map();
    std::memcpy(map, vertices.data(), vertices.size() * sizeof(vertices[0]));
    stagingBuffer->unmap();

    gfx::vulkan::Buffer::copy(context, *stagingBuffer, *m_vertices, VkBufferCopy{
        .size = vertices.size() * sizeof(vertices[0]),
    });

    m_indices = gfx::vulkan::Buffer::Builder{}
        .build(context, indices.size() * sizeof(indices[0]), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    stagingBuffer = gfx::vulkan::Buffer::Builder{}
        .build(context, indices.size() * sizeof(indices[0]), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    map = stagingBuffer->map();
    std::memcpy(map, indices.data(), indices.size() * sizeof(indices[0]));
    stagingBuffer->unmap();

    gfx::vulkan::Buffer::copy(context, *stagingBuffer, *m_indices, VkBufferCopy{
        .size = indices.size() * sizeof(indices[0]),
    });
}   

Mesh::~Mesh() {

}

void Mesh::draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, bool withMaterial) {
    if (withMaterial)
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1, &material->descriptorSet->descriptorSet(), 0, nullptr);

    VkDeviceSize offsets{0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_vertices->buffer(), &offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_indices->buffer(), offsets, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, m_indexCount, 1, 0, 0, 0);
}

} // namespace core
