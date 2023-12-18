#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "gfx/vulkan/context.hpp"
#include "gfx/vulkan/pipeline.hpp"
#include "gfx/vulkan/descriptor.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "gfx/vulkan/image.hpp"
#include "gfx/vulkan/renderpass.hpp"
#include "gfx/vulkan/framebuffer.hpp"
#include "gfx/vulkan/timer.hpp"

#include "core/model.hpp"

#include "editor_camera.hpp"

#include <glm/glm.hpp>

struct camera_uniform_t {
    glm::mat4 projection_view;
    glm::mat4 view;
    glm::mat4 inverse_view;
    glm::mat4 projection;
    glm::mat4 inverse_projection;
};

struct model_uniform_t {
    glm::mat4 model;
    glm::mat4 inverse_model;
};

struct gpu_mesh_t {
    core::ref<gfx::vulkan::buffer_t> vertex_buffer;
    core::ref<gfx::vulkan::buffer_t> index_buffer;
    uint32_t index_count;
    uint32_t vertex_count;
    // ideally seperate descriptor ser for model & inv model matrices etc and for material so u can sort materials and reduce bindings
    // but here no model and inv model, only mat with diffuse
    core::ref<gfx::vulkan::descriptor_set_t> material_descriptor_set;  
    core::aabb_t aabb;
};

struct draw_data_info_t {
    gpu_mesh_t gpu_mesh;
};

struct plane_t {
    plane_t() = default;
    plane_t(const glm::vec3& point, const glm::vec3& norm) : normal(glm::normalize(norm)), distance(glm::dot(normal, point)) {}
    float get_signed_distance_to_plane(const glm::vec3& point) const {
        return glm::dot(normal, point) - distance;
    }
    glm::vec3 normal = {0, 1, 0};
    float distance = 0.f;
};

struct aabb_t {
    aabb_t(const glm::vec3& min, glm::vec3& max) : center((min + max) * 0.5f), extents(max.x - center.x, max.y - center.y, max.z - center.z) {}
    aabb_t(const glm::vec3& center, float ix, float iy, float iz) : center(center), extents(ix, iy, iz) {}

    bool is_on_or_forward_plane(const plane_t& plane) const {
        const float r = extents.x * std::abs(plane.normal.x) + extents.y * std::abs(plane.normal.y) + extents.z * std::abs(plane.normal.z);
        return -r <= plane.get_signed_distance_to_plane(center);
    }

    glm::vec3 center{ 0, 0, 0 };
    glm::vec3 extents{ 0, 0, 0 }; 
};

struct frustum_t {
    frustum_t(const editor_camera_t& camera, float aspect, float fovy, float znear, float zfar) {
        const glm::vec3 camera_front = glm::normalize(camera.front());
        const glm::vec3 camera_up    = glm::normalize(camera.up());
        const glm::vec3 camera_right = glm::normalize(camera.right());

        const float half_v_size = zfar * glm::tan(fovy * .5f);
        const float half_h_size = half_v_size * aspect;
        const glm::vec3 front_mult_far = zfar * camera_front;
        
        near_face = { camera.position() + znear * camera_front, camera_front };
        far_face = { camera.position() + front_mult_far, -camera_front };

        right_face = { camera.position(), glm::cross(front_mult_far - camera_right * half_h_size, camera_up) };
        left_face = { camera.position(), glm::cross(camera_up,front_mult_far + camera_right * half_h_size) };

        top_face = { camera.position(), glm::cross(camera_right, front_mult_far - camera_up * half_v_size) };
        bottom_face = { camera.position(), glm::cross(front_mult_far + camera_up * half_v_size, camera_right) };
    }

    bool is_on_frustum(const aabb_t& aabb) {
        const glm::vec3 global_center = aabb.center;

        const glm::vec3 right   = glm::vec3{1, 0, 0}  * aabb.extents.x; 
        const glm::vec3 up      = glm::vec3{0, 1, 0}  * aabb.extents.y; 
        const glm::vec3 forward = glm::vec3{0, 0, -1} * aabb.extents.z;

        const float new_i = std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, right)) +
                            std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, up)) +
                            std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, forward));

        const float new_j = std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, right)) +
                            std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, up)) +
                            std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, forward));

        const float new_k = std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, right)) +
                            std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, up)) +
                            std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, forward));

        const aabb_t global_aabb{ global_center, new_i, new_j, new_k };

        return (global_aabb.is_on_or_forward_plane(left_face)) &&
               (global_aabb.is_on_or_forward_plane(right_face)) &&
               (global_aabb.is_on_or_forward_plane(top_face)) &&
               (global_aabb.is_on_or_forward_plane(bottom_face)) &&
               (global_aabb.is_on_or_forward_plane(near_face)) &&
               (global_aabb.is_on_or_forward_plane(far_face));
    }

    plane_t near_face;
    plane_t far_face;
    plane_t right_face;
    plane_t left_face;
    plane_t top_face;
    plane_t bottom_face;
};

struct culling_settings_t {
    plane_t near_face;
    plane_t far_face;
    plane_t right_face;
    plane_t left_face;
    plane_t top_face;
    plane_t bottom_face;
    uint size;   
    float p00;
    float p11;
    float near;
    float far; 
    float hiz_width; 
    float hiz_height; 
};

class renderer_t {
public:
    renderer_t(core::ref<core::window_t> window, core::ref<gfx::vulkan::context_t> context);

    ~renderer_t();

    static core::ref<gfx::vulkan::descriptor_set_layout_t> get_material_descriptor_set();

    void render(VkCommandBuffer commandbuffer, uint32_t current_index, const editor_camera_t& editor_camera, const std::vector<draw_data_info_t> draw_data_infos);

    core::ref<gfx::vulkan::image_t> get_albedo() { return _gbuffer_albedo_image; }

private:    
    core::ref<core::window_t> _window;
    core::ref<gfx::vulkan::context_t> _context;

    core::ref<gfx::vulkan::renderpass_t> _depth_pre_renderpass;
    core::ref<gfx::vulkan::renderpass_t> _deferred_renderpass;  // albedo only for now

    core::ref<gfx::vulkan::gpu_timer_t> _depth_pre_gpu_timer;
    core::ref<gfx::vulkan::gpu_timer_t> _deferred_gpu_timer;

    core::ref<gfx::vulkan::image_t> _depth_image;
    core::ref<gfx::vulkan::image_t> _hiz_image;
    core::ref<gfx::vulkan::image_t> _gbuffer_albedo_image;

    core::ref<gfx::vulkan::framebuffer_t> _depth_pre_framebuffer;
    core::ref<gfx::vulkan::framebuffer_t> _deferred_framebuffer;

    core::ref<gfx::vulkan::descriptor_set_layout_t> _camera_uniform_descriptor_set_layout;    
    core::ref<gfx::vulkan::descriptor_set_layout_t> _storage_sampler_image_descriptor_set_layout;    
    core::ref<gfx::vulkan::descriptor_set_layout_t> _culling_descriptor_set_layout;    

    std::vector<core::ref<gfx::vulkan::buffer_t>> _camera_uniforms;
    std::vector<core::ref<gfx::vulkan::buffer_t>> _indirect_draws;
    std::vector<core::ref<gfx::vulkan::buffer_t>> _culling_settings;
    std::vector<core::ref<gfx::vulkan::buffer_t>> _aabbs;

    std::vector<core::ref<gfx::vulkan::descriptor_set_t>> _camera_uniform_descriptor_sets;
    core::ref<gfx::vulkan::descriptor_set_t> _storage_sampler_image_copy_descriptor_set;    
    std::vector<core::ref<gfx::vulkan::descriptor_set_t>> _storage_sampler_image_hiz_gen_descriptor_sets;
    std::vector<core::ref<gfx::vulkan::descriptor_set_t>> _culling_descriptor_sets;    

    core::ref<gfx::vulkan::pipeline_t> _depth_pre_pipeline;
    core::ref<gfx::vulkan::pipeline_t> _deferred_pipeline;
    core::ref<gfx::vulkan::pipeline_t> _copy_pipeline;
    core::ref<gfx::vulkan::pipeline_t> _gen_hiz_mips_pipeine;
    core::ref<gfx::vulkan::pipeline_t> _culling_pipeline;

};

#endif