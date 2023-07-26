#ifndef CORE_MODEL_HPP
#define CORE_MODEL_HPP

#include "core/log.hpp"
#include "core/components.hpp"
#include "core/mesh.hpp"
#include "core/material.hpp"

#include "gfx/vulkan/context.hpp"
#include "gfx/vulkan/image.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <filesystem>
#include <optional>
#include <unordered_map>

namespace core {

struct vertex_t {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec3 tangent;
    glm::vec3 bi_tangent;
};

enum texture_type_t {
    e_diffuse_map,
    e_specular_map,
    e_normal_map,
    e_diffuse_color,
};

struct texture_info_t {
    texture_type_t texture_type;
    std::filesystem::path file_path;
    glm::vec4 diffuse_color;
};

struct material_t {
    std::vector<texture_info_t> texture_infos;
};

struct mesh_t {
    std::vector<vertex_t> vertices;
    std::vector<uint32_t> indices; 
    material_t material;
};

struct model_t {
    std::vector<mesh_t> meshes;
};

struct model_loading_info_t {
    std::filesystem::path file_path;
    model_t model;
};

std::optional<texture_info_t> process_texture(model_loading_info_t& model_loading_info, aiMaterial *material, aiTextureType type, texture_type_t texture_type);

material_t process_material(model_loading_info_t& model_loading_info, aiMaterial *material);

mesh_t process_mesh(model_loading_info_t& model_loading_info, aiMesh *mesh, const aiScene *scene);

void process_node(model_loading_info_t& model_loading_info, aiNode *node, const aiScene *scene);

model_t load_model_from_path(const std::filesystem::path& file_path);

class Model {
public:
    Model(core::ref<gfx::vulkan::context_t> context);
    ~Model();

    void loadFromPath(const std::filesystem::path& filePath);

    std::vector<core::ref<core::Mesh>> meshes;

    void draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, bool withMaterial = true);

private:
    void processNode(aiNode *node, const aiScene *scene, aiMatrix4x4& transform);
    core::ref<core::Mesh> processMesh(aiMesh *mesh, const aiScene *scene, aiMatrix4x4& transform);
    core::ref<core::Material> processMaterial(aiMaterial *material);
    core::ref<gfx::vulkan::image_t> loadMaterialTexture(aiMaterial *mat, aiTextureType type, std::string typeName);

private:
    std::filesystem::path m_filePath, m_directory;
    core::ref<gfx::vulkan::context_t> m_context;
    std::unordered_map<std::string, core::ref<gfx::vulkan::image_t>> m_loadedImages;
};

} // namespace core

#endif