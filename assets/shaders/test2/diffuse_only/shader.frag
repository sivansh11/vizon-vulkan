#version 450 

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 out_diffuse;

layout (set = 2, binding = 0) uniform sampler2D diffuse_map;

void main() {
    out_diffuse = texture(diffuse_map, uv);
}