#include "core/window.hpp"
#include "core/core.hpp"
#include "core/event.hpp"
#include "core/log.hpp"
#include "core/mesh.hpp"
#include "core/material.hpp"
#include "core/model.hpp"

#include "glm/gtx/string_cast.hpp"
#include "core/components.hpp"

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

#include "editor_camera.hpp"

#include <memory>
#include <iostream>
#include <algorithm>
#include <chrono>


int main(int argc, char **argv) {
    auto window = core::make_ref<core::window_t>("test2", 1200, 800);
    auto context = core::make_ref<gfx::vulkan::context_t>(window, 2, true);

    auto dsl = gfx::vulkan::descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .addLayoutBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .addLayoutBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .build(context);
    
    auto pipeline = gfx::vulkan::pipeline_builder_t{}
        .add_descriptor_set_layout(dsl)
        .add_shader("../../assets/new_shaders/cull/glsl.comp")
        .build(context);

    // auto buf = gfx::vulkan::buffer_builder_t{}
    //     .build(context, sizeof(int) * 70, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    // auto dsl = gfx::vulkan::descriptor_set_layout_builder_t{}
    //     .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT)
    //     .build(context);
    
    // auto ds = dsl->new_descriptor_set();
    // ds->write()
    //     .pushBufferInfo(0, 1, buf->descriptor_info(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
    //     .update();

    // auto test = gfx::vulkan::pipeline_builder_t{}
    //     .add_descriptor_set_layout(dsl)
    //     .add_shader("../../assets/new_shaders/test_comp/glsl.comp")
    //     .build(context);
    
    // context->single_use_commandbuffer([&](VkCommandBuffer commandbuffer) {
    //     vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_COMPUTE, test->pipeline_layout(), 0, 1, &ds->descriptor_set(), 0, 0);
    //     test->bind(commandbuffer);
    //     vkCmdDispatch(commandbuffer, (70 / 32) + 1, 1, 1);
    // });

    context->wait_idle();

    // auto val = (int *)buf->map();

    // for (int i = 0; i < 70; i++) {
    //     std::cout << val[i] << '\n';
    // }

    // core::Transform t{};
    // auto mat = t.mat4();

    // std::cout << glm::to_string(mat[2]) << '\n';

    return 0;
}