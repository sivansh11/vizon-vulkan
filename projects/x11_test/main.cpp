#include "core/core.hpp"
#include "core/imgui_utils.hpp"
#include "core/window.hpp"
#include "core/core.hpp"
#include "core/event.hpp"
#include "core/log.hpp"
#include "core/mesh.hpp"
#include "core/material.hpp"
#include "core/model.hpp"

#include "gfx/vulkan/context.hpp"
#include "gfx/vulkan/pipeline.hpp"
#include "gfx/vulkan/descriptor.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "gfx/vulkan/image.hpp"
#include "gfx/vulkan/renderpass.hpp"
#include "gfx/vulkan/framebuffer.hpp"
#include "gfx/vulkan/timer.hpp"
#include "gfx/vulkan/acceleration_structure.hpp"

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/render.h>

using namespace core;
using namespace gfx::vulkan;

int main() {
    if (!glfwInit()) {
        ERROR("Failed to initialize GLFW");
        std::terminate();
    }

    auto window = make_ref<window_t>("x11 test", 600, 400);

    Display *x11_display = glfwGetX11Display();
    if (!x11_display) {
        ERROR("Failed to get x11 display!");
        std::terminate();
    }

    bool has_name_pixmap = false;
    int event_base, error_base;
    if (XCompositeQueryExtension(x11_display, &event_base, &error_base)) {
        int major = 0, minor = 2;
        XCompositeQueryVersion(x11_display, &major, &minor);

        if (major > 0 || minor > 2) {
            has_name_pixmap = true;
        }
    }

    for (int i = 0; i < ScreenCount(x11_display); i++) {
        XCompositeRedirectSubwindows(x11_display, RootWindow(x11_display, i), CompositeRedirectAutomatic);
        // XCompositeRedirectWindow(); might be usefull, come back
    }

    Window window_id = glfwGetX11Window(window->window());

    XWindowAttributes window_attrs{};
    XGetWindowAttributes(x11_display, window_id, &window_attrs);  // synchronous call, avoid if multithreading

    XRenderPictFormat *format = XRenderFindVisualFormat(x11_display, window_attrs.visual);

    XRenderPictureAttributes picture_attrs{};
    picture_attrs.subwindow_mode = IncludeInferiors;

    Picture picture = XRenderCreatePicture(x11_display, window_id, format, CPSubwindowMode, &picture_attrs);

    XserverRegion region = XFixesCreateRegionFromWindow(x11_display, window_id, WindowRegionBounding);

    XFixesTranslateRegion(x11_display, region, -window_attrs.x, -window_attrs.y);
    XFixesSetPictureClipRegion(x11_display, picture, 0, 0, region);
    XFixesDestroyRegion(x11_display, region);

    XShapeSelectInput(x11_display, window_id, ShapeNotifyMask);

    

    XRenderComposite(x11_display, (format->type == PictTypeDirect && format->direct.alpha) ? PictOpOver : PictOpSrc, picture, None, des)

    while (!window->should_close()) {
        window->poll_events();
        if (glfwGetKey(window->window(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window->window(), true);
        }
    }

    return EXIT_SUCCESS;
}