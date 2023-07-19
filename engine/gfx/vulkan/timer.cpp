#include "timer.hpp"

#include "core/log.hpp"

namespace gfx {

namespace vulkan {

Timer::Timer(core::ref<Context> context)
  : m_context(context) {
    VkQueryPoolCreateInfo queryPoolCreateInfo{};
    queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryPoolCreateInfo.queryCount = 2;
    queryPoolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;

    if (vkCreateQueryPool(context->device(), &queryPoolCreateInfo, nullptr, &m_queryPool) != VK_SUCCESS) {
        ERROR("Failed to create query pool");
        std::terminate();
    }
}

Timer::~Timer() {
    vkDestroyQueryPool(m_context->device(), m_queryPool, nullptr);
}

void Timer::begin(VkCommandBuffer commandBuffer) {
    vkCmdResetQueryPool(commandBuffer, m_queryPool, 0, 2);
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_queryPool, 0);
}

void Timer::end(VkCommandBuffer commandBuffer) {
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPool, 1);
}

std::optional<float> Timer::getTime() {
    uint64_t timeStamps[2];
    auto result = vkGetQueryPoolResults(m_context->device(), m_queryPool, 0, 2, sizeof(uint64_t) * 2, timeStamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
    if (result == VK_NOT_READY) {
        return std::nullopt;
    } else if (result != VK_SUCCESS) {
        WARN("not successful time stamp");
        return std::nullopt;
    }
    return ((timeStamps[1] - timeStamps[0]) * m_context->physicalDeviceProperties().limits.timestampPeriod) / 1000000.f;
}

} // namespace vulkan

} // namespace gfx
