#version 450

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 finalColor;

struct DirectionalLight {
    vec3 position;
    vec3 color;
    vec3 ambience;
    vec3 term;
};

layout (set = 0, binding = 0) uniform sampler2D texAlbedoSpec;
layout (set = 0, binding = 1) uniform sampler2D texNormal;
layout (set = 0, binding = 2) uniform sampler2D texDepth;
layout (set = 0, binding = 3) uniform sampler2DShadow texShadowMap;
layout (set = 0, binding = 4) uniform sampler2D texSSAO;
layout (set = 0, binding = 5) uniform CompositeSet0UBO {
    mat4 lightSpace;
    mat4 view;
    mat4 projection;
    mat4 invView;
    mat4 invProjection;
    DirectionalLight directionalLight;
};

vec4 getViewPositionFromDepth(vec2 uv, float depth);
float calculateShadow();

void main() {
    vec3 albedo = texture(texAlbedoSpec, uv).rgb;
    
    float visibility = calculateShadow();

    finalColor = vec4((albedo * visibility) + (directionalLight.ambience * albedo * texture(texSSAO, uv).r), 1);
}

float calculateShadow() {
    vec4 worldPosition = invView * getViewPositionFromDepth(uv, texture(texDepth, uv).r);
    vec4 lightSpacePosition = lightSpace * worldPosition;
    vec3 projCoords = lightSpacePosition.xyz / lightSpacePosition.w;
    vec2 uvCoords;
    uvCoords.x = projCoords.x;
    uvCoords.y = projCoords.y;
    uvCoords.xy = (uvCoords.xy + 1) / 2;
    float z = projCoords.z;
    ivec2 size = textureSize(texShadowMap, 0);
    float xOffset = 1.0 / float(size.x);
    float yOffset = 1.0 / float(size.y);
    float factor = 0.0;
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec2 offset = vec2(float(x) * xOffset, float(y) * yOffset);
            vec3 uvc = vec3(uvCoords + offset, z - 0.005);
            factor += texture(texShadowMap, uvc);
        }
    }
    factor /= 9.0;

    return factor;
}


vec4 getViewPositionFromDepth(vec2 uv, float depth) {
    float z = depth;
    vec4 clipSpacePosition = vec4(uv * 2.0 - 1.0, z, 1.0);
    vec4 viewSpacePosition = invProjection * clipSpacePosition;
    viewSpacePosition /= viewSpacePosition.w;
    return viewSpacePosition.xyzw;
}