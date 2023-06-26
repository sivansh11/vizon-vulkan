#ifndef UBOS_HPP
#define UBOS_HPP

#include <glm/glm.hpp>

struct GlobalUniformBufferStruct {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 invView;
    glm::mat4 invProjection;
};

struct PerObjectUniformBufferStruct {
    glm::mat4 model;
    glm::mat4 invModel;
};

#endif