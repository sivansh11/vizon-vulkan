#ifndef GFX_VULKAN_CONTEXT_HPP
#define GFX_VULKAN_CONTEXT_HPP

#define GLFW_INCLUDE_VULKAN
#define VK_NO_PROTOTYPES
#include <GLFW/glfw3.h>

#include "core/window.hpp"

#include <volk.h>

#include <memory>
#include <vector>

#include "core/window.hpp"

#include <volk.h>

#include <memory>
#include <vector>
#include <optional>
#include <functional>

namespace gfx {

namespace vulkan {

class Context {
public:
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    Context(std::shared_ptr<core::Window> window, uint32_t MAX_FRAMES_IN_FLIGHT, bool validation);
    ~Context();

    std::optional<std::pair<VkCommandBuffer, uint32_t>> startFrame();
    bool endFrame(VkCommandBuffer commandBuffer);

    void beginSwapChainRenderPass(VkCommandBuffer commandBuffer, const VkClearValue& clearValue);
    void endSwapChainRenderPass(VkCommandBuffer commandBuffer);

    VkCommandBuffer startSingleUseCommandBuffer();
    void endSingleUseCommandBuffer(VkCommandBuffer commandBuffer);
    void singleUseCommandBuffer(std::function<void(VkCommandBuffer)> fn);

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    VkDevice& device() { return m_device; }
    VkSwapchainKHR& swapChain() { return m_swapChain; }

    VkFormat& swapChainFormat() { return m_swapChainFormat; }
    VkExtent2D& swapChainExtent() { return m_swapChainExtent; }

    VkQueue& graphicsQueue() { return m_graphicsQueue; }
    VkQueue& presentQueue() { return m_presentQueue; }
    
    VkRenderPass& swapChainRenderPass() { return m_swapChainRenderPass; }

    uint32_t& currentFrame() { return m_currentFrame; }

    VkPhysicalDeviceProperties& physicalDeviceProperties() { return m_physicalDeviceProperties; }

    // NOTE: maybe change this
    std::vector<VkImageView>& swapChainImageViews() { return m_swapChainImageViews; }
    std::vector<VkFramebuffer>& swapChainFramebuffers() { return m_swapChainFramebuffers; }

    VkDescriptorPool descriptorPool() { return m_descriptorPool; }
    
    const uint32_t MAX_FRAMES_IN_FLIGHT;

private:
    bool checkInstanceValidationLayerSupport();
    void pushInstanceValidationLayers();
    void pushInstanceDebugUtilsMessengerExtension();
    void pushRequiredInstanceExtensions();
    VkDebugUtilsMessengerCreateInfoEXT getDebugUtilsMessengerCreateInfo();
    void setupDebugMessenger();

    void createSurface();

    void pushRequiredDeviceExtensions();
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalDevice);
    bool isDeviceExtensionSupported(VkPhysicalDevice physicalDevice);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice physicalDevice);
    bool isPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice);
    void pickPhysicalDevice();

    void pushDeviceValidationLayers();
    void createLogicalDevice();

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    void createSwapChain();

    void createRenderPass();

    void createFramebuffers();

    void createCommandPool();

    void allocateCommandBuffers();

    void createSyncObjects();

    void createDescriptorPool();

    void recreateSwapChainAndItsResources();

private:
    std::shared_ptr<core::Window> window;

    const bool m_validation;
    VkPhysicalDeviceProperties m_physicalDeviceProperties{};

    std::vector<const char *> m_instanceLayers{};
    std::vector<const char *> m_instanceExtensions{};

    std::vector<const char *> m_deviceLayers{};
    std::vector<const char *> m_deviceExtensions{};

    VkInstance m_instance{};
    VkDebugUtilsMessengerEXT m_debugUtilsMessenger{};
    VkSurfaceKHR m_surface{};
    VkPhysicalDevice m_physicalDevice{};
    VkDevice m_device{};
    VkQueue m_graphicsQueue{};
    VkQueue m_presentQueue{};
    VkSwapchainKHR m_swapChain{};
    VkFormat m_swapChainFormat{};
    VkExtent2D m_swapChainExtent;
    std::vector<VkImage> m_swapChainImages{};
    // NOTE: maybe change this
    std::vector<VkImageView> m_swapChainImageViews{};
    VkRenderPass m_swapChainRenderPass{};
    std::vector<VkFramebuffer> m_swapChainFramebuffers{};
    VkCommandPool m_commandPool{};
    std::vector<VkCommandBuffer> m_commandBuffers{};

    uint32_t m_currentFrame{};
    uint32_t m_imageIndex{};

    std::vector<VkSemaphore> m_imageAvailableSemaphores{};
    std::vector<VkSemaphore> m_renderFinishedSemaphores{};
    std::vector<VkFence> m_inFlightFences{};

    VkDescriptorPool m_descriptorPool{};
};

} // namespace vulkan

} // namespace gfx

#endif