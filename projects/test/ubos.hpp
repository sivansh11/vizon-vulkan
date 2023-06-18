#ifndef UBOS_HPP
#define UBOS_HPP

#include <glm/glm.hpp>

struct Set0UBO {
    glm::mat4 projection;
    glm::mat4 view;
};

struct ShadowSet0UBO {
    glm::mat4 lightSpace;
};

struct Set1UBO {
    glm::mat4 model;
    glm::mat4 invModelT;
};

struct DirectionalLight {
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 color;
    alignas(16) glm::vec3 ambience;
    alignas(16) glm::vec3 term;
};

struct CompositeSet0UBO {
    glm::mat4 lightSpace;
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 invView;
    glm::mat4 invProjection;
    DirectionalLight directionalLight;
};

struct SsaoSet0UBO {
    glm::mat4 projection;
    glm::mat4 invProjection;
    glm::mat4 view;
    glm::mat4 invView;
    alignas(4) float radius;
    alignas(4) float bias;
};

#endif