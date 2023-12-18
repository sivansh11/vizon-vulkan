#ifndef RENDER_SYSTEM_DEPTH_PREPASS_SYSTEM_HPP
#define RENDER_SYSTEM_DEPTH_PREPASS_SYSTEM_HPP

#include "gfx/vulkan/context.hpp"
#include "gfx/vulkan/pipeline.hpp"
#include "gfx/vulkan/descriptor.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "gfx/vulkan/image.hpp"
#include "gfx/vulkan/renderpass.hpp"
#include "gfx/vulkan/framebuffer.hpp"
#include "gfx/vulkan/timer.hpp"

#include "base.hpp"

#include "../editor_camera.hpp"

#include "core/model.hpp"

class depth_prepass_t {
public:
    depth_prepass_t(core::ref<core::window_t> window, core::ref<gfx::vulkan::context_t> context) 
      : _window(window),
        _context(context) {
        
        
        auto [width, height] = _window->get_dimensions();


        _renderpass = gfx::vulkan::renderpass_builder_t{}
            .set_depth_attachment(VkAttachmentDescription{
                .format = VK_FORMAT_D32_SFLOAT,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
            })
            .build(_context);

        _image = gfx::vulkan::image_builder_t{}
            .build2D(_context, width, height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        _framebuffer = gfx::vulkan::framebuffer_builder_t{}
            .add_attachment_view(_image->image_view())
            .build(_context, _renderpass->renderpass(), width, height);

        _camera_uniform_descriptor_set_layout = gfx::vulkan::descriptor_set_layout_builder_t{}
            .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT)
            .build(_context);

        _model_uniform_descriptor_set_layout = gfx::vulkan::descriptor_set_layout_builder_t{}
            .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT)
            .build(_context);
        
        for (uint32_t i = 0; i < _context->MAX_FRAMES_IN_FLIGHT; i++) {
            auto camera_uniform = gfx::vulkan::buffer_builder_t{}
                .build(_context, sizeof(camera_uniform_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            auto camera_uniform_descriptor_set = _camera_uniform_descriptor_set_layout->new_descriptor_set();
            camera_uniform_descriptor_set->write()
                .pushBufferInfo(0, 1, camera_uniform->descriptor_info())
                .update();
            _camera_uniform_descriptor_set.push_back(camera_uniform_descriptor_set);
            _camera_uniform.push_back(camera_uniform);

            auto model_uniform = gfx::vulkan::buffer_builder_t{}
                .build(_context, sizeof(model_uniform_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            auto model_uniform_descriptor_set = _model_uniform_descriptor_set_layout->new_descriptor_set();
            model_uniform_descriptor_set->write()
                .pushBufferInfo(0, 1, model_uniform->descriptor_info())
                .update();
            _model_uniform_descriptor_set.push_back(model_uniform_descriptor_set);
            _model_uniform.push_back(model_uniform);
            
        }

        _pipeline = gfx::vulkan::pipeline_builder_t{}
            .add_shader("../../assets/new_shaders/depth/glsl.vert")
            .add_shader("../../assets/new_shaders/depth/glsl.frag")
            .add_descriptor_set_layout(_camera_uniform_descriptor_set_layout)  // set 0
            .add_descriptor_set_layout(_model_uniform_descriptor_set_layout)   // set 1
            .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
            .add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
            .add_vertex_input_binding_description(0, sizeof(core::vertex_t), VK_VERTEX_INPUT_RATE_VERTEX)
            .add_vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, position))
            .build(_context, _renderpass->renderpass());

        _gpu_timer = core::make_ref<gfx::vulkan::gpu_timer_t>(_context);
    }

    void render(VkCommandBuffer commandbuffer, uint32_t current_index, const editor_camera_t& editor_camera, const std::vector<cmd_draw_t> cmd_draws) {
        // potentially sort, cull and batch draw cmds
        auto final_cmd_draws = cmd_draws;
        
        VkClearValue clear_color{};
        clear_color.color = {0, 0, 0, 0};    
        VkClearValue clear_depth{};
        clear_depth.depthStencil.depth = 1;

        VkViewport swapchain_viewport{};
        swapchain_viewport.x = 0;
        swapchain_viewport.y = 0;
        swapchain_viewport.width = static_cast<float>(_context->swapchain_extent().width);
        swapchain_viewport.height = static_cast<float>(_context->swapchain_extent().height);
        swapchain_viewport.minDepth = 0;
        swapchain_viewport.maxDepth = 1;
        VkRect2D swapchain_scissor{};
        swapchain_scissor.offset = {0, 0};
        swapchain_scissor.extent = _context->swapchain_extent();

        _gpu_timer->begin(commandbuffer);
        _renderpass->begin(commandbuffer, _framebuffer->framebuffer(), VkRect2D{
            .offset = {0, 0},
            .extent = _context->swapchain_extent(),
        }, {
            clear_depth,
        });

        camera_uniform_t camera_uniform{};
        camera_uniform.view = editor_camera.getView();
        camera_uniform.projection = editor_camera.getProjection();
        camera_uniform.projection_view = camera_uniform.projection * camera_uniform.view;
        camera_uniform.inverse_projection = glm::inverse(camera_uniform.projection);
        camera_uniform.inverse_view = glm::inverse(camera_uniform.view);
        std::memcpy(_camera_uniform[current_index]->map(), &camera_uniform, sizeof(camera_uniform));        

        vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline->pipeline_layout(), 0, 1, &_camera_uniform_descriptor_set[current_index]->descriptor_set(), 0, 0);

        for (auto cmd_draw : final_cmd_draws) {
            // bind mesh descriptor set in future
            VkDeviceSize offsets{ 0 };
            vkCmdBindVertexBuffers(commandbuffer, 0, 1, &cmd_draw.gpu_mesh_data.vertex_buffer->buffer(), &offsets);
            vkCmdBindIndexBuffer(commandbuffer, cmd_draw.gpu_mesh_data.index_buffer->buffer(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandbuffer, cmd_draw.gpu_mesh_data.index_count, 1, 0, 0, 0);
        }

        _gpu_timer->end(commandbuffer);
        _renderpass->end(commandbuffer);
    }

private:
    core::ref<core::window_t> _window;
    core::ref<gfx::vulkan::context_t> _context;
                            
    core::ref<gfx::vulkan::renderpass_t> _renderpass;

    core::ref<gfx::vulkan::image_t> _image;

    core::ref<gfx::vulkan::framebuffer_t> _framebuffer;

    core::ref<gfx::vulkan::descriptor_set_layout_t> _camera_uniform_descriptor_set_layout;
    core::ref<gfx::vulkan::descriptor_set_layout_t> _model_uniform_descriptor_set_layout;

    std::vector<core::ref<gfx::vulkan::buffer_t>> _camera_uniform;
    std::vector<core::ref<gfx::vulkan::buffer_t>> _model_uniform;

    std::vector<core::ref<gfx::vulkan::descriptor_set_t>> _camera_uniform_descriptor_set;
    std::vector<core::ref<gfx::vulkan::descriptor_set_t>> _model_uniform_descriptor_set;

    core::ref<gfx::vulkan::pipeline_t> _pipeline;

    core::ref<gfx::vulkan::gpu_timer_t> _gpu_timer;
};

#endif