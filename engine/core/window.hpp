#ifndef CORE_WINDOW_HPP
#define CORE_WINDOW_HPP

#define GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_NONE
#define VK_NO_PROTOTYPES
#include <GLFW/glfw3.h>

#include <string>

namespace core {

class Window {
public:
    Window(const std::string& title, uint32_t width, uint32_t height);
    ~Window();

    std::pair<int, int> getSize();

    const std::string& getTitle() const { return m_title; }
    GLFWwindow *getWindow() { return m_windowPtr; }
    bool shouldClose() { return glfwWindowShouldClose(m_windowPtr); }
    void pollEvents() { return glfwPollEvents(); }

private:
    std::string m_title{};
    GLFWwindow *m_windowPtr{};
};

} // namespace core

#endif