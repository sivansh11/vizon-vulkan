#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 biTangent;

layout (location = 0) out vec2 out_uv;
layout (location = 1) out vec3 out_T;
layout (location = 2) out vec3 out_B;
layout (location = 3) out vec3 out_N;

layout (set = 0, binding = 0) uniform Set0UBO {
    mat4 projection;
    mat4 view;
};

layout (set = 1, binding = 0) uniform Set1UBO {
    mat4 model;
    mat3 invModelT;
};

void main() {
    out_T = normalize(vec3(invModelT * tangent));
    out_B = normalize(vec3(invModelT * biTangent));
    out_N = normalize(vec3(invModelT * normal));

    vec4 worldPosition = model * vec4(position, 1);
    out_uv = uv;

    gl_Position = projection * view * worldPosition;
}
