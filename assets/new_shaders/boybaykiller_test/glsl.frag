#version 450

#define MAX_COLOR_VALUES 256.0

layout (location = 0) in vec2 fragment_uv;
layout (location = 1) in vec3 fragment_ndc;

layout (set = 1, binding = 0) uniform sampler2D diffuse_map;

layout (set = 2, binding = 0, r64ui) uniform uimage3D voxels_r64ui;

void main() {
    vec4 diffuse = texture(diffuse_map, fragment_uv);
    if (diffuse.a < 0.05) discard;

    vec4 final_color = vec4(diffuse.rgb, 1);

    vec3 uvw = fragment_ndc * 0.5 + 0.5;  // go from [-1, 1] -> [0, 1]
    ivec3 voxel_position = ivec3(uvw * imageSize(voxels_r64ui));

    // imageStore(voxels_r64ui, voxel_position, final_color);
    uvec4 quantized_light = uvec4(final_color * 255.0);
    uint packed_light = (quantized_light.a << 24) |
                        (quantized_light.b << 16) |
                        (quantized_light.g << 8 ) |
                        (quantized_light.r << 0 );
    imageAtomicMax(voxels_r64ui, voxel_position, packed_light);                       
}

