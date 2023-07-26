#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 bi_tangent;

layout (location = 0) out vec2 out_uv;

layout (set = 0, binding = 0) uniform set_0_uniform_buffer_t {
    mat4 view;
    mat4 inv_view;
    mat4 projection;
    mat4 inv_projection;
};

layout (set = 1, binding = 0) uniform set_1_uniform_buffer_t {
    mat4 model;
    mat3 inv_model;
};

void main() {
    vec4 worldPosition = model * vec4(position, 1);
    out_uv = uv;

    gl_Position = projection * view * worldPosition;
}