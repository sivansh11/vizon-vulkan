#include "model.hpp"

namespace core {    

Model::Model(std::shared_ptr<gfx::vulkan::Context> context) 
  : m_context(context) {

}

Model::~Model() {

}

void Model::loadFromPath(const std::filesystem::path& filePath) {
    meshes.clear();
    m_loadedImages.clear();
    m_filePath = filePath;
    Assimp::Importer importer{};
    const aiScene *scene = importer.ReadFile(filePath.string(),
                                             aiProcess_Triangulate |
                                             aiProcess_GenNormals |
                                             aiProcess_CalcTangentSpace |
                                             aiProcess_PreTransformVertices
                                            );
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        throw std::runtime_error(importer.GetErrorString());
    }   
    // I know this is bad and slow, too lazy to look up the api to directly get the directory 
    m_directory = m_filePath.string().substr(0, m_filePath.string().find_last_of('/'));
    aiMatrix4x4 transform{};
    processNode(scene->mRootNode, scene, transform);
}

void Model::processNode(aiNode *node, const aiScene *scene, aiMatrix4x4 &transform) {
    aiMatrix4x4 localTransform = transform * node->mTransformation;
    for (uint32_t i = 0; i < node->mNumChildren; i++) {
        processNode(node->mChildren[i], scene, localTransform);
    }
    for (uint32_t i = 0; i < node->mNumMeshes; i++) {
        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(processMesh(mesh, scene, localTransform));
    }
}

std::shared_ptr<Mesh> Model::processMesh(aiMesh *mesh, const aiScene *scene, aiMatrix4x4 &transform) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    vertices.reserve(mesh->mNumVertices);
    for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;

        vertex.position.x = mesh->mVertices[i].x;
        vertex.position.y = mesh->mVertices[i].y;
        vertex.position.z = mesh->mVertices[i].z;

        vertex.normal.x = mesh->mNormals[i].x;
        vertex.normal.y = mesh->mNormals[i].y;
        vertex.normal.z = mesh->mNormals[i].z;

        if (mesh->mTextureCoords[0]) {
            vertex.uv.x = mesh->mTextureCoords[0][i].x;
            vertex.uv.y = mesh->mTextureCoords[0][i].y;
        } else {
            vertex.uv = {0, 0};
        }

        if (mesh->HasTangentsAndBitangents()) {
            vertex.tangent.x = mesh->mTangents[i].x;
            vertex.tangent.y = mesh->mTangents[i].y;
            vertex.tangent.z = mesh->mTangents[i].z;
            vertex.biTangent.x = mesh->mBitangents[i].x;
            vertex.biTangent.y = mesh->mBitangents[i].y;
            vertex.biTangent.z = mesh->mBitangents[i].z;
        } else {
            // assert(false);
        }

        vertices.push_back(vertex);
    }

    for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
        aiFace& face = mesh->mFaces[i];
        for (uint32_t j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }

    auto processedMaterial = processMaterial(scene->mMaterials[mesh->mMaterialIndex]);
    auto processedMesh = std::make_shared<Mesh>(m_context, vertices, indices);
    transform.Decompose(*(aiVector3D*)(&processedMesh->m_transform.scale), *(aiVector3D*)(&processedMesh->m_transform.rotation), *(aiVector3D*)(&processedMesh->m_transform.translation));
    processedMesh->material = processedMaterial;
    return processedMesh;
}


std::shared_ptr<Material> Model::processMaterial(aiMaterial *material) {

    std::shared_ptr<Material> mat = std::make_shared<Material>();

    mat->descriptorSet = gfx::vulkan::DescriptorSet::Builder{}
        .build(m_context, Material::getMaterialDescriptorSetLayout());
    
    // load all the textures and parameters
    mat->diffuse = loadMaterialTexture(material, aiTextureType_DIFFUSE, "diffuse");
    mat->specular = loadMaterialTexture(material, aiTextureType_SPECULAR, "diffuse");
    mat->normal = loadMaterialTexture(material, aiTextureType_NORMALS, "diffuse");

    mat->update();

    return mat;
}


std::shared_ptr<gfx::vulkan::Image> Model::loadMaterialTexture(aiMaterial *mat, aiTextureType type, std::string typeName) {
    if (mat->GetTextureCount(type) == 0) {
        auto img = gfx::vulkan::Image::Builder{}
            .build2D(m_context, 1, 1, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        auto stagingBuffer = gfx::vulkan::Buffer::Builder{} 
            .build(m_context, 4 * sizeof(uint8_t), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        auto map = stagingBuffer->map();
        uint8_t data[4] = {255, 255, 255, 255};
        std::memcpy(map, data, sizeof(uint8_t) * 4);
        stagingBuffer->unmap();

        img->transitionLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        gfx::vulkan::Image::copyBufferToImage(m_context, *stagingBuffer, *img, VkBufferImageCopy{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .imageOffset = {0, 0, 0},
            .imageExtent = {static_cast<uint32_t>(1), static_cast<uint32_t>(1), 1}
        });

        img->transitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        return img;
    }
    if (mat->GetTextureCount(type) == 0) return nullptr;

    std::shared_ptr<gfx::vulkan::Image> img;
    for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
        aiString str;
        mat->GetTexture(type, i, &str);
        if (m_loadedImages.contains(str.C_Str())) {
            return m_loadedImages.at(str.C_Str());
        }
        std::string filePath = m_directory.string() + '/' + str.C_Str();
        std::replace(filePath.begin(), filePath.end(), '\\', '/');
        if (type == aiTextureType_DIFFUSE)
            img = gfx::vulkan::Image::Builder{}
                .loadFromPath(m_context, filePath);
        else 
            img = gfx::vulkan::Image::Builder{}
                .loadFromPath(m_context, filePath, VK_FORMAT_R8G8B8A8_UNORM);
        
        // tex = std::make_shared<gfx::Texture>((m_directory + '/' + str.C_Str()).c_str());
        m_loadedImages[str.C_Str()] = img;
    }
    return img;
}

void Model::draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, bool withMaterial) {
    for (auto mesh : meshes) {
        mesh->draw(commandBuffer, pipelineLayout, withMaterial);
    }
}

} // namespace core
