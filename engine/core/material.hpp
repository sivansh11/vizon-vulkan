#ifndef CORE_MATERIAL_HPP
#define CORE_MATERIAL_HPP

#include "gfx/vulkan/descriptor.hpp"
#include "gfx/vulkan/image.hpp"

#include <memory>

namespace core {

struct Material {
    std::shared_ptr<gfx::vulkan::DescriptorSet> descriptorSet;

    std::shared_ptr<gfx::vulkan::Image> diffuse;
    // std::shared_ptr<gfx::vulkan::Image> specular;
    // std::shared_ptr<gfx::vulkan::Image> normal;

    void update();

    static void init(std::shared_ptr<gfx::vulkan::Context> context);
    static std::shared_ptr<gfx::vulkan::DescriptorSetLayout> getMaterialDescriptorSetLayout();
    static void destroy();
};

} // namespace core

#endif