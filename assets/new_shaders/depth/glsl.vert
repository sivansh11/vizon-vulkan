#version 450

layout (location = 0) in vec3 vertex_position;

layout (set = 0, binding = 0) uniform camera_uniform_t {
    // current
    mat4 projection_view;
    mat4 view;
    mat4 inverse_view;
    mat4 projection;
    mat4 inverse_projection;
    // maybe add priv ?
};

// not in this renderer
// layout (set = 1, binding = 0) uniform model_uniform_t {
//     mat4 model;
//     mat4 inverse_model;
// };

void main() {
    // vec4 world_position = model * vec4(vertex_position, 1);
    vec4 world_position = vec4(vertex_position, 1);
    gl_Position = projection_view * world_position;
}