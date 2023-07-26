#version 460
#extension GL_EXT_ray_query : enable


layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 outColor;

layout (binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout (binding = 0, set = 1) uniform properties {
	int width;
	int height;	
};

void main() {
    vec3 camera_origin;

	vec2 d = uv * 2 - 1;
	vec4 direction = vec4(d.x, d.y, 1, 1);

    rayQueryEXT rayQuery;
	rayQueryInitializeEXT(rayQuery, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, vec3(0, 0, -5), 0.001, direction.xyz, 10000);

	while(rayQueryProceedEXT(rayQuery)) {}

	switch (rayQueryGetIntersectionTypeEXT(rayQuery, true)) {
		case gl_RayQueryCommittedIntersectionTriangleEXT:
			outColor = vec4(1, 1, 1, 1);
			break;
		case gl_RayQueryCommittedIntersectionNoneEXT:
			outColor = vec4(1, 1, 0, 1);
			break;
	}

}