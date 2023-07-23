#ifndef EDITOR_CAMERA_HPP
#define EDITOR_CAMERA_HPP

#include "core/components.hpp"
#include "core/window.hpp"

#include "glm/gtx/string_cast.hpp"

#include <memory>
#include <iostream>

class EditorCamera : public core::CameraComponent {
public:
    EditorCamera(std::shared_ptr<core::Window> window)
      : m_window(window) {

    }
    ~EditorCamera() {

    }

    void onUpdate(float ts) {
        // ImVec2 ws = ImGui::GetWindowSize();
        auto [width, height] = m_window->getSize();
        update(float(width) / float(height));

        double curX, curY;
        glfwGetCursorPos(m_window->getWindow(), &curX, &curY);

        float velocity = m_speed * ts;

        if (glfwGetKey(m_window->getWindow(), GLFW_KEY_W)) 
            m_pos += m_front * velocity;
        if (glfwGetKey(m_window->getWindow(), GLFW_KEY_S)) 
            m_pos -= m_front * velocity;
        if (glfwGetKey(m_window->getWindow(), GLFW_KEY_D)) 
            m_pos += m_right * velocity;
        if (glfwGetKey(m_window->getWindow(), GLFW_KEY_A)) 
            m_pos -= m_right * velocity;
        if (glfwGetKey(m_window->getWindow(), GLFW_KEY_SPACE)) 
            m_pos += m_up * velocity;
        if (glfwGetKey(m_window->getWindow(), GLFW_KEY_LEFT_SHIFT)) 
            m_pos -= m_up * velocity;
        
        glm::vec2 mouse{curX, curY};
        glm::vec2 difference = mouse - m_initialMouse;
        m_initialMouse = mouse;

        if (glfwGetMouseButton(m_window->getWindow(), GLFW_MOUSE_BUTTON_1)) {
            
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
    std::shared_ptr<core::Window> m_window;
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