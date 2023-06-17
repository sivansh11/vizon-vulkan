#version 450

layout (location = 0) in vec3 pos;

layout (set = 0, binding = 0) uniform ShadowSet0UBO {
    mat4 lightSpace;
};
layout (set = 1, binding = 0) uniform Set1UBO {
    mat4 model;
    mat3 invModelT;
};

void main() {
    gl_Position = lightSpace * model * vec4(pos, 1.0);
}