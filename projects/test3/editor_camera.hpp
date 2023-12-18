#ifndef EDITOR_CAMERA_HPP
#define EDITOR_CAMERA_HPP

#include "core/components.hpp"
#include "core/window.hpp"

#include "glm/gtx/string_cast.hpp"

#include <memory>
#include <iostream>

class editor_camera_t : public core::CameraComponent {
public:
    editor_camera_t(std::shared_ptr<core::window_t> window)
      : m_window(window) {

    }
    ~editor_camera_t() {

    }

    void onUpdate(float ts) {
        // ImVec2 ws = ImGui::GetWindowSize();
        auto [width, height] = m_window->get_dimensions();
        update(float(width) / float(height));

        double curX, curY;
        glfwGetCursorPos(m_window->window(), &curX, &curY);

        float velocity = m_speed * ts;

        if (glfwGetKey(m_window->window(), GLFW_KEY_W)) 
            m_pos += m_front * velocity;
        if (glfwGetKey(m_window->window(), GLFW_KEY_S)) 
            m_pos -= m_front * velocity;
        if (glfwGetKey(m_window->window(), GLFW_KEY_D)) 
            m_pos += m_right * velocity;
        if (glfwGetKey(m_window->window(), GLFW_KEY_A)) 
            m_pos -= m_right * velocity;
        if (glfwGetKey(m_window->window(), GLFW_KEY_SPACE)) 
            m_pos += m_up * velocity;
        if (glfwGetKey(m_window->window(), GLFW_KEY_LEFT_SHIFT)) 
            m_pos -= m_up * velocity;
        
        glm::vec2 mouse{curX, curY};
        glm::vec2 difference = mouse - m_initialMouse;
        m_initialMouse = mouse;

        if (glfwGetMouseButton(m_window->window(), GLFW_MOUSE_BUTTON_1)) {
            
            difference.x = difference.x / float(width);
            difference.y = -(difference.y / float(height));

            m_yaw += difference.x * m_sensitivity;
            m_pitch += difference.y * m_sensitivity;

            if (m_pitch > 89.0f) 
                m_pitch = 89.0f;
            if (m_pitch < -89.0f) 
                m_pitch = -89.0f;
        }

        glm::vec3 front;
        front.x = glm::cos(glm::radians(m_yaw)) * glm::cos(glm::radians(m_pitch));
        front.y = glm::sin(glm::radians(m_pitch));
        front.z = glm::sin(glm::radians(m_yaw)) * glm::cos(glm::radians(m_pitch));
        m_front = front * speedMultiplyer;
        m_right = glm::normalize(glm::cross(m_front, glm::vec3{0, 1, 0}));
        m_up    = glm::normalize(glm::cross(m_right, m_front));

        m_view = glm::lookAt(m_pos, m_pos + m_front, glm::vec3{0, 1, 0});
    } 


    float getYaw() { return m_yaw; }
    float getPitch() { return m_pitch; }

    glm::vec3 getDir() {
        return m_pos + m_front;
    }

    float speedMultiplyer = 1;

private:
    std::shared_ptr<core::window_t> m_window;
    glm::vec3 m_front{0};
    glm::vec3 m_up{0, 1, 0};
    glm::vec3 m_right{0};

    glm::vec2 m_initialMouse{};

    float m_yaw = 0, m_pitch = 0;

    float m_speed = .005f;
    float m_sensitivity = 100;

    bool is_first{true};
};

#endif