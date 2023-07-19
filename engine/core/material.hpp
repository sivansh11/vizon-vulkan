#ifndef CORE_MATERIAL_HPP
#define CORE_MATERIAL_HPP

#include "gfx/vulkan/descriptor.hpp"
#include "gfx/vulkan/image.hpp"

#include <memory>

namespace core {

struct Material {
    core::ref<gfx::vulkan::DescriptorSet> descriptorSet;

    core::ref<gfx::vulkan::Image> diffuse;
    // core::ref<gfx::vulkan::Image> specular;
    // core::ref<gfx::vulkan::Image> normal;

    void update();

    static void init(core::ref<gfx::vulkan::Context> context);
    static core::ref<gfx::vulkan::DescriptorSetLayout> getMaterialDescriptorSetLayout();
    static void destroy();
};

} // namespace core

#endif