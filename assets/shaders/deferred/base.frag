#version 450

layout (location = 0) in vec2 uv;
layout (location = 1) in vec3 T;
layout (location = 2) in vec3 B;
layout (location = 3) in vec3 N;

layout (location = 0) out vec4 finalAlbedoSpec;
layout (location = 1) out vec4 finalNormal;

layout (set = 2, binding = 0) uniform sampler2D diffuseMap;
layout (set = 2, binding = 1) uniform sampler2D specularMap;
layout (set = 2, binding = 2) uniform sampler2D normalMap;

vec4 diffuse;
float specular;
vec3 normal;
mat3 TBN;

void main() {
    diffuse = texture(diffuseMap, uv);

    if (diffuse.a < 0.05) discard;

    specular = texture(specularMap, uv).r;
    normal = texture(normalMap, uv).rgb * 2.0f - 1.0f;
    TBN = mat3(T, B, N);
    
    finalAlbedoSpec.rgb = diffuse.rgb;
    finalAlbedoSpec.a = specular;
    finalNormal = vec4(normalize(TBN * normal), 1);
}