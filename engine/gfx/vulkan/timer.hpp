#ifndef GFX_VULKAN_TIMER_HPP
#define GFX_VULKAN_TIMER_HPP

#include "context.hpp"

#include <optional>

namespace gfx {
    
namespace vulkan {

class Timer {
public:
    Timer(core::ref<Context> context);
    ~Timer();

    void begin(VkCommandBuffer commandBuffer);
    void end(VkCommandBuffer commandBuffer);

    std::optional<float> getTime();

private:
    core::ref<Context> m_context{};
    VkQueryPool m_queryPool{};
};

} // namespace vulkan

} // namespace gfx

#endif