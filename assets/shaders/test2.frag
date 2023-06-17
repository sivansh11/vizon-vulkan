#version 450

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform UBO {
    float t;
} ubo;

layout (set = 0, binding = 1) uniform sampler2D tex;

void main() {
    outColor = texture(tex, uv);
}