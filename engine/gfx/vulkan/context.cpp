#include "context.hpp"

#include "core/core.hpp"
#include "core/log.hpp"

#include <set>
#include <string>

namespace gfx {

namespace vulkan {

static bool isVolkInitialized = false;

static void initializeVolk() {
    VIZON_PROFILE_FUNCTION();
    if (volkInitialize() != VK_SUCCESS) {
        ERROR("Failed to initialize Volk");
        std::terminate();
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    VIZON_PROFILE_FUNCTION();
    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: 
            TRACE("{}", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: 
            INFO("{}", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: 
            WARN("{}", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: 
            ERROR("{}", pCallbackData->pMessage);
            break;
    }
    return VK_FALSE;
}

Context::Context(std::shared_ptr<core::Window> window, uint32_t MAX_FRAMES_IN_FLIGHT, bool validation, bool enableRaytracing)
  : MAX_FRAMES_IN_FLIGHT(MAX_FRAMES_IN_FLIGHT),
    window(window),
    m_validation(validation),
    m_raytracing(enableRaytracing) {
    VIZON_PROFILE_FUNCTION();
    if (!isVolkInitialized) {
        isVolkInitialized = true;
        initializeVolk();
    }

    if (m_validation) {
        if (!checkInstanceValidationLayerSupport()) {
            ERROR("Validation layers requested but not supported");
            std::terminate();
        }
        pushInstanceValidationLayers();
        pushInstanceDebugUtilsMessengerExtension();
    }
    pushRequiredInstanceExtensions();

    VkApplicationInfo applicationInfo{};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = window->getTitle().c_str();
    applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.pEngineName = "Horizon";
    applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instanceCreateInfo{};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &applicationInfo;
    
    instanceCreateInfo.enabledExtensionCount = m_instanceExtensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = m_instanceExtensions.data();

    instanceCreateInfo.enabledLayerCount = m_instanceLayers.size();
    instanceCreateInfo.ppEnabledLayerNames = m_instanceLayers.data();

    auto debugUtilsMessengerCreateInfo = getDebugUtilsMessengerCreateInfo();
    if (m_validation) {
        instanceCreateInfo.pNext = &debugUtilsMessengerCreateInfo;
    }
    
    auto result = vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS) {
        ERROR("Failed to create instance!");
        std::terminate();
    }

    volkLoadInstance(m_instance);

    TRACE("Created vulkan instance");

    if (m_validation) {
        setupDebugMessenger();
    }
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createRenderPass();
    createFramebuffers();
    createCommandPool();
    allocateCommandBuffers();
    createSyncObjects();
    createDescriptorPool();

    INFO("Created Context");
}

Context::~Context() {
    vkDeviceWaitIdle(m_device);
    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
    }
    vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    for (auto framebuffer : m_swapChainFramebuffers) {
        vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }
    vkDestroyRenderPass(m_device, m_swapChainRenderPass, nullptr);
    for (auto imageView : m_swapChainImageViews) {
        vkDestroyImageView(m_device, imageView, nullptr);
    }
    vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    vkDestroyDevice(m_device, nullptr);
    if (m_validation) {
        vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugUtilsMessenger, nullptr);
    }
    vkDestroyInstance(m_instance, nullptr);
    TRACE("Destroyed Context");
}

std::optional<std::pair<VkCommandBuffer, uint32_t>> Context::startFrame() {
    VIZON_PROFILE_FUNCTION();
    vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
    
    auto result = vkAcquireNextImageKHR(m_device, m_swapChain, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &m_imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChainAndItsResources();
        return std::nullopt;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        ERROR("Failed to acquire swap chain image");
        std::terminate();
    }
    vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);
    vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);
    VkCommandBufferBeginInfo commandBufferBeginInfo{};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(m_commandBuffers[m_currentFrame], &commandBufferBeginInfo) != VK_SUCCESS) {
        ERROR("Failed to being command buffer");
        std::terminate();
    }
    return std::pair<VkCommandBuffer, uint32_t>{ m_commandBuffers[m_currentFrame], m_currentFrame };
}

bool Context::endFrame(VkCommandBuffer commandBuffer) {
    VIZON_PROFILE_FUNCTION();
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        ERROR("Failed to record command buffer");
        std::terminate();
    }

    VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphores[m_currentFrame] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	VkSemaphore signalSemaphores[] = { m_renderFinishedSemaphores[m_currentFrame] };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;
	  
	if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
		ERROR("Failed to submit draw command buffer");
        std::terminate();
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	VkSwapchainKHR swapChains[] = { m_swapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &m_imageIndex;
	presentInfo.pResults = nullptr;

	auto result = vkQueuePresentKHR(m_presentQueue, &presentInfo);
    bool recreated = false;

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapChainAndItsResources();
        recreated = true;
    } else if (result != VK_SUCCESS) {
        ERROR("Failed to present swap chain");
        std::terminate();
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    return recreated;
}

void Context::beginSwapChainRenderPass(VkCommandBuffer commandBuffer, const VkClearValue& clearValue) {
    VIZON_PROFILE_FUNCTION();
    VkRenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.framebuffer = m_swapChainFramebuffers[m_imageIndex];
    renderPassBeginInfo.renderPass = m_swapChainRenderPass;
    renderPassBeginInfo.renderArea.offset = {0, 0};
    renderPassBeginInfo.renderArea.extent = m_swapChainExtent;
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = &clearValue;
    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);  // hardcoded subpass contents inline
}

void Context::endSwapChainRenderPass(VkCommandBuffer commandBuffer) {
    VIZON_PROFILE_FUNCTION();
    vkCmdEndRenderPass(commandBuffer);
}

VkCommandBuffer Context::startSingleUseCommandBuffer() {
    VIZON_PROFILE_FUNCTION();
    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = m_commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;

    vkAllocateCommandBuffers(m_device, &commandBufferAllocateInfo, &commandBuffer);

    VkCommandBufferBeginInfo commandBufferBeginInfo{};
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);

    return commandBuffer;
}

void Context::endSingleUseCommandBuffer(VkCommandBuffer commandBuffer) {
    VIZON_PROFILE_FUNCTION();
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_graphicsQueue);

    vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
}

void Context::singleUseCommandBuffer(std::function<void(VkCommandBuffer)> fn) {
    VIZON_PROFILE_FUNCTION();
    auto commandBuffer = startSingleUseCommandBuffer();
    fn(commandBuffer);
    endSingleUseCommandBuffer(commandBuffer);
}

uint32_t Context::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VIZON_PROFILE_FUNCTION();
    VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &physicalDeviceMemoryProperties);

    for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    ERROR("Failed to find suitable memory type");
    std::terminate();
}

bool Context::checkInstanceValidationLayerSupport() {
    VIZON_PROFILE_FUNCTION();
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> layers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

    for (auto layer : layers) {
        std::string_view layerName = layer.layerName;
        if (layerName == "VK_LAYER_KHRONOS_validation") 
            return true;
    }
    return false;
}   

void Context::pushInstanceValidationLayers() {
    VIZON_PROFILE_FUNCTION();
    m_instanceLayers.push_back("VK_LAYER_KHRONOS_validation");   
}

void Context::pushInstanceDebugUtilsMessengerExtension() {
    VIZON_PROFILE_FUNCTION();
    m_instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
}

void Context::pushRequiredInstanceExtensions() {
    VIZON_PROFILE_FUNCTION();
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    for (uint32_t i = 0; i < glfwExtensionCount; i++) {
        m_instanceExtensions.push_back(glfwExtensions[i]);
    }
    // m_instanceExtensions.push_back("VK_KHR_get_physical_device_properties2");
}

VkDebugUtilsMessengerCreateInfoEXT Context::getDebugUtilsMessengerCreateInfo() {
    VIZON_PROFILE_FUNCTION();
    VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo{};
    debugUtilsMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugUtilsMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT    |
                                                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
                                                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
                                                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugUtilsMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT    | 
                                                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
                                                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugUtilsMessengerCreateInfo.pfnUserCallback = debugCallback;
    debugUtilsMessengerCreateInfo.pUserData = nullptr;
    return debugUtilsMessengerCreateInfo;
}

void Context::setupDebugMessenger() {
    VIZON_PROFILE_FUNCTION();
    auto debugMessengerCreateInfo = getDebugUtilsMessengerCreateInfo();

    auto result = vkCreateDebugUtilsMessengerEXT(m_instance, &debugMessengerCreateInfo, nullptr, &m_debugUtilsMessenger);
    if (result != VK_SUCCESS) {
        ERROR("Failed to create debug utils messenger!");
        std::terminate();
    }
    TRACE("Created debug utils messenger");
}

void Context::createSurface() {
    VIZON_PROFILE_FUNCTION();
    auto result = glfwCreateWindowSurface(m_instance, window->getWindow(), nullptr, &m_surface);
    if (result != VK_SUCCESS) {
        ERROR("Failed to create surface");
        std::terminate();
    }
    TRACE("Created surface");
}

void Context::pushRequiredDeviceExtensions() {
    VIZON_PROFILE_FUNCTION();
    m_deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if (m_raytracing) {
        m_deviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        m_deviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
        m_deviceExtensions.push_back("VK_EXT_descriptor_indexing");
        m_deviceExtensions.push_back(VK_KHR_MAINTENANCE_3_EXTENSION_NAME);
        m_deviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
        m_deviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    }
}

Context::QueueFamilyIndices Context::findQueueFamilies(VkPhysicalDevice physicalDevice) {
    VIZON_PROFILE_FUNCTION();
    QueueFamilyIndices queueFamilyIndices{};

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamiliesProperties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamiliesProperties.data());

    int i = 0;
    for (auto queueFamilyProperties : queueFamiliesProperties) {
        if (queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            queueFamilyIndices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, m_surface, &presentSupport);
        if (presentSupport) {
            queueFamilyIndices.presentFamily = i;
        }

        if (queueFamilyIndices.isComplete()) {
            break;
        }
        i++;
    }

    return queueFamilyIndices;
}

bool Context::isDeviceExtensionSupported(VkPhysicalDevice physicalDevice) {
    VIZON_PROFILE_FUNCTION();
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(m_deviceExtensions.begin(), m_deviceExtensions.end());

    for (auto extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }
    return requiredExtensions.empty();
}

Context::SwapChainSupportDetails Context::querySwapChainSupport(VkPhysicalDevice physicalDevice) {
    VIZON_PROFILE_FUNCTION();
    SwapChainSupportDetails swapChainSupportDetails{};

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_surface, &swapChainSupportDetails.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_surface, &formatCount, nullptr);
    if (formatCount != 0) {
        swapChainSupportDetails.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_surface, &formatCount, swapChainSupportDetails.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        swapChainSupportDetails.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_surface, &presentModeCount, swapChainSupportDetails.presentModes.data());
    }

    return swapChainSupportDetails;
}

bool Context::isPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice) {
    VIZON_PROFILE_FUNCTION();
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
    vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

    auto queueFamilyIndices = findQueueFamilies(physicalDevice);

    bool extensionsSupported = isDeviceExtensionSupported(physicalDevice);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        auto swapChainSupportDetails = querySwapChainSupport(physicalDevice);
        swapChainAdequate = !swapChainSupportDetails.formats.empty() && !swapChainSupportDetails.presentModes.empty();
    }

    INFO("timestampperiod {}", deviceProperties.limits.timestampPeriod);    

    return queueFamilyIndices.isComplete() && swapChainAdequate && deviceProperties.limits.timestampComputeAndGraphics == VK_TRUE && deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}

void Context::pickPhysicalDevice() {
    VIZON_PROFILE_FUNCTION();
    pushRequiredDeviceExtensions();

    uint32_t deviceCount;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        ERROR("Failed to find a GPU with vulkan support, try updating the drivers");
        std::terminate();
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    for (auto device : devices) {
        if (isPhysicalDeviceSuitable(device)) {
            m_physicalDevice = device;
            break;
        }
    }
    if (m_physicalDevice == VK_NULL_HANDLE) {
        ERROR("Failed to find a suitable GPU");
        std::terminate();
    }

    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &deviceProperties);
    vkGetPhysicalDeviceFeatures(m_physicalDevice, &deviceFeatures);

    m_physicalDeviceProperties = deviceProperties;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR physicalDeviceRayTracingPipelinePropertiesKHR{};
    physicalDeviceRayTracingPipelinePropertiesKHR.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 physicalDeviceProperties2{}; 
    physicalDeviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    physicalDeviceProperties2.properties = m_physicalDeviceProperties;
    physicalDeviceProperties2.pNext = &physicalDeviceRayTracingPipelinePropertiesKHR;

    vkGetPhysicalDeviceProperties2(m_physicalDevice, &physicalDeviceProperties2);

    VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &physicalDeviceMemoryProperties);

    INFO("Picked physical device: {}", deviceProperties.deviceName);
}

void Context::pushDeviceValidationLayers() {
    VIZON_PROFILE_FUNCTION();
    m_deviceLayers.push_back("VK_LAYER_KHRONOS_validation"); 
}

void Context::createLogicalDevice() {
    VIZON_PROFILE_FUNCTION();
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(m_physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos{};
    std::set<uint32_t> uniqueQueueFamilies = { queueFamilyIndices.graphicsFamily.value(), queueFamilyIndices.presentFamily.value() };
    
    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo deviceQueueCreateInfo{};
        deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        deviceQueueCreateInfo.queueFamilyIndex = queueFamily;
        deviceQueueCreateInfo.queueCount = 1;
        deviceQueueCreateInfo.pQueuePriorities = &queuePriority;
        deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
    }

    VkPhysicalDeviceBufferDeviceAddressFeatures
        physicalDeviceBufferDeviceAddressFeatures = {
            .sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
            .pNext = NULL,
            .bufferDeviceAddress = VK_TRUE,
            .bufferDeviceAddressCaptureReplay = VK_FALSE,
            .bufferDeviceAddressMultiDevice = VK_FALSE};

    VkPhysicalDeviceAccelerationStructureFeaturesKHR
        physicalDeviceAccelerationStructureFeatures = {
            .sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
            .pNext = &physicalDeviceBufferDeviceAddressFeatures,
            .accelerationStructure = VK_TRUE,
            .accelerationStructureCaptureReplay = VK_FALSE,
            .accelerationStructureIndirectBuild = VK_FALSE,
            .accelerationStructureHostCommands = VK_FALSE,
            .descriptorBindingAccelerationStructureUpdateAfterBind = VK_FALSE};

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR
        physicalDeviceRayTracingPipelineFeatures = {
            .sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
            .pNext = &physicalDeviceAccelerationStructureFeatures,
            .rayTracingPipeline = VK_TRUE,
            .rayTracingPipelineShaderGroupHandleCaptureReplay = VK_FALSE,
            .rayTracingPipelineShaderGroupHandleCaptureReplayMixed = VK_FALSE,
            .rayTracingPipelineTraceRaysIndirect = VK_FALSE,
            .rayTraversalPrimitiveCulling = VK_FALSE};
    VkPhysicalDeviceFeatures deviceFeatures = {.geometryShader = VK_TRUE};

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfos.data();
    deviceCreateInfo.queueCreateInfoCount = deviceQueueCreateInfos.size();
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
    deviceCreateInfo.enabledExtensionCount = 0;
    if (m_raytracing)
        deviceCreateInfo.pNext = &physicalDeviceRayTracingPipelineFeatures;

    if (m_validation) 
        pushDeviceValidationLayers();

    deviceCreateInfo.enabledLayerCount = m_deviceLayers.size();
    deviceCreateInfo.ppEnabledLayerNames = m_deviceLayers.data();
    deviceCreateInfo.enabledExtensionCount = m_deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = m_deviceExtensions.data();

    auto result = vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device);
    if (result != VK_SUCCESS) {
        ERROR("Failed to create device");
        std::terminate();
    }
    TRACE("Created device");

    // volkLoadDevice(m_device);

    vkGetDeviceQueue(m_device, queueFamilyIndices.graphicsFamily.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, queueFamilyIndices.presentFamily.value(), 0, &m_presentQueue);
}

VkSurfaceFormatKHR Context::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    VIZON_PROFILE_FUNCTION();
    for (auto availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR Context::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    VIZON_PROFILE_FUNCTION();
    for (auto availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Context::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    VIZON_PROFILE_FUNCTION();
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(window->getWindow(), &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void Context::createSwapChain() {
    VIZON_PROFILE_FUNCTION();
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_physicalDevice);

    auto surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    auto presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    auto extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapChainCreateInfo{};
    swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCreateInfo.surface = m_surface;
    swapChainCreateInfo.minImageCount = imageCount;
    swapChainCreateInfo.imageFormat = surfaceFormat.format;
    swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapChainCreateInfo.imageExtent = extent;
    swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    auto queueFamilyIndices = findQueueFamilies(m_physicalDevice);
    uint32_t queueIndices[] = { queueFamilyIndices.graphicsFamily.value(), queueFamilyIndices.presentFamily.value() };

    if (queueFamilyIndices.graphicsFamily != queueFamilyIndices.presentFamily) {
        swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapChainCreateInfo.queueFamilyIndexCount = 2;
        swapChainCreateInfo.pQueueFamilyIndices = queueIndices;
    } else {
        swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    swapChainCreateInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainCreateInfo.presentMode = presentMode;
    swapChainCreateInfo.clipped = VK_TRUE;

    swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    auto result = vkCreateSwapchainKHR(m_device, &swapChainCreateInfo, nullptr, &m_swapChain);
    if (result != VK_SUCCESS) {
        ERROR("Failed to create swap chain");
        std::terminate();
    }

    m_swapChainExtent = extent;
    m_swapChainFormat = surfaceFormat.format;

    vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
    m_swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.data());

    m_swapChainImageViews.resize(imageCount);
    for (size_t i = 0; i < m_swapChainImages.size(); i++) {
        VkImageViewCreateInfo imageViewCreateInfo{};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.image = m_swapChainImages[i];
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = m_swapChainFormat;
        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;
        auto result = vkCreateImageView(m_device, &imageViewCreateInfo, nullptr, &m_swapChainImageViews[i]);
        if (result != VK_SUCCESS) {
            ERROR("Failed to create image view");
            std::terminate();
        }
    }

    TRACE("Created swap chain");
}

void Context::createRenderPass() {
    VIZON_PROFILE_FUNCTION();
    VkAttachmentDescription colorAttachment{};
	colorAttachment.format = m_swapChainFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_swapChainRenderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create render pass!");
	}
}

void Context::createFramebuffers() {
    VIZON_PROFILE_FUNCTION();
    m_swapChainFramebuffers.resize(m_swapChainImages.size());
    for (size_t i = 0; i < m_swapChainImages.size(); i++) {
        VkImageView attachments[] = {
            m_swapChainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_swapChainRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = m_swapChainExtent.width;
        framebufferInfo.height = m_swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }

    }
}

void Context::createCommandPool() {
    VIZON_PROFILE_FUNCTION();
    auto queueFamilyIndices = findQueueFamilies(m_physicalDevice);
    VkCommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    if (vkCreateCommandPool(m_device, &commandPoolCreateInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        ERROR("Failed to create command pool");
        std::terminate();
    }
}

void Context::allocateCommandBuffers() {
    VIZON_PROFILE_FUNCTION();
    VkCommandBufferAllocateInfo commandbufferAllocateInfo{};
    commandbufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandbufferAllocateInfo.commandPool = m_commandPool;
    commandbufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandbufferAllocateInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    
    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    if (vkAllocateCommandBuffers(m_device, &commandbufferAllocateInfo, m_commandBuffers.data()) != VK_SUCCESS) {
        ERROR("Failed to allocate commandbuffer");
        std::terminate();
    }
}

void Context::createSyncObjects() {
    VIZON_PROFILE_FUNCTION();
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS) {
            ERROR("Failed to create semaphore");
            std::terminate();
        }

        if (vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS) {
            ERROR("Failed to create semaphore");
            std::terminate();
        }

        if (vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            ERROR("Failed to create fence");
            std::terminate();
        }
    }    
}

void Context::createDescriptorPool() {
    VIZON_PROFILE_FUNCTION();
    std::vector<VkDescriptorPoolSize> poolSizes{};

    {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = 1000;
        poolSizes.push_back(poolSize);
    }

    {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_SAMPLER;
        poolSize.descriptorCount = 1000;
        poolSizes.push_back(poolSize);
    }

    {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSize.descriptorCount = 1000;
        poolSizes.push_back(poolSize);
    }

    {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSize.descriptorCount = 1000;
        poolSizes.push_back(poolSize);
    }

    {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        poolSize.descriptorCount = 1000;
        poolSizes.push_back(poolSize);
    }

    {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 1000;
        poolSizes.push_back(poolSize);
    }

    
    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.poolSizeCount = poolSizes.size();
    descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();
    descriptorPoolCreateInfo.maxSets = 1000 * 6;

    if (vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        ERROR("Failed to create descriptor pool");
        std::terminate();
    }
}

void Context::recreateSwapChainAndItsResources() {
    VIZON_PROFILE_FUNCTION();
    int width, height;
    glfwGetFramebufferSize(window->getWindow(), &width, &height);
    if (width == 0 || height == 0) {
        glfwGetFramebufferSize(window->getWindow(), &width, &height);
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(m_device);

    for (auto framebuffer : m_swapChainFramebuffers) {
        vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }
    for (auto imageView : m_swapChainImageViews) {
        vkDestroyImageView(m_device, imageView, nullptr);
    }
    vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);

    createSwapChain();
    createFramebuffers();

    for (auto resizeCallBack : m_resizeCallBacks) {
        resizeCallBack();
    }
}

} // namespace vulkan

} // namespace gfx
