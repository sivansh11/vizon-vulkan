#version 450

#define MAX_COLOR_VALUES 256.0

layout (location = 0) in vec2 fragment_uv;
layout (location = 1) in vec3 fragment_ndc;

layout (set = 1, binding = 0) uniform sampler2D diffuse_map;

layout (set = 2, binding = 0, r32ui) uniform uimage3D voxels_r32ui;

void imageAtomicAverage(ivec3 pos, vec4 addingColor);

void main() {
    vec4 diffuse = texture(diffuse_map, fragment_uv);
    if (diffuse.a < 0.05) discard;

    vec4 final_color = vec4(diffuse.rgb, 1);

    vec3 uvw = fragment_ndc * 0.5 + 0.5;  // go from [-1, 1] -> [0, 1]
    ivec3 voxel_position = ivec3(uvw * imageSize(voxels_r32ui));

    // imageStore(voxels_r32ui, voxel_position, final_color);
    imageAtomicAverage(voxel_position, final_color);
}

void imageAtomicAverage(ivec3 pos, vec4 addingColor) {
    // New value to store in the image.
    uint newValue = packUnorm4x8(addingColor);
    // Expected value, that data can be stored in the image.
    uint expectedValue = 0;
    // Actual data in the image.
    uint actualValue;

    // Average/sum of the voxel.
    vec4 color; 
    
    // Try to store ...
    actualValue = imageAtomicCompSwap(voxels_r32ui, pos, expectedValue, newValue);
    
    while (actualValue != expectedValue) {
        // ... but did not work out, as the expected value did not match the actual value.
        expectedValue = actualValue; 
    
        // Unpack the current average ...
        color = unpackUnorm4x8(actualValue);
        // ... and convert back to the sum.
        color.a *= MAX_COLOR_VALUES;
        color.rgb *= color.a;
            
        // Add the current color ...     
        color.rgb += addingColor.rgb;
        color.a += 1.0;
        
        // ... and normalize back.
        color.rgb /= color.a;
        color.a /= MAX_COLOR_VALUES;
    
        // Pack and ...
        newValue = packUnorm4x8(color);
        
        // ... try to store again ...
        actualValue = imageAtomicCompSwap(voxels_r32ui, pos, expectedValue, newValue);
    }
}
