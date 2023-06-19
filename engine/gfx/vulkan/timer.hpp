#ifndef GFX_VULKAN_TIMER_HPP
#define GFX_VULKAN_TIMER_HPP

#include "context.hpp"

#include <optional>

namespace gfx {
    
namespace vulkan {

class Timer {
public:
    Timer(std::shared_ptr<Context> context);
    ~Timer();

    void begin(VkCommandBuffer commandBuffer);
    void end(VkCommandBuffer commandBuffer);

    std::optional<float> getTime();

private:
    std::shared_ptr<Context> m_context{};
    VkQueryPool m_queryPool{};
};

} // namespace vulkan

} // namespace gfx

#endif