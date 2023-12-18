#ifndef CORE_MATERIAL_HPP
#define CORE_MATERIAL_HPP

#include "gfx/vulkan/descriptor.hpp"
#include "gfx/vulkan/image.hpp"

#include <memory>

namespace core {

struct Material {
    core::ref<gfx::vulkan::descriptor_set_t> descriptor_set;

    core::ref<gfx::vulkan::image_t> diffuse;
    core::ref<gfx::vulkan::image_t> specular;
    core::ref<gfx::vulkan::image_t> normal;

    void update();

    static void init(core::ref<gfx::vulkan::context_t> context);
    static core::ref<gfx::vulkan::descriptor_set_layout_t> getMaterialDescriptorSetLayout();
    static void destroy();
};

} // namespace core

#endif