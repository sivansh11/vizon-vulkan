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

#include <glm/glm.hpp>
#include "glm/gtx/string_cast.hpp"
#include <entt/entt.hpp>

#include "core/imgui_utils.hpp"

#include "editor_camera.hpp"

#include "bvh.hpp"
#include "core/model.hpp"

#include <glm/gtx/string_cast.hpp>

#include <memory>
#include <iostream>
#include <algorithm>
#include <chrono>

int main(int argc, char **argv) {

    auto window = std::make_shared<core::window_t>("VIZON-vulkan", 800, 600);
    auto context = std::make_shared<gfx::vulkan::context_t>(window, 1, true);
    core::ImGui_init(window, context);

    auto model = core::load_model_from_path("../../assets/models/cornell_box.obj");
    std::vector<triangle_t> triangles;
    {
        for (auto mesh : model.meshes) {
            assert(mesh.indices.size() % 3 == 0);
            for (uint32_t i = 0; i < mesh.indices.size(); i+=3) {
                core::vertex_t v0 = mesh.vertices[mesh.indices[i + 0]];
                core::vertex_t v1 = mesh.vertices[mesh.indices[i + 1]];
                core::vertex_t v2 = mesh.vertices[mesh.indices[i + 2]];

                triangle_t triangle{};
                triangle.p0 = v0.position;
                triangle.p1 = v1.position;
                triangle.p2 = v2.position;

                triangles.push_back(triangle);

            }
        }
    }

    std::cout << "Loaded file with " << triangles.size() << " triangle(s)" << std::endl;

    std::vector<aabb_t> aabbs(triangles.size());
    std::vector<glm::vec3> centers(triangles.size());

    for (uint32_t i = 0; i < triangles.size(); i++) {
        aabbs[i] = aabb_t{}.extend(triangles[i].p0).extend(triangles[i].p1).extend(triangles[i].p2);
        centers[i] = (triangles[i].p0 + triangles[i].p1 + triangles[i].p2) / 3.0f;            
    }

    bvh_t bvh = bvh_t::build(aabbs.data(), centers.data(), triangles.size());
    std::cout << "Built BVH with " << bvh.nodes.size() << " node(s), depth " << bvh.depth() << std::endl;

    for (auto node : bvh.nodes) {
        std::cout << glm::to_string(node.aabb.min) << " " << glm::to_string(node.aabb.max) << " " << node.primitive_count << " " << node.first_index << '\n';
    }

    struct aabb_vis_vertex_t {
        glm::vec3 vertex;
    };

    std::vector<aabb_vis_vertex_t> aabb_vis_vertices{};
    for (auto node : bvh.nodes) {
        auto& aabb = node.aabb;
        auto& v0 = aabb.min; 
        auto& v1 = aabb.max; 

        aabb_vis_vertices.emplace_back(glm::vec3{v0.x, v0.y, v0.z});
        aabb_vis_vertices.emplace_back(glm::vec3{v0.x, v0.y, v1.z});
        aabb_vis_vertices.emplace_back(glm::vec3{v0.x, v1.y, v0.z});
        aabb_vis_vertices.emplace_back(glm::vec3{v0.x, v1.y, v1.z});
        aabb_vis_vertices.emplace_back(glm::vec3{v1.x, v0.y, v0.z});
        aabb_vis_vertices.emplace_back(glm::vec3{v1.x, v0.y, v1.z});
        aabb_vis_vertices.emplace_back(glm::vec3{v1.x, v1.y, v0.z});
        aabb_vis_vertices.emplace_back(glm::vec3{v1.x, v1.y, v1.z});
    }

    auto [width, height] = window->get_dimensions();

    editor_camera_t editor_camera{ window };

    auto rt_renderpass = gfx::vulkan::renderpass_builder_t{}
        .add_color_attachment(VkAttachmentDescription{
            .format = VK_FORMAT_R8G8B8A8_SRGB,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
        })
        .build(context);

    auto rt_timer = core::make_ref<gfx::vulkan::gpu_timer_t>(context);
    
    auto rt_image = gfx::vulkan::image_builder_t{}
        .build2D(context, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    auto rt_framebuffer = gfx::vulkan::framebuffer_builder_t{}
        .add_attachment_view(rt_image->image_view())
        .build(context, rt_renderpass->renderpass(), width, height);

    auto rt_dsl = gfx::vulkan::descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .addLayoutBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .addLayoutBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .addLayoutBinding(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(context);

    auto rt_ds = rt_dsl->new_descriptor_set();

    auto triangles_buffer = gfx::vulkan::buffer_builder_t{}
        .build(context, triangles.size() * sizeof(triangle_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    auto nodes_buffer = gfx::vulkan::buffer_builder_t{}
        .build(context, bvh.nodes.size() * sizeof(node_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    auto indices_buffer = gfx::vulkan::buffer_builder_t{}
        .build(context, bvh.primitive_indices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    struct ubo_t {
        glm::mat4 view;
        glm::mat4 projection;
        glm::mat4 inverse_view;
        glm::mat4 inverse_projection;
        int num_tris;
        glm::vec3 eye;
        glm::vec3 dir;
        glm::vec3 up;
        glm::vec3 right;
    };

    auto ubo_buffer = gfx::vulkan::buffer_builder_t{}
        .build(context, sizeof(ubo_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);    

    rt_ds->write()  
        .pushBufferInfo(0, 1, triangles_buffer->descriptor_info(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
        .pushBufferInfo(1, 1, nodes_buffer->descriptor_info(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
        .pushBufferInfo(2, 1, indices_buffer->descriptor_info(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
        .pushBufferInfo(3, 1, ubo_buffer->descriptor_info())
        .update();

    std::memcpy(triangles_buffer->map(), triangles.data(), triangles.size() * sizeof(triangle_t));
    std::memcpy(nodes_buffer->map(), bvh.nodes.data(), bvh.nodes.size() * sizeof(node_t));
    std::memcpy(indices_buffer->map(), bvh.primitive_indices.data(), bvh.primitive_indices.size() * sizeof(uint32_t));
    auto ubo = reinterpret_cast<ubo_t *>(ubo_buffer->map());

    auto rt_pipeline = gfx::vulkan::pipeline_builder_t{}
        .add_shader("../../assets/new_shaders/rt_test/glslvert")
        .add_shader("../../assets/new_shaders/rt_test/glsl.frag")
        .add_descriptor_set_layout(rt_dsl)
        .add_default_color_blend_attachment_state()
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
        .add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .build(context, rt_renderpass->renderpass());
    
    auto imgui_dsl = gfx::vulkan::descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(context);
    auto imgui_ds = imgui_dsl->new_descriptor_set();
    imgui_ds->write()
        .pushImageInfo(0, 1, rt_image->descriptor_info(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
        .update();

        
    glm::vec3 eye{ 0, 1, 3 };
    glm::vec3 dir{ 0, 0, -1 };
    glm::vec3 up{ 0, 1, 0 };

    dir = glm::normalize(dir);
    auto right = glm::normalize(glm::cross(dir, up));
    up = glm::cross(right, dir);

    float target_FPS = 1000.f;
    auto last_time = std::chrono::system_clock::now();
    while (!window->should_close()) {
        window->poll_events();
        
        auto current_time = std::chrono::system_clock::now();
        auto time_difference = current_time - last_time;
        if (time_difference.count() / 1e6 < 1000.f / target_FPS) {
            continue;
        }
        last_time = current_time;
        float dt = time_difference.count() / float(1e6);

        editor_camera.onUpdate(dt);
        ubo->view = editor_camera.getView();
        ubo->projection = editor_camera.getProjection();
        ubo->inverse_view = glm::inverse(editor_camera.getView());
        ubo->inverse_projection= glm::inverse(editor_camera.getProjection());
        ubo->eye = eye;
        ubo->dir = dir;
        ubo->up = up;
        ubo->right = right;
        ubo->num_tris = triangles.size();

        if (auto start_frame = context->start_frame()) {
            auto [commandbuffer, current_index] = *start_frame;

            VkClearValue clear_color{};
            clear_color.color = {0, 0, 0, 0};    
            VkClearValue clear_depth{};
            clear_depth.depthStencil.depth = 1;

            VkViewport swapchain_viewport{};
            swapchain_viewport.x = 0;
            swapchain_viewport.y = 0;
            swapchain_viewport.width = static_cast<float>(context->swapchain_extent().width);
            swapchain_viewport.height = static_cast<float>(context->swapchain_extent().height);
            swapchain_viewport.minDepth = 0;
            swapchain_viewport.maxDepth = 1;
            VkRect2D swapchain_scissor{};
            swapchain_scissor.offset = {0, 0};
            swapchain_scissor.extent = context->swapchain_extent();

            rt_timer->begin(commandbuffer);
            rt_renderpass->begin(commandbuffer, rt_framebuffer->framebuffer(), VkRect2D{
                .offset = {0, 0},
                .extent = context->swapchain_extent(),
            }, {
                clear_color ,
            });
            vkCmdSetViewport(commandbuffer, 0, 1, &swapchain_viewport);
            vkCmdSetScissor(commandbuffer, 0, 1, &swapchain_scissor);

            rt_pipeline->bind(commandbuffer);

            vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rt_pipeline->pipeline_layout(), 0, 1, &rt_ds->descriptor_set(), 0, 0);
            
            vkCmdDraw(commandbuffer, 6, 1, 0, 0);

            rt_timer->end(commandbuffer);
            rt_renderpass->end(commandbuffer);

            
            context->begin_swapchain_renderpass(commandbuffer, clear_color);
            
            core::ImGui_newframe();

            ImGui::Begin("view");
            ImGui::Image(reinterpret_cast<ImTextureID>(reinterpret_cast<void *>(imgui_ds->descriptor_set())), ImGui::GetContentRegionAvail(), {0, 1}, {1, 0});
            ImGui::End();

            ImGui::Begin("debug");
            ImGui::Text("%f", ImGui::GetIO().Framerate);
            ImGui::End();

            core::ImGui_endframe(commandbuffer);
            context->end_swapchain_renderpass(commandbuffer);

            context->end_frame(commandbuffer);
        }
        core::clear_frame_function_times();
    }

    
    // glm::vec3 eye{ 0, 1, 3 };
    // glm::vec3 dir{ 0, 0, -1 };
    // glm::vec3 up{ 0, 1, 0 };

    // dir = glm::normalize(dir);
    // auto right = glm::normalize(glm::cross(dir, up));
    // up = glm::cross(right, dir);

    // std::vector<uint8_t> image(1024 * 1024 * 3);
    // uint32_t intersections = 0;
    // std::cout << "Rendering";
    // for (uint32_t y = 0; y < 1024; ++y) {
    //     for (uint32_t x = 0; x < 1024; ++x) {
    //         auto u = 2.0f * static_cast<float>(x)/static_cast<float>(1024) - 1.0f;
    //         auto v = 2.0f * static_cast<float>(y)/static_cast<float>(1024) - 1.0f;
    //         ray_t ray;
    //         ray.origin = eye;
    //         ray.direction = dir + u * right + v * up;
    //         ray.tmin = 0;
    //         ray.tmax = std::numeric_limits<float>::max();
    //         auto hit = bvh.traverse(ray, triangles);
    //         if (hit)
    //             intersections++;
    //         auto pixel = 3 * (y * 1024 + x);
    //         image[pixel + 0] = hit.primitive_index * 37;
    //         image[pixel + 1] = hit.primitive_index * 91;
    //         image[pixel + 2] = hit.primitive_index * 51;
    //     }
    //     if (y % (1024 / 10) == 0)
    //         std::cout << "." << std::flush;
    // }
    // std::cout << "\n" << intersections << " intersection(s) found" << std::endl;

    // std::ofstream out("out.ppm", std::ofstream::binary);
    // out << "P6 " << 1024 << " " << 1024 << " " << 255 << "\n";
    // for(uint32_t j = 1024; j > 0; --j)
    //     out.write(reinterpret_cast<char*>(image.data() + (j - 1) * 3 * 1024), sizeof(uint8_t) * 3 * 1024);
    // std::cout << "Image saved as " << "out.ppm" << std::endl;


    context->wait_idle();

    core::ImGui_shutdown();

    return 0;
}