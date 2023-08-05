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
#include "gfx/vulkan/acceleration_structure.hpp"

#include "core/core.hpp"
#include "core/imgui_utils.hpp"
#include "core/window.hpp"

using namespace core;
using namespace gfx::vulkan;

class renderer_t {
public:
    renderer_t(ref<window_t> window, ref<context_t> context);
    ~renderer_t();

    void render(VkCommandBuffer commandbuffer, uint32_t current_index);

    VkDescriptorSet get_color() { return _final_color_descriptor_set->descriptor_set(); }
    VkDescriptorSet get_depth() { return _final_depth_descriptor_set->descriptor_set(); }

private:
    ref<window_t>  _window;
    ref<context_t> _context;

    VkFormat _depth_format;
    VkFormat _color_format;
    
    ref<renderpass_t> _depth_prepass_renderpass;
    ref<framebuffer_t> _depth_only_framebuffer; 

    ref<renderpass_t> _final_renderpass;
    ref<image_t> _final_color_image;
    ref<image_t> _final_depth_image;
    ref<framebuffer_t> _final_framebuffer;

    ref<descriptor_set_layout_t> _final_descriptor_set_layout;
    ref<descriptor_set_t> _final_color_descriptor_set;
    ref<descriptor_set_t> _final_depth_descriptor_set;


};

#endif