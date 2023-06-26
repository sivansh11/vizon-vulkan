#ifndef WINDOW_HPP
#define WINDOW_HPP

#include "gfx/vulkan/image.hpp"
#include "gfx/vulkan/descriptor.hpp"

#include "core/material.hpp"

#include <ScreenCapture.h>

#include <string>

class Window {
public:
    Window(std::shared_ptr<gfx::vulkan::Context> context, const std::string& name);
    ~Window();

    void update(VkCommandBuffer commandBuffer);

    std::shared_ptr<gfx::vulkan::Image> image() { return m_image; }

private:
    std::shared_ptr<gfx::vulkan::Context> m_context;
    std::vector<SL::Screen_Capture::Window> selectedWindow;
    std::atomic<bool> m_imgBufferChanged = false;
    std::shared_ptr<SL::Screen_Capture::IScreenCaptureManager> m_framegrabber;
    std::shared_ptr<gfx::vulkan::Image> m_image;
    void *map;
};

#endif