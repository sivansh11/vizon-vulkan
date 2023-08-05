#include "renderer.hpp"

renderer_t::renderer_t(ref<window_t> window, ref<context_t> context) {
    _window = window;
    _context = context;

    auto [width, height] = _window->get_dimensions();

    _depth_format = VK_FORMAT_D32_SFLOAT;
    _color_format = VK_FORMAT_R8G8B8A8_UNORM;

    _final_color_image = image_builder_t{}   
        .build2D(_context, width, height, _color_format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    _final_depth_image = image_builder_t{}   
        .build2D(_context, width, height, _depth_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    _depth_prepass_renderpass = renderpass_builder_t{}
        .set_depth_attachment(VkAttachmentDescription{
            .format = _depth_format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,                  
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
        })
        .build(_context);

    _depth_only_framebuffer = framebuffer_builder_t{}
        .add_attachment_view(_final_depth_image->image_view())
        .build(_context, _depth_prepass_renderpass->renderpass(), width, height);

    _final_renderpass = renderpass_builder_t{}
        .add_color_attachment(VkAttachmentDescription{
            .format = _color_format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
        })
        .set_depth_attachment(VkAttachmentDescription{
            .format = _depth_format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,                  // depth prepass
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
        })
        .build(_context);

    _final_framebuffer = framebuffer_builder_t{}
        .add_attachment_view(_final_color_image->image_view())
        .add_attachment_view(_final_depth_image->image_view())
        .build(_context, _final_renderpass->renderpass(), width, height);
        
    _final_descriptor_set_layout = descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(_context);
    
    _final_color_descriptor_set = _final_descriptor_set_layout->new_descriptor_set();
    _final_color_descriptor_set->write()
        .pushImageInfo(0, 1, _final_color_image->descriptor_info(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
        .update();
    
    _final_depth_descriptor_set = _final_descriptor_set_layout->new_descriptor_set();
    _final_depth_descriptor_set->write()
        .pushImageInfo(0, 1, _final_depth_image->descriptor_info(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
        .update();
}

renderer_t::~renderer_t() {

}

void renderer_t::render(VkCommandBuffer commandbuffer, uint32_t current_index) {
    VkClearValue clear_color{};
    clear_color.color = {1, 0, 0, 1};    
    VkClearValue clear_depth{};
    clear_depth.depthStencil.depth = 1;
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(_context->swapchain_extent().width);
    viewport.height = static_cast<float>(_context->swapchain_extent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = _context->swapchain_extent();   

    _depth_prepass_renderpass->begin(commandbuffer, _depth_only_framebuffer->framebuffer(), VkRect2D{
        .offset = {0, 0},
        .extent = _context->swapchain_extent(),
    }, {
        clear_depth,
    }); 

    _depth_prepass_renderpass->end(commandbuffer);

    _final_renderpass->begin(commandbuffer, _final_framebuffer->framebuffer(), VkRect2D{
        .offset = {0, 0},
        .extent = _context->swapchain_extent(),
    }, {
        clear_color,
        clear_depth,
    }); 


    _final_renderpass->end(commandbuffer);
}