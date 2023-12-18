#ifndef CORE_COMPONENTS_HPP
#define CORE_COMPONENTS_HPP

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace core {

class CameraComponent {
public:
    CameraComponent() {}
    ~CameraComponent() {}

    void update(float aspectRation) {
        m_projection = glm::perspective(glm::radians(m_fov), aspectRation, m_near, m_far);
    }

    const glm::mat4& getProjection() const {
        return m_projection;
    }

    const glm::mat4& getView() const { 
        return m_view; 
    }

    const glm::vec3& getPos() const { return m_pos; }

    float fov() const { return m_fov; }
    float near() const { return m_near; }
    float far() const { return m_far; }

    glm::vec3 m_pos{0, 0, 0};
    glm::mat4 m_projection{1.0f};
    glm::mat4 m_view{1};
    float m_fov{45.0f};
    float m_far{1000.f}, m_near{0.1f};
};

struct Transform {
    glm::vec3 translation{ 0.f, 0.f, 0.f };
    glm::vec3 rotation{ 0.f, 0.f, 0.f };
    glm::vec3 scale{ 1.f, 1.f, 1.f };
    
    Transform() = default;

    glm::mat4 mat4() const {
        return glm::translate(glm::mat4(1.f), translation)
            * glm::toMat4(glm::quat(rotation))
            * glm::scale(glm::mat4(1.f), scale);
    }
};

} // namespace core

#endif