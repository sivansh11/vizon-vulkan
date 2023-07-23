#ifndef GFX_VULKAN_TIMER_HPP
#define GFX_VULKAN_TIMER_HPP

#include "context.hpp"

#include <optional>

namespace gfx {
    
namespace vulkan {

// GPU side profiler
class Timer {
public:
    Timer(core::ref<context_t> context);
    ~Timer();

    void begin(VkCommandBuffer commandBuffer);
    void end(VkCommandBuffer commandBuffer);

    std::optional<float> getTime();

private:
    core::ref<context_t> m_context{};
    VkQueryPool m_queryPool{};
};

} // namespace vulkan

} // namespace gfx

#endif