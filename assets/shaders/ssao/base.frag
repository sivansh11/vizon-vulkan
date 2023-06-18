#version 450

layout (location = 0) in vec2 uv;

layout (location = 0) out float outAmbientOcclusion;

const vec3 samples[26] = { // HIGH (26 samples)
    vec3(0.2196607,0.9032637,0.2254677),
    vec3(0.05916681,0.2201506,0.1430302),
    vec3(-0.4152246,0.1320857,0.7036734),
    vec3(-0.3790807,0.1454145,0.100605),
    vec3(0.3149606,-0.1294581,0.7044517),
    vec3(-0.1108412,0.2162839,0.1336278),
    vec3(0.658012,-0.4395972,0.2919373),
    vec3(0.5377914,0.3112189,0.426864),
    vec3(-0.2752537,0.07625949,0.1273409),
    vec3(-0.1915639,-0.4973421,0.3129629),
    vec3(-0.2634767,0.5277923,0.1107446),
    vec3(0.8242752,0.02434147,0.06049098),
    vec3(0.06262707,-0.2128643,0.03671562),
    vec3(-0.1795662,-0.3543862,0.07924347),
    vec3(0.06039629,0.24629,0.4501176),
    vec3(-0.7786345,-0.3814852,0.2391262),
    vec3(0.2792919,0.2487278,0.05185341),
    vec3(0.1841383,0.1696993,0.8936281),
    vec3(-0.3479781,0.4725766,0.719685),
    vec3(-0.1365018,-0.2513416,0.470937),
    vec3(0.1280388,-0.563242,0.3419276),
    vec3(-0.4800232,-0.1899473,0.2398808),
    vec3(0.6389147,0.1191014,0.5271206),
    vec3(0.1932822,-0.3692099,0.6060588),
    vec3(-0.3465451,-0.1654651,0.6746758),
    vec3(0.2448421,-0.1610962,0.1289366)
};

layout (set = 0, binding = 0) uniform sampler2D texDepth;
layout (set = 0, binding = 1) uniform sampler2D texNormal;
layout (set = 0, binding = 2) uniform sampler2D texNoise;
layout (set = 0, binding = 3) uniform SsaoSet0UBO {
    mat4 projection;
    mat4 invProjection;
    mat4 view;
    mat4 invView;
    float radius;
    float bias;
};

vec4 getViewPositionFromDepth(vec2 uv, float depth);

void main() {
    vec3 worldPosition = vec3(getViewPositionFromDepth(uv, texture(texDepth, uv).r));
    vec3 normal = mat3(view) * texture(texNormal, uv).xyz;
    vec3 r = normalize(texture(texNoise, uv).xyz);

    vec3 tangent = normalize(r - normal * dot(r, normal));
    vec3 biTangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, biTangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < 26; i++) {
        vec3 samplePos = TBN * vec3(samples[i]);
        samplePos = worldPosition + samplePos * radius;

        vec4 offset = vec4(samplePos, 1);
        offset = projection * offset;
        offset.xy /= offset.w;
        offset.xy = offset.xy * 0.5 + 0.5;

        float sampleDepth = getViewPositionFromDepth(offset.xy, texture(texDepth, offset.xy).r).z;

        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(worldPosition.z - sampleDepth));

        occlusion += (sampleDepth > samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }
    occlusion = 1.0 - (occlusion / float(26));
    outAmbientOcclusion = occlusion;
}

vec4 getViewPositionFromDepth(vec2 uv, float depth) {
    float z = depth;
    vec4 clipSpacePosition = vec4(uv * 2.0 - 1.0, z, 1.0);
    vec4 viewSpacePosition = invProjection * clipSpacePosition;
    viewSpacePosition /= viewSpacePosition.w;
    return viewSpacePosition.xyzw;
}