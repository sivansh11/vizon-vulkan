#version 450

layout (location = 0) in vec2 fragment_uv;
// layout (location = 1) in vec3 fragment_T;
// layout (location = 2) in vec3 fragment_B;
// layout (location = 3) in vec3 fragment_N;

layout (location = 0) out vec4 gbuffer_albedo;

layout (set = 1, binding = 0) uniform sampler2D diffuse_map;

void main() {
    vec4 diffuse = texture(diffuse_map, fragment_uv);
    if (diffuse.a < 0.05) discard;

    gbuffer_albedo.rgba = vec4(diffuse.rgb, 1);
}
