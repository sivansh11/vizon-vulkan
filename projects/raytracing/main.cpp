#include <iostream>

#include "core/core.hpp"
#include "core/imgui_utils.hpp"
#include "core/window.hpp"
#include "core/core.hpp"
#include "core/event.hpp"
#include "core/log.hpp"
#include "core/mesh.hpp"
#include "core/material.hpp"
#include "core/model.hpp"


#include "gfx/vulkan/context.hpp"
#include "gfx/vulkan/pipeline.hpp"
#include "gfx/vulkan/descriptor.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "gfx/vulkan/image.hpp"
#include "gfx/vulkan/renderpass.hpp"
#include "gfx/vulkan/framebuffer.hpp"
#include "gfx/vulkan/timer.hpp"
#include "gfx/vulkan/acceleration_structure.hpp"

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

using namespace core;
using namespace gfx::vulkan;

class camera_t {
public:
    camera_t(ref<window_t> window) : _window(window) {}

    const glm::mat4 get_projection() { return _projection; }
    const glm::mat4 get_view() { return _view; }
    const glm::vec3 get_position() { return _position; }
    glm::vec3 get_direction() {
        return _position + _front;
    }

    void on_update(float ts) {
        auto [width, height] = _window->get_dimensions();

        _projection = glm::perspective(glm::radians(_fov), float(width) / float(height), _near, _far);

        double cur_x, cur_y;
        glfwGetCursorPos(_window->window(), &cur_x, &cur_y);

        float velocity = _speed * ts;

        if (glfwGetKey(_window->window(), GLFW_KEY_W)) 
            _position += _front * velocity;
        if (glfwGetKey(_window->window(), GLFW_KEY_S)) 
            _position -= _front * velocity;
        if (glfwGetKey(_window->window(), GLFW_KEY_D)) 
            _position += _right * velocity;
        if (glfwGetKey(_window->window(), GLFW_KEY_A)) 
            _position -= _right * velocity;
        if (glfwGetKey(_window->window(), GLFW_KEY_SPACE)) 
            _position += _up * velocity;
        if (glfwGetKey(_window->window(), GLFW_KEY_LEFT_SHIFT)) 
            _position -= _up * velocity;
        
        glm::vec2 mouse{ cur_x, cur_y };
        glm::vec2 difference = mouse - _initial_mouse;
        _initial_mouse = mouse;

        if (glfwGetMouseButton(_window->window(), GLFW_MOUSE_BUTTON_1)) {
            
            difference.x = difference.x / float(width);
            difference.y = -(difference.y / float(height));

            _yaw += difference.x * _sensitivity;
            _pitch += difference.y * _sensitivity;

            if (_pitch > 89.0f) 
                _pitch = 89.0f;
            if (_pitch < -89.0f) 
                _pitch = -89.0f;
        }

        glm::vec3 front;
        front.x = glm::cos(glm::radians(_yaw)) * glm::cos(glm::radians(_pitch));
        front.y = glm::sin(glm::radians(_pitch));
        front.z = glm::sin(glm::radians(_yaw)) * glm::cos(glm::radians(_pitch));
        _front = front * speed_multiplyer;
        _right = glm::normalize(glm::cross(_front, glm::vec3{0, 1, 0}));
        _up    = glm::normalize(glm::cross(_right, _front));

        _view = glm::lookAt(_position, _position + _front, glm::vec3{0, 1, 0});
    }

    float speed_multiplyer = 1;

private:
    ref<window_t> _window;

    glm::vec3 _front{0.f};
    glm::vec3 _up{0.f, 1.f, 0.f};
    glm::vec3 _right{0.f};

    glm::vec2 _initial_mouse{};

    float _yaw = 0.f;
    float _pitch = 0.f;
    float _speed = 0.005f;
    float _sensitivity = 100.f;

    bool _is_first{true};

    glm::vec3 _position{0.f};
    glm::mat4 _projection{1.f};
    glm::mat4 _view{1.f};
    
    float _fov{45.f};
    float _far{10000.f};
    float _near{0.0001f};
};

int main() {
    ref<window_t> window = make_ref<window_t>("vulkan-raytracing", 1200, 800);
    ref<context_t> context = make_ref<context_t>(window /*window*/, 1 /*MAX_FRAMES_IN_FLIGHT*/, true /*validations*/, true /*raytracing enable*/);
    ImGui_init(window, context);
    camera_t camera{window};

    model_t model = load_model_from_path("../../assets/models/cube/cube_scene.obj");

    // creating a combined vertex and index vectors (cpu side)
    std::vector<vertex_t> combined_vertices;
    std::vector<uint32_t> combined_indices;
    uint32_t index_offset = 0;
    for (auto mesh : model.meshes) {
        combined_vertices.reserve(combined_vertices.size() + mesh.vertices.size());
        for (auto vertex : mesh.vertices) {
            combined_vertices.push_back(vertex);
        }
        combined_indices.reserve(combined_indices.size() + mesh.indices.size());
        for (auto index : mesh.indices) {
            combined_indices.push_back(index + index_offset);
        }
        index_offset += mesh.indices.size();
    }

    // uploading the combined vertex and index data to the gpu
    ref<buffer_t> staging_buffer;
    ref<buffer_t> scene_vertex_buffer;
    ref<buffer_t> scene_index_buffer;

    staging_buffer = buffer_builder_t{}
        .build(context,
               combined_vertices.size() * sizeof(vertex_t),
               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    scene_vertex_buffer = buffer_builder_t{}
        .build(context,
               combined_vertices.size() * sizeof(vertex_t),
               VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    std::memcpy(staging_buffer->map(), combined_vertices.data(), combined_vertices.size() * sizeof(vertex_t));
    buffer_t::copy(context, *staging_buffer, *scene_vertex_buffer, VkBufferCopy{
        .size = combined_vertices.size() * sizeof(vertex_t),
    });

    staging_buffer = buffer_builder_t{}
        .build(context,
               combined_indices.size() * sizeof(uint32_t),
               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    scene_index_buffer = buffer_builder_t{}
        .build(context,
               combined_indices.size() * sizeof(uint32_t),
               VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    std::memcpy(staging_buffer->map(), combined_indices.data(), combined_indices.size() * sizeof(uint32_t));
    buffer_t::copy(context, *staging_buffer, *scene_index_buffer, VkBufferCopy{
        .size = combined_indices.size() * sizeof(uint32_t),
    });

    std::vector<ref<acceleration_structure_t>> blases;
    std::vector<VkAccelerationStructureInstanceKHR> blas_instances;
    uint64_t mesh_vertex_offset = 0;
    uint64_t mesh_index_offset = 0;
    uint32_t primitive_offset = 0;
    // creating the blas for each mesh
    for (auto mesh : model.meshes) {
        auto blas = acceleration_structure_builder_t{}
            .triangles(scene_vertex_buffer->device_address(), sizeof(vertex_t), combined_vertices.size(), scene_index_buffer->device_address() + mesh_index_offset, mesh.indices.size())
            .build(context);        

        VkAccelerationStructureInstanceKHR blas_instance{};
        blas_instance.transform = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
        };
        blas_instance.instanceCustomIndex = primitive_offset;
        blas_instance.mask = 0xFF;
        blas_instance.instanceShaderBindingTableRecordOffset = 0;
        blas_instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        blas_instance.accelerationStructureReference = blas->device_address();

        blases.push_back(blas);
        blas_instances.push_back(blas_instance);

        mesh_vertex_offset += mesh.vertices.size() * sizeof(vertex_t);
        mesh_index_offset += mesh.indices.size() * sizeof(uint32_t);
        primitive_offset += mesh.indices.size() / 3;
    }

    struct temp_test_material_t {
        glm::vec4 color;
    };

    std::vector<temp_test_material_t> materials;
    for (auto blas_instance : blas_instances) {
        static int i = 0;
        switch (i % 3) {
            case 0:
                materials.push_back(temp_test_material_t{{1, 0, 0, 1}});
                break;
            case 1:
                materials.push_back(temp_test_material_t{{0, 1, 0, 1}});
                break;
            case 2:
                materials.push_back(temp_test_material_t{{0, 0, 1, 1}});
                break;
            
            default:
                break;
        }
        i++;
    }

    auto material_buffer = buffer_builder_t{}
        .build(context,
               sizeof(temp_test_material_t) * materials.size(),
               VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    std::memcpy(material_buffer->map(), materials.data(), sizeof(temp_test_material_t) * materials.size());

    auto instance_buffer = buffer_builder_t{}
        .build(context, 
               sizeof(VkAccelerationStructureInstanceKHR) * blas_instances.size(),
               VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    std::memcpy(instance_buffer->map(), blas_instances.data(), sizeof(VkAccelerationStructureInstanceKHR) * blas_instances.size());

    auto tlas = acceleration_structure_builder_t{}
        .instances(instance_buffer->device_address(), blas_instances.size())
        .build(context);

    auto [width, height] = window->get_dimensions();

    auto rt_renderpass = renderpass_builder_t{}
        .add_color_attachment(VkAttachmentDescription{
            .format = VK_FORMAT_R8G8B8A8_SNORM,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
        })
        .build(context);

    auto rt_image = image_builder_t{}
        .build2D(context, width, height, VK_FORMAT_R8G8B8A8_SNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    auto rt_framebuffer = framebuffer_builder_t{}
        .add_attachment_view(rt_image->image_view())
        .build(context, rt_renderpass->renderpass(), width, height);
    
    auto rt_descriptor_set_layout = descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .addLayoutBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .addLayoutBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .addLayoutBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .addLayoutBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(context);

    auto rt_descriptor_set = rt_descriptor_set_layout->new_descriptor_set();

    struct uniform_data_t {
        glm::mat4 view_inv;
        glm::mat4 projection_inv;
    } uniform_data;

    auto rt_uniform_buffer = buffer_builder_t{}
        .build(context, sizeof(uniform_data), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    rt_descriptor_set->write()
        .pushAccelerationStructureInfo(0, 1, VkWriteDescriptorSetAccelerationStructureKHR{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .accelerationStructureCount = 1,
            .pAccelerationStructures = &tlas->acceleration_structure(),
        })
        .pushBufferInfo(1, 1, rt_uniform_buffer->descriptor_info())
        .pushBufferInfo(2, 1, scene_vertex_buffer->descriptor_info(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
        .pushBufferInfo(3, 1, scene_index_buffer->descriptor_info(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
        .pushBufferInfo(4, 1, material_buffer->descriptor_info(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
        .update();

    auto rt_pipeline = pipeline_builder_t{}
        .add_default_color_blend_attachment_state()
        .add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
        .add_descriptor_set_layout(rt_descriptor_set_layout)
        .add_shader("../../assets/shaders/test4/rt/shader.vert")
        .add_shader("../../assets/shaders/test4/rt/shader.frag")
        .build(context, rt_renderpass->renderpass());

    auto rt_timer = make_ref<gpu_timer_t>(context);

    auto swapchain_descriptor_set_layout = descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(context);

    auto swapchain_descriptor_set = swapchain_descriptor_set_layout->new_descriptor_set();

    swapchain_descriptor_set->write()
        .pushImageInfo(0, 1, rt_image->descriptor_info(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
        .update();
    
    auto swapchain_pipeline = pipeline_builder_t{}
        .add_default_color_blend_attachment_state()
        .add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
        .add_descriptor_set_layout(swapchain_descriptor_set_layout)
        .add_shader("../../assets/shaders/swapchain/base.vert")
        .add_shader("../../assets/shaders/swapchain/base.frag")
        .build(context, context->swapchain_renderpass());


    float target_fps = 60.f;
    auto last_time = std::chrono::system_clock::now();
    while (!window->should_close()) {
        window->poll_events();

        auto current_time = std::chrono::system_clock::now();
        auto time_difference = current_time - last_time;
        if (time_difference.count() / 1e6 < 1000.f / target_fps) {
            continue;
        }
        last_time = current_time;
        float dt = time_difference.count() / float(1e6);

        camera.on_update(dt);
        uniform_data.view_inv = glm::inverse(camera.get_view());
        uniform_data.projection_inv = glm::inverse(camera.get_projection());
        std::memcpy(rt_uniform_buffer->map(), &uniform_data, sizeof(uniform_data));


        if (auto start_frame = context->start_frame()) {
            auto [commandbuffer, current_index] = *start_frame;

            VkClearValue clear_color{};
            clear_color.color = {0, 0, 0, 0};    
            VkClearValue clear_depth{};
            clear_depth.depthStencil.depth = 1;
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(context->swapchain_extent().width);
            viewport.height = static_cast<float>(context->swapchain_extent().height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = context->swapchain_extent();   

            // raytracing
            rt_timer->begin(commandbuffer);
            rt_renderpass->begin(commandbuffer, rt_framebuffer->framebuffer(), VkRect2D{
                .offset = {0, 0},
                .extent = context->swapchain_extent(),
            }, {
                clear_color,
            }); 

            vkCmdSetViewport(commandbuffer, 0, 1, &viewport);
    		vkCmdSetScissor(commandbuffer, 0, 1, &scissor);
            rt_pipeline->bind(commandbuffer);
            vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rt_pipeline->pipeline_layout(), 0, 1, &rt_descriptor_set->descriptor_set(), 0, nullptr);
            vkCmdDraw(commandbuffer, 6, 1, 0, 0);

            rt_timer->end(commandbuffer);
            rt_renderpass->end(commandbuffer);

            // swapchain renderpass
            context->begin_swapchain_renderpass(commandbuffer, clear_color);

            vkCmdSetViewport(commandbuffer, 0, 1, &viewport);
    		vkCmdSetScissor(commandbuffer, 0, 1, &scissor);
            swapchain_pipeline->bind(commandbuffer);
            vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, swapchain_pipeline->pipeline_layout(), 0, 1, &swapchain_descriptor_set->descriptor_set(), 0, nullptr);
            vkCmdDraw(commandbuffer, 6, 1, 0, 0);

            ImGui_newframe();
            ImGui::Begin("debug panel");
            if (auto t = rt_timer->get_time()) {
                ImGui::Text("%f", *t);
            } else {
                ImGui::Text("undefined");
            }
            ImGui::End();
            ImGui_endframe(commandbuffer);

            context->end_swapchain_renderpass(commandbuffer);

            context->end_frame(commandbuffer);
        }

        clear_frame_function_times();
    }

    context->wait_idle();
    ImGui_shutdown();
}