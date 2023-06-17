#include "window.hpp"

#include "core/log.hpp"

namespace core {

Window::Window(const std::string& title, uint32_t width, uint32_t height)
  : m_title(title) {
    if (!glfwInit()) {
        ERROR("Failed to initialize GLFW");
        std::terminate();
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_windowPtr = glfwCreateWindow(width, height, m_title.c_str(), NULL, NULL);

    TRACE("Window {} created", m_title);
}    

Window::~Window() {
    glfwTerminate();
    TRACE("Window {} destroyed", m_title);
}

std::pair<int, int> Window::getSize() {
    int width, height;
    glfwGetWindowSize(m_windowPtr, &width, &height);
    return { width, height };
}

} // namespace core
