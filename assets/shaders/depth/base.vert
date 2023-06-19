#version 450

layout (location = 0) in vec3 position;

layout (set = 0, binding = 0) uniform DepthSet0UBO {
    mat4 projection;
    mat4 view;
    mat4 model;
};

void main() {
    vec4 worldPosition = model * vec4(position, 1);

    gl_Position = projection * view * worldPosition;
}
