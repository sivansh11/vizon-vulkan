#include "renderer.hpp"

#include "core/imgui_utils.hpp"

// #define BOYBAYKILLER_TEST 1

namespace utils {

    static uint32_t power_of_2_before(uint32_t value) {
        uint32_t n = 0;
        while (true) {
            uint32_t number = std::pow(2, n);        
            if (number > value) {
                return std::pow(2, n - 1);
            }
            n++;
        }
    }

    static glm::mat4 create_orthographic_off_center(float left, float right, float bottom, float top, float zNear, float zFar) {
        // Calculate inverse values for division
        float invRL = 1.0f / (right - left);
        float invTB = 1.0f / (top - bottom);
        float invFN = 1.0f / (zFar - zNear);

        // Create the orthographic projection matrix
        glm::mat4 ortho = glm::mat4(
            glm::vec4(2.0f * invRL, 0.0f, 0.0f, 0.0f),
            glm::vec4(0.0f, 2.0f * invTB, 0.0f, 0.0f),
            glm::vec4(0.0f, 0.0f, -2.0f * invFN, 0.0f),
            glm::vec4(-(right + left) * invRL, -(top + bottom) * invTB, -(zFar + zNear) * invFN, 1.0f)
        );

        return ortho;
    }


} // namespace utils


namespace renderer {

    struct camera_uniform_t {
        // current
        glm::mat4 projection_view;
        glm::mat4 view;
        glm::mat4 inverse_view;
        glm::mat4 projection;
        glm::mat4 inverse_projection;
        // maybe add priv ?
    };


    struct renderer_data_t {
        bool _instantiated{false};
        
        core::ref<core::window_t> _window;
        core::ref<gfx::vulkan::context_t> _context;
        
        core::ref<gfx::vulkan::descriptor_set_layout_t> _material_descriptor_set_layout;

        core::ref<gfx::vulkan::image_t> _voxels_r32ui;
        core::ref<gfx::vulkan::image_t> _voxels_rgba8;
        core::ref<gfx::vulkan::image_t> _voxelization_dummy_image;
        core::ref<gfx::vulkan::image_t> _depth_image;

        #ifdef BOYBAYKILLER_TEST
        core::ref<gfx::vulkan::image_t> _voxels_r64ui;
        #endif

        core::ref<gfx::vulkan::gpu_timer_t> _voxelization_gpu_timer;
        core::ref<gfx::vulkan::gpu_timer_t> _voxelization_boybaykiller_gpu_timer;
        core::ref<gfx::vulkan::gpu_timer_t> _voxel_clear_gpu_timer;
        core::ref<gfx::vulkan::gpu_timer_t> _voxel_copy_gpu_timer;
        core::ref<gfx::vulkan::gpu_timer_t> _voxel_mip_map_gpu_timer;
        core::ref<gfx::vulkan::gpu_timer_t> _depth_gpu_timer;
        
        core::ref<gfx::vulkan::renderpass_t> _voxelization_renderpass; // empty ?
        core::ref<gfx::vulkan::renderpass_t> _depth_renderpass;

        core::ref<gfx::vulkan::framebuffer_t> _voxelization_framebuffer;  // has to be empty, but no imageless, so need a dummy image
        core::ref<gfx::vulkan::framebuffer_t> _depth_framebuffer;

        core::ref<gfx::vulkan::descriptor_set_layout_t> _voxelization_descriptor_set_0_layout;
        core::ref<gfx::vulkan::descriptor_set_layout_t> _voxelization_descriptor_set_2_layout;        
        core::ref<gfx::vulkan::descriptor_set_layout_t> _voxel_clear_descriptor_set_layout;
        core::ref<gfx::vulkan::descriptor_set_layout_t> _voxel_copy_descriptor_set_layout;
        core::ref<gfx::vulkan::descriptor_set_layout_t> _camera_uniform_descriptor_set_layout;

        core::ref<gfx::vulkan::descriptor_set_t> _voxelization_descriptor_set_0;
        core::ref<gfx::vulkan::descriptor_set_t> _voxelization_descriptor_set_2;
        #ifdef BOYBAYKILLER_TEST
        core::ref<gfx::vulkan::descriptor_set_t> _voxelization_boybaykiller_descriptor_set_2;
        #endif
        core::ref<gfx::vulkan::descriptor_set_t> _voxel_clear_descriptor_set;
        core::ref<gfx::vulkan::descriptor_set_t> _voxel_copy_descriptor_set;
        std::vector<core::ref<gfx::vulkan::descriptor_set_t>> _camera_uniform_descriptor_sets;

        core::ref<gfx::vulkan::buffer_t> _voxelization_ubo;
        std::vector<core::ref<gfx::vulkan::buffer_t>> _camera_unfiroms;
        
        core::ref<gfx::vulkan::pipeline_t> _voxelization_pipeline;
        #ifdef BOYBAYKILLER_TEST
        core::ref<gfx::vulkan::pipeline_t> _voxelization_boybaykiller_pipeline;
        #endif
        core::ref<gfx::vulkan::pipeline_t> _voxel_clear_pipeline;
        core::ref<gfx::vulkan::pipeline_t> _voxel_copy_pipeline;
        core::ref<gfx::vulkan::pipeline_t> _depth_pipeline;

        struct voxelization_settings_t {
            glm::vec3 grid_min = glm::vec3(-28.0f, - 3.0f, -17.0f);
            glm::vec3 grid_max = glm::vec3( 28.0f,  20.0f,  17.0f);
        };

        voxelization_settings_t _voxelization_settings{};
    };

    static renderer_data_t s_renderer_data{};

#define voxel_size 256

    void init(core::ref<core::window_t> window, core::ref<gfx::vulkan::context_t> context) {
        s_renderer_data._window = window;
        s_renderer_data._context = context;

        auto [width, height] = s_renderer_data._window->get_dimensions();

        s_renderer_data._material_descriptor_set_layout = gfx::vulkan::descriptor_set_layout_builder_t{}
            .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(context);
        
        s_renderer_data._voxels_r32ui = gfx::vulkan::image_builder_t{}
            .build3D(s_renderer_data._context, voxel_size,
                                               voxel_size,
                                               voxel_size,
                                               VK_FORMAT_R32_UINT, 
                                               VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        s_renderer_data._voxels_r32ui->transition_layout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        s_renderer_data._voxels_rgba8 = gfx::vulkan::image_builder_t{}
            .mip_maps()
            .build3D(s_renderer_data._context, voxel_size,
                                               voxel_size,
                                               voxel_size,
                                               VK_FORMAT_R8G8B8A8_UNORM, 
                                               VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        s_renderer_data._voxels_rgba8->transition_layout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        s_renderer_data._voxelization_dummy_image = gfx::vulkan::image_builder_t{}
            .build2D(s_renderer_data._context, voxel_size, voxel_size, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        s_renderer_data._depth_image = gfx::vulkan::image_builder_t{}
            .build2D(s_renderer_data._context, width, height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        #ifdef BOYBAYKILLER_TEST
        s_renderer_data._voxels_r64ui = gfx::vulkan::image_builder_t{}
            .build3D(s_renderer_data._context, voxel_size,
                                               voxel_size,
                                               voxel_size,
                                               VK_FORMAT_R64_UINT, 
                                               VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        s_renderer_data._voxels_r64ui->transition_layout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        #endif

        s_renderer_data._voxelization_gpu_timer = core::make_ref<gfx::vulkan::gpu_timer_t>(s_renderer_data._context);
        s_renderer_data._voxelization_boybaykiller_gpu_timer = core::make_ref<gfx::vulkan::gpu_timer_t>(s_renderer_data._context);
        s_renderer_data._voxel_clear_gpu_timer = core::make_ref<gfx::vulkan::gpu_timer_t>(s_renderer_data._context);
        s_renderer_data._voxel_copy_gpu_timer = core::make_ref<gfx::vulkan::gpu_timer_t>(s_renderer_data._context);
        s_renderer_data._voxel_mip_map_gpu_timer = core::make_ref<gfx::vulkan::gpu_timer_t>(s_renderer_data._context);
        s_renderer_data._depth_gpu_timer = core::make_ref<gfx::vulkan::gpu_timer_t>(s_renderer_data._context);

        s_renderer_data._voxelization_renderpass = gfx::vulkan::renderpass_builder_t{}
            .add_color_attachment(VkAttachmentDescription{
                .format = VK_FORMAT_R8G8B8A8_SRGB,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
            })
            .build(s_renderer_data._context);
        
        s_renderer_data._depth_renderpass = gfx::vulkan::renderpass_builder_t{}
            .set_depth_attachment(VkAttachmentDescription{
            .format = VK_FORMAT_D32_SFLOAT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        })
        .build(s_renderer_data._context);

        s_renderer_data._voxelization_framebuffer = gfx::vulkan::framebuffer_builder_t{}
            .add_attachment_view(s_renderer_data._voxelization_dummy_image->image_view())
            .build(s_renderer_data._context, s_renderer_data._voxelization_renderpass->renderpass(), voxel_size, voxel_size);

        s_renderer_data._depth_framebuffer = gfx::vulkan::framebuffer_builder_t{}
            .add_attachment_view(s_renderer_data._depth_image->image_view())
            .build(s_renderer_data._context, s_renderer_data._depth_renderpass->renderpass(), width, height);

        s_renderer_data._voxelization_descriptor_set_0_layout = gfx::vulkan::descriptor_set_layout_builder_t{}
            .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT)
            .build(s_renderer_data._context);
        
        s_renderer_data._voxelization_descriptor_set_2_layout = gfx::vulkan::descriptor_set_layout_builder_t{}
            .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(s_renderer_data._context);

        s_renderer_data._voxel_clear_descriptor_set_layout = gfx::vulkan::descriptor_set_layout_builder_t{}
            .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(s_renderer_data._context);

        s_renderer_data._voxel_copy_descriptor_set_layout = gfx::vulkan::descriptor_set_layout_builder_t{}
            .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT)
            .addLayoutBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(s_renderer_data._context);

        s_renderer_data._camera_uniform_descriptor_set_layout = gfx::vulkan::descriptor_set_layout_builder_t{}
            .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL)
            .build(s_renderer_data._context);

        s_renderer_data._voxelization_ubo = gfx::vulkan::buffer_builder_t{}
            .build(s_renderer_data._context, sizeof(glm::mat4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        for (int i = 0; i < s_renderer_data._context->MAX_FRAMES_IN_FLIGHT; i++) {
            auto camera_uniform = gfx::vulkan::buffer_builder_t{}
                .build(s_renderer_data._context, sizeof(camera_uniform_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            auto camera_uniform_descriptor_set = s_renderer_data._camera_uniform_descriptor_set_layout->new_descriptor_set();
            camera_uniform_descriptor_set->write()
                .pushBufferInfo(0, 1, camera_uniform->descriptor_info())
                .update();
            s_renderer_data._camera_unfiroms.push_back(camera_uniform);
            s_renderer_data._camera_uniform_descriptor_sets.push_back(camera_uniform_descriptor_set);
        }

        s_renderer_data._voxelization_descriptor_set_0 = s_renderer_data._voxelization_descriptor_set_0_layout->new_descriptor_set();
        s_renderer_data._voxelization_descriptor_set_2 = s_renderer_data._voxelization_descriptor_set_2_layout->new_descriptor_set();
        #ifdef BOYBAYKILLER_TEST
        s_renderer_data._voxelization_descriptor_set_2 = s_renderer_data._voxelization_descriptor_set_2_layout->new_descriptor_set();
        #endif

        s_renderer_data._voxelization_descriptor_set_0->write()
            .pushBufferInfo(0, 1, s_renderer_data._voxelization_ubo->descriptor_info())
            .update();
        
        s_renderer_data._voxelization_descriptor_set_2->write()
            .pushImageInfo(0, 1, s_renderer_data._voxels_r32ui->descriptor_info(VK_IMAGE_LAYOUT_GENERAL), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            .update();

        #ifdef BOYBAYKILLER_TEST
        s_renderer_data._voxelization_boybaykiller_descriptor_set_2->write()
            .pushImageInfo(0, 1, s_renderer_data._voxels_r32ui->descriptor_info(VK_IMAGE_LAYOUT_GENERAL), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            .update();
        #endif

        s_renderer_data._voxel_clear_descriptor_set = s_renderer_data._voxel_clear_descriptor_set_layout->new_descriptor_set();
        s_renderer_data._voxel_clear_descriptor_set->write()
            .pushImageInfo(0, 1, s_renderer_data._voxels_r32ui->descriptor_info(VK_IMAGE_LAYOUT_GENERAL), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            .update();

        s_renderer_data._voxel_copy_descriptor_set = s_renderer_data._voxel_copy_descriptor_set_layout->new_descriptor_set();
        s_renderer_data._voxel_copy_descriptor_set->write()
            .pushImageInfo(0, 1, s_renderer_data._voxels_rgba8->descriptor_info(VK_IMAGE_LAYOUT_GENERAL), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            .pushImageInfo(1, 1, s_renderer_data._voxels_r32ui->descriptor_info(VK_IMAGE_LAYOUT_GENERAL))
            .update();

        s_renderer_data._voxelization_pipeline = gfx::vulkan::pipeline_builder_t{}
            .add_shader("../../assets/new_shaders/voxelization/glsl.vert")
            .add_shader("../../assets/new_shaders/voxelization/glsl.frag")
            .add_push_constant_range(0, sizeof(int), VK_SHADER_STAGE_VERTEX_BIT)
            .add_descriptor_set_layout(s_renderer_data._voxelization_descriptor_set_0_layout)  // set 0
            .add_descriptor_set_layout(s_renderer_data._material_descriptor_set_layout)        // set 1
            .add_descriptor_set_layout(s_renderer_data._voxelization_descriptor_set_2_layout)  // set 2
            .add_default_color_blend_attachment_state()
            .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
            .add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
            .add_vertex_input_binding_description(0, sizeof(core::vertex_t), VK_VERTEX_INPUT_RATE_VERTEX)
            .add_vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, position))
            .add_vertex_input_attribute_description(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(core::vertex_t, uv))
            .build(s_renderer_data._context, s_renderer_data._voxelization_renderpass->renderpass());

        #ifdef BOYBAYKILLER_TEST
        s_renderer_data._voxelization_boybaykiller_pipeline = gfx::vulkan::pipeline_builder_t{}
            .add_shader("../../assets/new_shaders/voxelization/glsl.vert")
            .add_shader("../../assets/new_shaders/voxelization/glsl.frag")
            .add_push_constant_range(0, sizeof(int), VK_SHADER_STAGE_VERTEX_BIT)
            .add_descriptor_set_layout(s_renderer_data._voxelization_descriptor_set_0_layout)  // set 0
            .add_descriptor_set_layout(s_renderer_data._material_descriptor_set_layout)        // set 1
            .add_descriptor_set_layout(s_renderer_data._voxelization_descriptor_set_2_layout)  // set 2
            .add_default_color_blend_attachment_state()
            .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
            .add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
            .add_vertex_input_binding_description(0, sizeof(core::vertex_t), VK_VERTEX_INPUT_RATE_VERTEX)
            .add_vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, position))
            .add_vertex_input_attribute_description(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(core::vertex_t, uv))
            .build(s_renderer_data._context, s_renderer_data._voxelization_renderpass->renderpass());
        #endif

        s_renderer_data._voxel_clear_pipeline = gfx::vulkan::pipeline_builder_t{}
            .add_shader("../../assets/new_shaders/voxel_clear/glsl.comp")
            .add_descriptor_set_layout(s_renderer_data._voxel_clear_descriptor_set_layout)
            .build(s_renderer_data._context);

        s_renderer_data._voxel_copy_pipeline = gfx::vulkan::pipeline_builder_t{}
            .add_shader("../../assets/new_shaders/voxel_copy/glsl.comp")
            .add_descriptor_set_layout(s_renderer_data._voxel_copy_descriptor_set_layout)
            .build(s_renderer_data._context);

        s_renderer_data._depth_pipeline = gfx::vulkan::pipeline_builder_t{}
            .add_shader("../../assets/new_shaders/depth/glsl.vert")
            .add_shader("../../assets/new_shaders/depth/glsl.frag")
            .add_descriptor_set_layout(s_renderer_data._camera_uniform_descriptor_set_layout)  // set 0
            .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
            .add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
            .add_vertex_input_binding_description(0, sizeof(core::vertex_t), VK_VERTEX_INPUT_RATE_VERTEX)
            .add_vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, position))
            .build(s_renderer_data._context, s_renderer_data._depth_renderpass->renderpass());
    

        s_renderer_data._instantiated = true;
    }

    void destroy() {
        s_renderer_data._context->wait_idle();
        s_renderer_data = {};
    }

    core::ref<gfx::vulkan::descriptor_set_layout_t> get_material_descriptor_set_layout() {
        assert(s_renderer_data._instantiated);
        return s_renderer_data._material_descriptor_set_layout;
    }

    core::ref<gfx::vulkan::image_t> render_depth(VkCommandBuffer commandbuffer, uint32_t current_index, const editor_camera_t& editor_camera, const std::vector<draw_data_info_t> draw_data_infos) {
        VkClearValue clear_color{};
        clear_color.color = {0, 0, 0, 0};    
        VkClearValue clear_depth{};
        clear_depth.depthStencil.depth = 1;

        VkViewport swapchain_viewport{};
        swapchain_viewport.x = 0;
        swapchain_viewport.y = 0;
        swapchain_viewport.width = static_cast<float>(s_renderer_data._context->swapchain_extent().width);
        swapchain_viewport.height = static_cast<float>(s_renderer_data._context->swapchain_extent().height);
        swapchain_viewport.minDepth = 0;
        swapchain_viewport.maxDepth = 1;
        VkRect2D swapchain_scissor{};
        swapchain_scissor.offset = {0, 0};
        swapchain_scissor.extent = s_renderer_data._context->swapchain_extent();

        s_renderer_data._depth_gpu_timer->begin(commandbuffer);
        s_renderer_data._depth_renderpass->begin(commandbuffer, s_renderer_data._depth_framebuffer->framebuffer(), VkRect2D{
            .offset = {0, 0},
            .extent = s_renderer_data._context->swapchain_extent(),
        }, {
            clear_depth,
        });

        vkCmdSetViewport(commandbuffer, 0, 1, &swapchain_viewport);
        vkCmdSetScissor(commandbuffer, 0, 1, &swapchain_scissor);

        s_renderer_data._depth_pipeline->bind(commandbuffer);

        vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_renderer_data._depth_pipeline->pipeline_layout(), 0, 1, &s_renderer_data._camera_uniform_descriptor_sets[current_index]->descriptor_set(), 0, nullptr);

        for (int i = 0; i < draw_data_infos.size(); i++) {
            auto& draw_data_info = draw_data_infos[i];
            // vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_renderer_data._voxelization_pipeline->pipeline_layout(), 1, 1, &draw_data_info.gpu_mesh.material_descriptor_set->descriptor_set(), 0, nullptr);
            VkDeviceSize offsets{ 0 };
            vkCmdBindVertexBuffers(commandbuffer, 0, 1, &draw_data_info.gpu_mesh.vertex_buffer->buffer(), &offsets);
            vkCmdBindIndexBuffer(commandbuffer, draw_data_info.gpu_mesh.index_buffer->buffer(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandbuffer, draw_data_info.gpu_mesh.index_count, 1, 0, 0, 0);
        }
        
        s_renderer_data._depth_gpu_timer->end(commandbuffer);
        s_renderer_data._depth_renderpass->end(commandbuffer);

        return s_renderer_data._depth_image;
    }

    core::ref<gfx::vulkan::image_t> voxelize_scene(VkCommandBuffer commandbuffer, uint32_t current_index, const editor_camera_t& editor_camera, const std::vector<draw_data_info_t> draw_data_infos) {
        *reinterpret_cast<glm::mat4 *>(s_renderer_data._voxelization_ubo->map()) = utils::create_orthographic_off_center(s_renderer_data._voxelization_settings.grid_min.x, s_renderer_data._voxelization_settings.grid_max.x, s_renderer_data._voxelization_settings.grid_min.y, s_renderer_data._voxelization_settings.grid_max.y, s_renderer_data._voxelization_settings.grid_min.z, s_renderer_data._voxelization_settings.grid_max.z);

        VkClearValue clear_color{};
        clear_color.color = {0, 0, 0, 0};    
        VkClearValue clear_depth{};
        clear_depth.depthStencil.depth = 1;

        VkViewport swapchain_viewport{};
        swapchain_viewport.x = 0;
        swapchain_viewport.y = 0;
        swapchain_viewport.width = voxel_size;
        swapchain_viewport.height = voxel_size;
        swapchain_viewport.minDepth = 0;
        swapchain_viewport.maxDepth = 1;
        VkRect2D swapchain_scissor{};
        swapchain_scissor.offset = {0, 0};
        swapchain_scissor.extent = {voxel_size, voxel_size};
        
        #ifdef BOYBAYKILLER_TEST

        {
            // voxelize
            s_renderer_data._voxelization_boybaykiller_gpu_timer->begin(commandbuffer);
            s_renderer_data._voxelization_renderpass->begin(commandbuffer, s_renderer_data._voxelization_framebuffer->framebuffer(), VkRect2D{
                .offset = {0, 0},
                .extent = {voxel_size, voxel_size},
            }, {
                clear_color,
            }); 
            vkCmdSetViewport(commandbuffer, 0, 1, &swapchain_viewport);
            vkCmdSetScissor(commandbuffer, 0, 1, &swapchain_scissor);

            s_renderer_data._voxelization_boybaykiller_pipeline->bind(commandbuffer);

            vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_renderer_data._voxelization_boybaykiller_pipeline->pipeline_layout(), 0, 1, &s_renderer_data._voxelization_descriptor_set_0->descriptor_set(), 0, nullptr);
            vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_renderer_data._voxelization_boybaykiller_pipeline->pipeline_layout(), 2, 1, &s_renderer_data._voxelization_boybaykiller_descriptor_set_2->descriptor_set(), 0, nullptr);

            int render_axis = 0;

            render_axis = 0;
            vkCmdPushConstants(commandbuffer, s_renderer_data._voxelization_boybaykiller_pipeline->pipeline_layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(int), &render_axis);
            for (int i = 0; i < draw_data_infos.size(); i++) {
                auto& draw_data_info = draw_data_infos[i];
                vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_renderer_data._voxelization_boybaykiller_pipeline->pipeline_layout(), 1, 1, &draw_data_info.gpu_mesh.material_descriptor_set->descriptor_set(), 0, nullptr);
                VkDeviceSize offsets{ 0 };
                vkCmdBindVertexBuffers(commandbuffer, 0, 1, &draw_data_info.gpu_mesh.vertex_buffer->buffer(), &offsets);
                vkCmdBindIndexBuffer(commandbuffer, draw_data_info.gpu_mesh.index_buffer->buffer(), 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(commandbuffer, draw_data_info.gpu_mesh.index_count, 1, 0, 0, 0);
            }

            render_axis = 1;
            vkCmdPushConstants(commandbuffer, s_renderer_data._voxelization_boybaykiller_pipeline->pipeline_layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(int), &render_axis);
            for (int i = 0; i < draw_data_infos.size(); i++) {
                auto& draw_data_info = draw_data_infos[i];
                vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_renderer_data._voxelization_boybaykiller_pipeline->pipeline_layout(), 1, 1, &draw_data_info.gpu_mesh.material_descriptor_set->descriptor_set(), 0, nullptr);
                VkDeviceSize offsets{ 0 };
                vkCmdBindVertexBuffers(commandbuffer, 0, 1, &draw_data_info.gpu_mesh.vertex_buffer->buffer(), &offsets);
                vkCmdBindIndexBuffer(commandbuffer, draw_data_info.gpu_mesh.index_buffer->buffer(), 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(commandbuffer, draw_data_info.gpu_mesh.index_count, 1, 0, 0, 0);
            }

            render_axis = 2;
            vkCmdPushConstants(commandbuffer, s_renderer_data._voxelization_boybaykiller_pipeline->pipeline_layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(int), &render_axis);
            for (int i = 0; i < draw_data_infos.size(); i++) {
                auto& draw_data_info = draw_data_infos[i];
                vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_renderer_data._voxelization_boybaykiller_pipeline->pipeline_layout(), 1, 1, &draw_data_info.gpu_mesh.material_descriptor_set->descriptor_set(), 0, nullptr);
                VkDeviceSize offsets{ 0 };
                vkCmdBindVertexBuffers(commandbuffer, 0, 1, &draw_data_info.gpu_mesh.vertex_buffer->buffer(), &offsets);
                vkCmdBindIndexBuffer(commandbuffer, draw_data_info.gpu_mesh.index_buffer->buffer(), 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(commandbuffer, draw_data_info.gpu_mesh.index_count, 1, 0, 0, 0);
            }

            s_renderer_data._voxelization_boybaykiller_gpu_timer->end(commandbuffer);
            s_renderer_data._voxelization_renderpass->end(commandbuffer);
        }


        #endif
        
        // clear
        s_renderer_data._voxel_clear_pipeline->bind(commandbuffer);
        vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_COMPUTE, s_renderer_data._voxel_clear_pipeline->pipeline_layout(), 0, 1, &s_renderer_data._voxel_clear_descriptor_set->descriptor_set(), 0, nullptr);
        s_renderer_data._voxel_clear_gpu_timer->begin(commandbuffer);
        vkCmdDispatch(commandbuffer, (voxel_size + 4 - 1) / 4, (voxel_size + 4 - 1) / 4, (voxel_size + 2 - 1) / 2);
        s_renderer_data._voxel_clear_gpu_timer->end(commandbuffer);
        
        // voxelize
        s_renderer_data._voxelization_gpu_timer->begin(commandbuffer);
        s_renderer_data._voxelization_renderpass->begin(commandbuffer, s_renderer_data._voxelization_framebuffer->framebuffer(), VkRect2D{
            .offset = {0, 0},
            .extent = {voxel_size, voxel_size},
        }, {
            clear_color,
        }); 
        vkCmdSetViewport(commandbuffer, 0, 1, &swapchain_viewport);
        vkCmdSetScissor(commandbuffer, 0, 1, &swapchain_scissor);

        s_renderer_data._voxelization_pipeline->bind(commandbuffer);

        vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_renderer_data._voxelization_pipeline->pipeline_layout(), 0, 1, &s_renderer_data._voxelization_descriptor_set_0->descriptor_set(), 0, nullptr);
        vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_renderer_data._voxelization_pipeline->pipeline_layout(), 2, 1, &s_renderer_data._voxelization_descriptor_set_2->descriptor_set(), 0, nullptr);

        int render_axis = 0;

        render_axis = 0;
        vkCmdPushConstants(commandbuffer, s_renderer_data._voxelization_pipeline->pipeline_layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(int), &render_axis);
        for (int i = 0; i < draw_data_infos.size(); i++) {
            auto& draw_data_info = draw_data_infos[i];
            vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_renderer_data._voxelization_pipeline->pipeline_layout(), 1, 1, &draw_data_info.gpu_mesh.material_descriptor_set->descriptor_set(), 0, nullptr);
            VkDeviceSize offsets{ 0 };
            vkCmdBindVertexBuffers(commandbuffer, 0, 1, &draw_data_info.gpu_mesh.vertex_buffer->buffer(), &offsets);
            vkCmdBindIndexBuffer(commandbuffer, draw_data_info.gpu_mesh.index_buffer->buffer(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandbuffer, draw_data_info.gpu_mesh.index_count, 1, 0, 0, 0);
        }

        render_axis = 1;
        vkCmdPushConstants(commandbuffer, s_renderer_data._voxelization_pipeline->pipeline_layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(int), &render_axis);
        for (int i = 0; i < draw_data_infos.size(); i++) {
            auto& draw_data_info = draw_data_infos[i];
            vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_renderer_data._voxelization_pipeline->pipeline_layout(), 1, 1, &draw_data_info.gpu_mesh.material_descriptor_set->descriptor_set(), 0, nullptr);
            VkDeviceSize offsets{ 0 };
            vkCmdBindVertexBuffers(commandbuffer, 0, 1, &draw_data_info.gpu_mesh.vertex_buffer->buffer(), &offsets);
            vkCmdBindIndexBuffer(commandbuffer, draw_data_info.gpu_mesh.index_buffer->buffer(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandbuffer, draw_data_info.gpu_mesh.index_count, 1, 0, 0, 0);
        }

        render_axis = 2;
        vkCmdPushConstants(commandbuffer, s_renderer_data._voxelization_pipeline->pipeline_layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(int), &render_axis);
        for (int i = 0; i < draw_data_infos.size(); i++) {
            auto& draw_data_info = draw_data_infos[i];
            vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_renderer_data._voxelization_pipeline->pipeline_layout(), 1, 1, &draw_data_info.gpu_mesh.material_descriptor_set->descriptor_set(), 0, nullptr);
            VkDeviceSize offsets{ 0 };
            vkCmdBindVertexBuffers(commandbuffer, 0, 1, &draw_data_info.gpu_mesh.vertex_buffer->buffer(), &offsets);
            vkCmdBindIndexBuffer(commandbuffer, draw_data_info.gpu_mesh.index_buffer->buffer(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandbuffer, draw_data_info.gpu_mesh.index_count, 1, 0, 0, 0);
        }

        s_renderer_data._voxelization_gpu_timer->end(commandbuffer);
        s_renderer_data._voxelization_renderpass->end(commandbuffer);

        // copy
        s_renderer_data._voxel_copy_pipeline->bind(commandbuffer);
        vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_COMPUTE, s_renderer_data._voxel_copy_pipeline->pipeline_layout(), 0, 1, &s_renderer_data._voxel_copy_descriptor_set->descriptor_set(), 0, nullptr);
        s_renderer_data._voxel_copy_gpu_timer->begin(commandbuffer);
        vkCmdDispatch(commandbuffer, (voxel_size + 4 - 1) / 4, (voxel_size + 4 - 1) / 4, (voxel_size + 2 - 1) / 2);
        s_renderer_data._voxel_copy_gpu_timer->end(commandbuffer);

        // pipeline barrier
        VkImageMemoryBarrier image_memory_barrier{};
        image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_memory_barrier.image = s_renderer_data._voxels_rgba8->image();
        image_memory_barrier.subresourceRange.aspectMask = s_renderer_data._voxels_rgba8->aspect();
        image_memory_barrier.subresourceRange.baseMipLevel = 0;
        image_memory_barrier.subresourceRange.levelCount = 1;
        image_memory_barrier.subresourceRange.baseArrayLayer = 0;
        image_memory_barrier.subresourceRange.layerCount = 1;
        image_memory_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(commandbuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);

        // mip mapping 
        s_renderer_data._voxels_rgba8->genMipMaps(commandbuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);        

        return s_renderer_data._voxels_rgba8;
    }

    core::ref<gfx::vulkan::image_t> voxel_visualization(VkCommandBuffer commandbuffer, uint32_t current_index, const editor_camera_t& editor_camera) {
        return {};
    }

    void imgui_display() {
        ImGui::Begin("Renderer");

        #ifdef BOYBAYKILLER_TEST
        static float last_voxelization_boybaykiller_time = 0;
        if (auto t = s_renderer_data._voxelization_boybaykiller_gpu_timer->get_time()) {
            last_voxelization_boybaykiller_time = *t;
        } 
        ImGui::Text("voxelization (boybaykiller test) took: %f", last_voxelization_boybaykiller_time);
        #endif

        static float last_clear_time = 0;
        if (auto t = s_renderer_data._voxel_clear_gpu_timer->get_time()) {
            last_clear_time = *t;
        } 
        ImGui::Text("voxel clear took: %f", last_clear_time);

        static float last_voxelization_time = 0;
        if (auto t = s_renderer_data._voxelization_gpu_timer->get_time()) {
            last_voxelization_time = *t;
        } 
        ImGui::Text("voxelization took: %f", last_voxelization_time);

        static float last_voxel_copy_time = 0;
        if (auto t = s_renderer_data._voxel_copy_gpu_timer->get_time()) {
            last_voxel_copy_time = *t;
        } 
        ImGui::Text("voxel copy took: %f", last_voxel_copy_time);
        

        ImGui::End();
    }

    void render(VkCommandBuffer commandbuffer, uint32_t current_index, const editor_camera_t& editor_camera, const std::vector<draw_data_info_t> draw_data_infos) {
        auto& camera_uniform = *reinterpret_cast<camera_uniform_t *>(s_renderer_data._camera_unfiroms[current_index]->map());
        camera_uniform.view = editor_camera.view();
        camera_uniform.projection = editor_camera.projection();
        camera_uniform.inverse_view = editor_camera.inverse_view();
        camera_uniform.inverse_projection = editor_camera.inverse_projection();
        camera_uniform.projection_view = camera_uniform.projection * camera_uniform.view;

        auto depth = render_depth(commandbuffer, current_index, editor_camera, draw_data_infos);
        auto voxels = voxelize_scene(commandbuffer, current_index, editor_camera, draw_data_infos);
    }

} // namespace renderer