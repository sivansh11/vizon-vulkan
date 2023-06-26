#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 biTangent;

layout (location = 0) out vec2 out_uv;

layout (set = 0, binding = 0) uniform GloablUniformBuffer {
    mat4 view;
    mat4 projection;
    mat4 invView;
    mat4 invProjection;
};

layout (set = 1, binding = 0) uniform PerObjectUniformBuffer {
    mat4 model;
    mat4 invModel;
};

void main() {
    gl_Position = projection * view * model * vec4(position, 1);
    out_uv = uv;
}


