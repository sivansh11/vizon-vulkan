#include "core/model.hpp"

#include <stb_image/stb_image.hpp>

#include <iostream>

struct vertex_t {
    glm::vec3 position{};
    glm::vec3 normal{};
    glm::vec2 uv{};
    glm::vec3 tangent{};
    glm::vec3 bi_tangent{};
};

struct aabb_t {
    glm::vec3 min, max;
};

enum texture_type_t {
    e_diffuse_map,
    e_specular_map,
    e_normal_map,
};

struct texture_t {
    int            width;
    int            height;
    texture_type_t texture_type;
    uint8_t *      pixels;
};

struct material_t {
    uint64_t   texture_count;
    texture_t *texture;
};

struct mesh_t {
    uint64_t   vertex_count;
    vertex_t  *vertices;
    uint64_t   index_count;
    uint32_t  *indices;
    aabb_t     aabb;
    material_t material;
};

struct model_t {
    uint64_t num_meshes;
    mesh_t  *meshes;  
};

class writer_t {
public:
    template <typename T>
    void write(const T& value) {
        const uint64_t num_bytes = sizeof(T);
        const char *data = reinterpret_cast<const char *>(&value);
        for (uint64_t i = 0; i < num_bytes; i++) {
            _data.push_back(data[i]);
        }
    }

    template <typename T>
    void write_array(const T* value_ptr, uint64_t count) {
        const uint64_t num_bytes = sizeof(T) * count;
        const char *data = reinterpret_cast<const char *>(value_ptr);
        for (uint64_t i = 0; i < num_bytes; i++) {
            _data.push_back(data[i]);
        }
    }

    char *data() { return _data.data(); }
    uint64_t size() { return _data.size(); }

private:
    std::vector<char> _data;
};

// the data memory should be stable
class reader_t {
public:
    reader_t(void *data, uint64_t size) : _data(reinterpret_cast<char *>(data)), _size(size) {}

    template <typename T>
    void read(T& value) {
        const uint64_t num_bytes = sizeof(T);

        T *data = reinterpret_cast<T *>(_data + _index);
        value = *data;
        _index += num_bytes;

        if (_index > _size) {
            ERROR("something is fucked");
            std::terminate();
        }
    }

    template <typename T>
    void read_array(T *&value_ptr, uint64_t count) {
        const uint64_t num_bytes = sizeof(T) * count;

        value_ptr = reinterpret_cast<T *>(_data + _index);
        _index += num_bytes;

        if (_index > _size) {
            ERROR("something is fucked");
            std::terminate();
        }
    }

private:
    uint64_t _size;
    uint64_t _index{ 0 };
    char *_data;
};

int main(int argc, char **argv) {
    core::model_t loaded_model = core::load_model_from_path("../../assets/models/Sponza/glTF/Sponza.gltf");

    model_t model;

    model.num_meshes = loaded_model.meshes.size();

    for (auto loaded_mesh : loaded_model.meshes) {
        mesh_t mesh;
        mesh.vertex_count = loaded_mesh.vertices.size();
        mesh.vertices = new vertex_t[mesh.vertex_count];
        
        mesh.index_count = loaded_mesh.indices.size();
        mesh.indices = new uint32_t[mesh.index_count];
        
        for (uint64_t i = 0; i < loaded_mesh.vertices.size(); i++) {
            mesh.vertices[i].position = loaded_mesh.vertices[i].position;
            mesh.vertices[i].normal = loaded_mesh.vertices[i].normal;
            mesh.vertices[i].uv = loaded_mesh.vertices[i].uv;
            mesh.vertices[i].tangent = loaded_mesh.vertices[i].tangent;
            mesh.vertices[i].bi_tangent = loaded_mesh.vertices[i].bi_tangent;
        }

        for (uint64_t i = 0; i < loaded_mesh.indices.size(); i++) {
            mesh.indices[i] = loaded_mesh.indices[i];
        }
        
        mesh.aabb.min = loaded_mesh.aabb.min;
        mesh.aabb.max = loaded_mesh.aabb.max;



        for (auto loaded_texture_info : loaded_mesh.material_description.texture_infos) {
            texture_t texture;

            switch (loaded_texture_info.texture_type) {
                case core::texture_type_t::e_diffuse_map:
                    texture.texture_type = texture_type_t::e_diffuse_map;
                    break;
                case core::texture_type_t::e_normal_map:
                    texture.texture_type = texture_type_t::e_normal_map;
                    break;
                case core::texture_type_t::e_specular_map:
                    texture.texture_type = texture_type_t::e_specular_map;
                    break;
                default:
                    continue;
            }
            
            
        }
    }

    writer_t writer{};

    // version
    writer.write((uint64_t)(0));


    return 0;
}