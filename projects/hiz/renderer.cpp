#include "renderer.hpp"

#define MAX_INDIRECT_COMMANDS 100  // for now

static core::ref<gfx::vulkan::descriptor_set_layout_t> s_material_descriptor_set_layout;

static void make_material_descriptor_set(core::ref<gfx::vulkan::context_t> context) {
    s_material_descriptor_set_layout = gfx::vulkan::descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(context);
}

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

renderer_t::renderer_t(core::ref<core::window_t> window, core::ref<gfx::vulkan::context_t> context) 
    : _window(window),
    _context(context) {

    if (!s_material_descriptor_set_layout) {
        make_material_descriptor_set(_context);
    }

    auto [width, height] = _window->get_dimensions();
    
    _depth_pre_renderpass = gfx::vulkan::renderpass_builder_t{}
        .set_depth_attachment(VkAttachmentDescription{
            .format = VK_FORMAT_D32_SFLOAT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        })
        .build(_context);

    _deferred_renderpass = gfx::vulkan::renderpass_builder_t{}
        .add_color_attachment(VkAttachmentDescription{
            .format = VK_FORMAT_R8G8B8A8_SRGB,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
        })
        .set_depth_attachment(VkAttachmentDescription{
            .format = VK_FORMAT_D32_SFLOAT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,  // prepass
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
        })
        .build(_context);

    _depth_pre_gpu_timer = core::make_ref<gfx::vulkan::gpu_timer_t>(_context);

    _deferred_gpu_timer = core::make_ref<gfx::vulkan::gpu_timer_t>(_context);

    _depth_image = gfx::vulkan::image_builder_t{}
        .build2D(_context, width, height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    _hiz_image = gfx::vulkan::image_builder_t{}
        .mip_maps()
        .build2D(_context, power_of_2_before(width), power_of_2_before(height), VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    _hiz_image->transition_layout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    _gbuffer_albedo_image = gfx::vulkan::image_builder_t{}
        .build2D(_context, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    _depth_pre_framebuffer = gfx::vulkan::framebuffer_builder_t{}
        .add_attachment_view(_depth_image->image_view(1, 0))
         .build(_context, _depth_pre_renderpass->renderpass(), width, height);

    _deferred_framebuffer = gfx::vulkan::framebuffer_builder_t{}
        .add_attachment_view(_gbuffer_albedo_image->image_view())
        .add_attachment_view(_depth_image->image_view(1, 0))
        .build(_context, _deferred_renderpass->renderpass(), width, height);

    _camera_uniform_descriptor_set_layout = gfx::vulkan::descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS)
        .build(_context);
    
    _storage_sampler_image_descriptor_set_layout = gfx::vulkan::descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .addLayoutBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .build(_context);

    _culling_descriptor_set_layout = gfx::vulkan::descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .addLayoutBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .addLayoutBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .addLayoutBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .build(_context);

    _storage_sampler_image_copy_descriptor_set = _storage_sampler_image_descriptor_set_layout->new_descriptor_set();
    _storage_sampler_image_copy_descriptor_set->write()
        .pushImageInfo(0, 1, _hiz_image->descriptor_info(VK_IMAGE_LAYOUT_GENERAL, 1, 0), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
        .pushImageInfo(1, 1, _depth_image->descriptor_info(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
        .update();
    
    for (uint32_t i = 0; i < _hiz_image->level_count() - 1; i++) {
        auto _storage_sampler_image_hiz_gen_descriptor_set = _storage_sampler_image_descriptor_set_layout->new_descriptor_set();
        _storage_sampler_image_hiz_gen_descriptor_set->write()
            .pushImageInfo(0, 1, _hiz_image->descriptor_info(VK_IMAGE_LAYOUT_GENERAL, 1, i + 1), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            .pushImageInfo(1, 1, _hiz_image->descriptor_info(VK_IMAGE_LAYOUT_GENERAL, 1, i))
            .update();
        _storage_sampler_image_hiz_gen_descriptor_sets.push_back(_storage_sampler_image_hiz_gen_descriptor_set);
    }

    for (uint32_t i = 0; i < _context->MAX_FRAMES_IN_FLIGHT; i++) {
        auto camera_uniform = gfx::vulkan::buffer_builder_t{}
            .build(_context, sizeof(camera_uniform_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        auto camera_uniform_descriptor_set = _camera_uniform_descriptor_set_layout->new_descriptor_set();
        camera_uniform_descriptor_set->write()
            .pushBufferInfo(0, 1, camera_uniform->descriptor_info())
            .update();
        _camera_uniform_descriptor_sets.push_back(camera_uniform_descriptor_set);
        _camera_uniforms.push_back(camera_uniform);
        
        auto indirect_draw = gfx::vulkan::buffer_builder_t{}
            .build(_context, sizeof(VkDrawIndexedIndirectCommand) * MAX_INDIRECT_COMMANDS, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        _indirect_draws.push_back(indirect_draw);

        auto aabbs = gfx::vulkan::buffer_builder_t{}
            .build(_context, sizeof(core::aabb_t) * MAX_INDIRECT_COMMANDS, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        _aabbs.push_back(aabbs);

        auto culling_settings = gfx::vulkan::buffer_builder_t{}
            .build(_context, sizeof(culling_settings_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        _culling_settings.push_back(culling_settings);

        auto culling_descriptor_set = _culling_descriptor_set_layout->new_descriptor_set();
        culling_descriptor_set->write()
            .pushBufferInfo(0, 1, culling_settings->descriptor_info())
            .pushBufferInfo(1, 1, aabbs->descriptor_info(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .pushBufferInfo(2, 1, indirect_draw->descriptor_info(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .pushImageInfo(3, 1, _hiz_image->descriptor_info(VK_IMAGE_LAYOUT_GENERAL))
            .update();
        _culling_descriptor_sets.push_back(culling_descriptor_set);
    }

    _depth_pre_pipeline = gfx::vulkan::pipeline_builder_t{}
        .add_shader("../../assets/new_shaders/depth/glsl.vert")
        .add_shader("../../assets/new_shaders/depth/glsl.frag")
        .add_descriptor_set_layout(_camera_uniform_descriptor_set_layout)  // set 0
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
        .add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .add_vertex_input_binding_description(0, sizeof(core::vertex_t), VK_VERTEX_INPUT_RATE_VERTEX)
        .add_vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, position))
        .build(_context, _depth_pre_renderpass->renderpass());
    
    _deferred_pipeline = gfx::vulkan::pipeline_builder_t{}
        .add_shader("../../assets/new_shaders/deferred/glsl.vert")
        .add_shader("../../assets/new_shaders/deferred/glsl.frag")
        .add_descriptor_set_layout(_camera_uniform_descriptor_set_layout)
        .add_descriptor_set_layout(get_material_descriptor_set())
        .add_default_color_blend_attachment_state()
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
        .add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .set_depth_stencil_state(VkPipelineDepthStencilStateCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = VK_TRUE,
			.depthWriteEnable = VK_TRUE,
			.depthCompareOp = VK_COMPARE_OP_EQUAL,
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable = VK_FALSE,
			.front = {},
			.back = {},
			.minDepthBounds = 0,
			.maxDepthBounds = 1,
		})
        .add_vertex_input_binding_description(0, sizeof(core::vertex_t), VK_VERTEX_INPUT_RATE_VERTEX)
        .add_vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, position))
        .add_vertex_input_attribute_description(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(core::vertex_t, uv))
        .build(context, _deferred_renderpass->renderpass());

    _copy_pipeline = gfx::vulkan::pipeline_builder_t{}
        .add_descriptor_set_layout(_storage_sampler_image_descriptor_set_layout)
        .add_shader("../../assets/new_shaders/copy/glsl.comp")
        .build(_context);
    
    _gen_hiz_mips_pipeine = gfx::vulkan::pipeline_builder_t{}
        .add_descriptor_set_layout(_storage_sampler_image_descriptor_set_layout)
        .add_shader("../../assets/new_shaders/hiz_gen/glsl.comp")
        .build(_context);
    
    _culling_pipeline = gfx::vulkan::pipeline_builder_t{}
        .add_descriptor_set_layout(_culling_descriptor_set_layout)
        .add_shader("../../assets/new_shaders/cull/glsl.comp")
        .build(_context);
}

renderer_t::~renderer_t() {
    if (s_material_descriptor_set_layout) 
        s_material_descriptor_set_layout.reset();
}

core::ref<gfx::vulkan::descriptor_set_layout_t> renderer_t::get_material_descriptor_set() {
    if (!s_material_descriptor_set_layout) {
        ERROR("material descriptor set layout not built");
        throw std::runtime_error("");
    } 

    return s_material_descriptor_set_layout;
}

void renderer_t::render(VkCommandBuffer commandbuffer, uint32_t current_index, const editor_camera_t& editor_camera, const std::vector<draw_data_info_t> draw_data_infos) {
    // potentially sort, cull and batch draw cmds
    // assuming the draws are sorted by material and meshes (only for now)
    auto final_draw_data_infos = draw_data_infos;
    assert(final_draw_data_infos.size() < MAX_INDIRECT_COMMANDS);

    if (final_draw_data_infos.size() >= MAX_INDIRECT_COMMANDS) {
        ERROR("more than 100 indirect commands");
        std::terminate();
    }

    auto [width, height] = _context->swapchain_extent(); 
    frustum_t frustum{editor_camera, float(width) / float(height), 1, editor_camera.near(), editor_camera.far()};

    auto indirect_draw = reinterpret_cast<VkDrawIndexedIndirectCommand *>(_indirect_draws[current_index]->map());
    auto aabb_data = reinterpret_cast<core::aabb_t *>(_aabbs[current_index]->map());
    auto culling_data = reinterpret_cast<culling_settings_t *>(_culling_settings[current_index]->map());

    for (int i = 0; i < final_draw_data_infos.size(); i++) {
        auto& p = indirect_draw[i];
        p.firstIndex = 0;
        p.firstInstance = 0;
        p.indexCount = final_draw_data_infos[i].gpu_mesh.index_count;
        p.vertexOffset = 0;
        p.instanceCount = 0;

        aabb_data[i] = final_draw_data_infos[i].gpu_mesh.aabb;
    }    

    culling_data->bottom_face = frustum.bottom_face;
    culling_data->top_face = frustum.top_face;
    culling_data->right_face = frustum.right_face;
    culling_data->left_face = frustum.left_face;
    culling_data->near_face = frustum.near_face;
    culling_data->far_face = frustum.far_face;
    culling_data->p00 = editor_camera.getProjection()[0][0];
    culling_data->p11 = editor_camera.getProjection()[1][1];
    culling_data->far = editor_camera.far();
    culling_data->near = editor_camera.near();
    auto [hiz_width, hiz_height] = _hiz_image->dimensions();
    culling_data->hiz_width = static_cast<float>(hiz_width);
    culling_data->hiz_height = static_cast<float>(hiz_height);

    culling_data->size = final_draw_data_infos.size();

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

    static bool first_frame = true;
    if (first_frame)
        first_frame = false;
    else {
        vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _copy_pipeline->pipeline_layout(), 0, 1, &_storage_sampler_image_copy_descriptor_set->descriptor_set(), 0, 0);
        _copy_pipeline->bind(commandbuffer);
        auto [width, height] = _hiz_image->dimensions();
        vkCmdDispatch(commandbuffer, width / 8, height / 4, 1);

        VkImageMemoryBarrier image_memory_barrier{};
        image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_memory_barrier.image = _hiz_image->image();
        image_memory_barrier.subresourceRange.aspectMask = _hiz_image->aspect();
        image_memory_barrier.subresourceRange.baseMipLevel = 0;
        image_memory_barrier.subresourceRange.levelCount = 1;
        image_memory_barrier.subresourceRange.baseArrayLayer = 0;
        image_memory_barrier.subresourceRange.layerCount = 1;
        image_memory_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(commandbuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);

        for (uint32_t i = 0; i < _hiz_image->mip_levels() - 1; i++) {
            vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _gen_hiz_mips_pipeine->pipeline_layout(), 0, 1, &_storage_sampler_image_hiz_gen_descriptor_sets[i]->descriptor_set(), 0, 0);
            _gen_hiz_mips_pipeine->bind(commandbuffer);
            auto [width, height] = _context->swapchain_extent();
            glm::vec2 size { width / (1 << (i + 1)), height / (1 << (i + 1))};
            vkCmdDispatch(commandbuffer, std::max(1, int((size.x + 8 - 1) / 8)), std::max(1, int((size.y + 4 - 1) / 4)), 1);

            VkImageMemoryBarrier image_memory_barrier{};
            image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            image_memory_barrier.image = _hiz_image->image();
            image_memory_barrier.subresourceRange.aspectMask = _hiz_image->aspect();
            image_memory_barrier.subresourceRange.baseMipLevel = i + 1;
            image_memory_barrier.subresourceRange.levelCount = 1;
            image_memory_barrier.subresourceRange.baseArrayLayer = 0;
            image_memory_barrier.subresourceRange.layerCount = 1;
            image_memory_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(commandbuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);
        }
    }

    vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _culling_pipeline->pipeline_layout(), 0, 1, &_culling_descriptor_sets[current_index]->descriptor_set(), 0, 0);
    _culling_pipeline->bind(commandbuffer);
    vkCmdDispatch(commandbuffer, (final_draw_data_infos.size() / 32) + 1, 1, 1);

    VkBufferMemoryBarrier buffer_memory_barrier{};
    buffer_memory_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    buffer_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    buffer_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    buffer_memory_barrier.buffer = _indirect_draws[current_index]->buffer();
    buffer_memory_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    buffer_memory_barrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    buffer_memory_barrier.offset = 0;
    buffer_memory_barrier.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(commandbuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, nullptr, 1, &buffer_memory_barrier, 0, nullptr);
    
    camera_uniform_t camera_uniform{};
    camera_uniform.view = editor_camera.getView();
    camera_uniform.projection = editor_camera.getProjection();
    camera_uniform.projection_view = camera_uniform.projection * camera_uniform.view;
    camera_uniform.inverse_projection = glm::inverse(camera_uniform.projection);
    camera_uniform.inverse_view = glm::inverse(camera_uniform.view);
    std::memcpy(_camera_uniforms[current_index]->map(), &camera_uniform, sizeof(camera_uniform));        

    _depth_pre_gpu_timer->begin(commandbuffer);
    _depth_pre_renderpass->begin(commandbuffer, _depth_pre_framebuffer->framebuffer(), VkRect2D{
        .offset = {0, 0},
        .extent = _context->swapchain_extent(),
    }, {
        clear_depth,
    });
    vkCmdSetViewport(commandbuffer, 0, 1, &swapchain_viewport);
    vkCmdSetScissor(commandbuffer, 0, 1, &swapchain_scissor);

    _depth_pre_pipeline->bind(commandbuffer);

    vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _depth_pre_pipeline->pipeline_layout(), 0, 1, &_camera_uniform_descriptor_sets[current_index]->descriptor_set(), 0, 0);
    
    for (int i = 0; i < final_draw_data_infos.size(); i++) {
        auto& draw_data_info = final_draw_data_infos[i];
        VkDeviceSize offsets{ 0 };
        vkCmdBindVertexBuffers(commandbuffer, 0, 1, &draw_data_info.gpu_mesh.vertex_buffer->buffer(), &offsets);
        vkCmdBindIndexBuffer(commandbuffer, draw_data_info.gpu_mesh.index_buffer->buffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexedIndirect(commandbuffer, _indirect_draws[current_index]->buffer(), i * sizeof(VkDrawIndexedIndirectCommand), 1, sizeof(VkDrawIndexedIndirectCommand));
    }

    // // todo: change this
    // int i = 0;
    // for (auto draw_data_info : draw_data_infos) {
    //     // aabb_t aabb{draw_data_info.gpu_mesh.aabb.min, draw_data_info.gpu_mesh.aabb.max};
//     // if (frustum.is_on_frustum(aabb)) {
    //         VkDeviceSize offsets{ 0 };
    //         vkCmdBindVertexBuffers(commandbuffer, 0, 1, &draw_data_info.gpu_mesh.vertex_buffer->buffer(), &offsets);
    //         vkCmdBindIndexBuffer(commandbuffer, draw_data_info.gpu_mesh.index_buffer->buffer(), 0, VK_INDEX_TYPE_UINT32);
    //         // vkCmdDrawIndexed(commandbuffer, draw_data_info.gpu_mesh.index_count, 1, 0, 0, 0);
    //     // }
    //     vkCmdDrawIndexedIndirect(commandbuffer, _indirect_draws[current_index]->buffer(), i * sizeof(VkDrawIndexedIndirectCommand), 1, sizeof(VkDrawIndexedIndirectCommand));
    //     i++;
    // }

    _depth_pre_gpu_timer->end(commandbuffer);
    _depth_pre_renderpass->end(commandbuffer);

    _deferred_gpu_timer->begin(commandbuffer);
    _deferred_renderpass->begin(commandbuffer, _deferred_framebuffer->framebuffer(), VkRect2D{
        .offset = {0, 0},
        .extent = _context->swapchain_extent(),
    }, {
        clear_color,
        clear_depth,
    }); 
    vkCmdSetViewport(commandbuffer, 0, 1, &swapchain_viewport);
    vkCmdSetScissor(commandbuffer, 0, 1, &swapchain_scissor);

    _deferred_pipeline->bind(commandbuffer);

    vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _deferred_pipeline->pipeline_layout(), 0, 1, &_camera_uniform_descriptor_sets[current_index]->descriptor_set(), 0, 0);


    for (int i = 0; i < final_draw_data_infos.size(); i++) {
        auto& draw_data_info = final_draw_data_infos[i];
        vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _deferred_pipeline->pipeline_layout(), 1, 1, &draw_data_info.gpu_mesh.material_descriptor_set->descriptor_set(), 0, 0);
        VkDeviceSize offsets{ 0 };
        vkCmdBindVertexBuffers(commandbuffer, 0, 1, &draw_data_info.gpu_mesh.vertex_buffer->buffer(), &offsets);
        vkCmdBindIndexBuffer(commandbuffer, draw_data_info.gpu_mesh.index_buffer->buffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexedIndirect(commandbuffer, _indirect_draws[current_index]->buffer(), i * sizeof(VkDrawIndexedIndirectCommand), 1, sizeof(VkDrawIndexedIndirectCommand));
    }

    // // todo: change this
    // i = 0;
    // for (auto draw_data_info : draw_data_infos) {
    //     // aabb_t aabb{draw_data_info.gpu_mesh.aabb.min, draw_data_info.gpu_mesh.aabb.max};
    //     // if (frustum.is_on_frustum(aabb)) {
    //         vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _deferred_pipeline->pipeline_layout(), 1, 1, &draw_data_info.gpu_mesh.material_descriptor_set->descriptor_set(), 0, 0);
    //         VkDeviceSize offsets{ 0 };
    //         vkCmdBindVertexBuffers(commandbuffer, 0, 1, &draw_data_info.gpu_mesh.vertex_buffer->buffer(), &offsets);
    //         vkCmdBindIndexBuffer(commandbuffer, draw_data_info.gpu_mesh.index_buffer->buffer(), 0, VK_INDEX_TYPE_UINT32);
    //         // vkCmdDrawIndexed(commandbuffer, draw_data_info.gpu_mesh.index_count, 1, 0, 0, 0);
    //     // }
    //     vkCmdDrawIndexedIndirect(commandbuffer, _indirect_draws[current_index]->buffer(), i * sizeof(VkDrawIndexedIndirectCommand), 1, sizeof(VkDrawIndexedIndirectCommand));
    //     i++;
    // }

    _deferred_gpu_timer->end(commandbuffer);
    _deferred_renderpass->end(commandbuffer);

}