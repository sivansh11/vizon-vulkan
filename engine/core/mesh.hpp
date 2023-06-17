#ifndef CORE_MESH_HPP
#define CORE_MESH_HPP

#include "core/material.hpp"
#include "core/components.hpp"

#include "gfx/vulkan/context.hpp"
#include "gfx/vulkan/pipeline.hpp"
#include "gfx/vulkan/descriptor.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "gfx/vulkan/image.hpp"
#include "gfx/vulkan/renderpass.hpp"
#include "gfx/vulkan/framebuffer.hpp"

#include <glm/glm.hpp>
#include <entt/entt.hpp>

namespace core {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec3 tangent;
    glm::vec3 biTangent;
};

class Mesh {
public:
    Mesh(std::shared_ptr<gfx::vulkan::Context> context, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    ~Mesh();

    void draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, bool withMaterial);

    std::shared_ptr<Material> material;

    friend class Model;
private:
    std::shared_ptr<gfx::vulkan::Buffer> m_vertices;
    std::shared_ptr<gfx::vulkan::Buffer> m_indices;
    uint32_t m_indexCount;
    uint32_t m_vertexCount;
    core::Transform m_transform;
};

} // namespace core

#endif