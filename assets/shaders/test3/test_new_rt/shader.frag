#version 450

#extension GL_EXT_ray_tracing : enable

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler2D screen;

void main() {
    vec3 camera_origin;
    
    outColor = texture(screen, uv);
}