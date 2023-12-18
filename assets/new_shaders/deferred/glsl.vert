#version 450

layout (location = 0) in vec3 vertex_position;
// layout (location = 1) in vec3 vertex_normal;
layout (location = 2) in vec2 vertex_uv;
// layout (location = 3) in vec3 vertex_tangent;
// layout (location = 4) in vec3 vertex_bitangent;

layout (location = 0) out vec2 fragment_uv;
// layout (location = 1) out vec3 fragment_T;
// layout (location = 2) out vec3 fragment_B;
// layout (location = 3) out vec3 fragment_N;

layout (set = 0, binding = 0) uniform camera_uniform_t {
    // current
    mat4 projection_view;
    mat4 view;
    mat4 inverse_view;
    mat4 projection;
    mat4 inverse_projection;
    // maybe add priv ?
};

void main() {
    fragment_uv = vertex_uv;
    vec4 world_position = vec4(vertex_position, 1);
    gl_Position = projection_view * world_position;
}