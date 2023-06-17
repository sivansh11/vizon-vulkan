#ifndef CORE_MODEL_HPP
#define CORE_MODEL_HPP

#include "core/components.hpp"
#include "core/mesh.hpp"
#include "core/material.hpp"

#include "gfx/vulkan/context.hpp"
#include "gfx/vulkan/image.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <filesystem>
#include <unordered_map>

namespace core {

class Model {
public:
    Model(std::shared_ptr<gfx::vulkan::Context> context);
    ~Model();

    void loadFromPath(const std::filesystem::path& filePath);

    std::vector<std::shared_ptr<core::Mesh>> meshes;

    void draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, bool withMaterial = true);

private:
    void processNode(aiNode *node, const aiScene *scene, aiMatrix4x4& transform);
    std::shared_ptr<core::Mesh> processMesh(aiMesh *mesh, const aiScene *scene, aiMatrix4x4& transform);
    std::shared_ptr<core::Material> processMaterial(aiMaterial *material);
    std::shared_ptr<gfx::vulkan::Image> loadMaterialTexture(aiMaterial *mat, aiTextureType type, std::string typeName);

private:
    std::filesystem::path m_filePath, m_directory;
    std::shared_ptr<gfx::vulkan::Context> m_context;
    std::unordered_map<std::string, std::shared_ptr<gfx::vulkan::Image>> m_loadedImages;
};

} // namespace core

#endif