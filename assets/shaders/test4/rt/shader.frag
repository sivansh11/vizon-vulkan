#version 460
#extension GL_EXT_ray_query : enable
#extension GL_EXT_scalar_block_layout : enable

#define PI 3.14159265

layout (location = 0) in vec2 uv;

layout (location = 0) out vec3 outColor;

layout (binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout (binding = 1, set = 0) uniform uniform_data_t {
	mat4 view_inv;
	mat4 projection_inv;
} uniform_data;

struct vertex_t {
	vec3 position;
    vec3 normal;
    vec2 uv;
    vec3 tangent;
    vec3 bi_tangent;
};

layout (binding = 2, set = 0, scalar, std430) buffer vertex_buffer_t {
	vertex_t vertices[];
};

layout (binding = 3, set = 0, scalar) buffer index_buffer_t {
	uint indices[];
};

struct material_t {
	vec4 color;
};

layout (binding = 4, set = 0, scalar) buffer material_buffer_t {
	material_t materials[];
};

void main() {
	vec2 d = uv * 2 - 1;

	vec4 origin = uniform_data.view_inv * vec4(0, 0, 0, 1);
	vec4 target = uniform_data.projection_inv * vec4 (d.x, d.y, 1, 1);
	vec4 direction = uniform_data.view_inv * vec4(normalize(target.xyz), 0);

	rayQueryEXT rayQuery;
	rayQueryInitializeEXT(rayQuery, 
						  topLevelAS, 
						  gl_RayFlagsOpaqueEXT, 
						  0xFF, 
						  origin.xyz, 
						  0.001, 
						  direction.xyz, 
						  10000);

	while(rayQueryProceedEXT(rayQuery)) {}

	if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT) {
		int primitive_id = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);
		int instance_id = rayQueryGetIntersectionInstanceIdEXT(rayQuery, true);
		int custom_instance_id = rayQueryGetIntersectionInstanceCustomIndexEXT(rayQuery, true);
		float t = rayQueryGetIntersectionTEXT(rayQuery, true);
		vec3 barry_coords = vec3(0, rayQueryGetIntersectionBarycentricsEXT(rayQuery, true));
		barry_coords.x = 1 - barry_coords.y - barry_coords.z;

		uint i0 = indices[3 * (primitive_id + custom_instance_id) + 0];
		uint i1 = indices[3 * (primitive_id + custom_instance_id) + 1];
		uint i2 = indices[3 * (primitive_id + custom_instance_id) + 2];

		vertex_t v0 = vertices[i0];
		vertex_t v1 = vertices[i1];
		vertex_t v2 = vertices[i2];

		vec3 object_space_intersection = barry_coords.x * v0.position.xyz + barry_coords.y * v1.position.xyz + barry_coords.z * v2.position.xyz;

		// vec3 object_normal = normalize(cross((v1 - v0).xyz, (v2 - v0).xyz));

		outColor = vec3(0.5) + 0.25 * object_space_intersection;
		// outColor = vec3(0.5) + 0.5 * object_normal;
		// outColor = vec3((custom_instance_id + primitive_id) / 10.0, (custom_instance_id + primitive_id) / 100.0, (custom_instance_id + primitive_id) / 1000.0);
	} else {
		outColor = vec3(0);
	}
}

// vec3 sky_color(vec3 direction) {
// 	if (direction.y > 0.0) {
// 		return mix(vec3(1.0), vec3(0.25, 0.5, 1.0), direction.y);
// 	} else {
// 		return vec3(0.3);
// 	}
// }

// struct hit_info_t {
// 	vec3 color;
// 	vec3 world_position;
// 	vec3 world_normal;
// };

// hit_info_t get_object_hit_info(rayQueryEXT rayQuery) {
// 	hit_info_t hit_info;
// 	int primitiveID = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);
// 	int instanceID = rayQueryGetIntersectionInstanceIdEXT(rayQuery, true);
// 	int custom_instance_id = rayQueryGetIntersectionInstanceCustomIndexEXT(rayQuery, true);

// 	int final_primitiveID = custom_instance_id + primitiveID;

// 	material_t material = materials[instanceID];

// 	uint i0 = indices[3 * final_primitiveID + 0];
// 	uint i1 = indices[3 * final_primitiveID + 1];
// 	uint i2 = indices[3 * final_primitiveID + 2];
// 	vec4 v0 = vertices[i0];
// 	vec4 v1 = vertices[i1];
// 	vec4 v2 = vertices[i2];

// 	vec2 alpha_beta = rayQueryGetIntersectionBarycentricsEXT(rayQuery, true);
// 	vec3 barry_centrics = vec3(1.0 - alpha_beta.x - alpha_beta.y, alpha_beta.x, alpha_beta.y);

// 	hit_info.world_position = v0.xyz * barry_centrics.x + v1.xyz * barry_centrics.y + v2.xyz * barry_centrics.z;
// 	hit_info.world_normal = normalize(cross(v1.xyz - v0.xyz, v2.xyz - v0.xyz));
// 	hit_info
// 	hit_info.color = vec3(.7);
// 	hit_info.color = vec3(instanceID / 10.0, instanceID / 10.0, instanceID / 10.0);
// 	hit_info.color = vec3(material.color.rgb);
// 	// hit_info.color = vec3(primitiveID / 50.0, primitiveID / 50.0, primitiveID / 50.0);

// 	return hit_info;
// }

// uint pcg_hash(uint seed)
// {
//   uint state = seed * 747796405u + 2891336453u;
//   uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
//   return (word >> 22u) ^ word;
// }

// // Used to advance the PCG state.
// uint rand_pcg(inout uint rng_state)
// {
//   uint state = rng_state;
//   rng_state = rng_state * 747796405u + 2891336453u;
//   uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
//   return (word >> 22u) ^ word;
// }

// // Advances the prng state and returns the corresponding random float.
// float rand(inout uint state)
// {
//   uint x = rand_pcg(state);
//   state = x;
//   return float(x)*uintBitsToFloat(0x2f800004u);
// }

// vec3 rand_vec3(inout uint seed)
// {
//     return vec3(rand(seed), rand(seed), rand(seed));
// }
// float rand(inout uint seed, float min, float max)
// {
//     return min + (max-min)*rand(seed);
// }
// vec3 rand_vec3(inout uint seed, float min, float max)
// {
//     return vec3(rand(seed, min, max), rand(seed, min, max), rand(seed, min, max));
// }

// vec3 random_in_unit_sphere(inout uint seed)
// {
//     while(true)
//     {
//         vec3 p = rand_vec3(seed, -1, 1);
//         if (length(p) >= 1) continue;
//         return p;
//     }
// }
// vec3 random_unit_vector(inout uint seed)
// {
//     return normalize(random_in_unit_sphere(seed));
// }

// vec3 uniformSampleSphere(inout uint seed) {
//     float z = rand(seed, 0, .01) * 100 * 2 - 1;
//     float a = rand(seed, 0, .01) * 100 * 2 * PI;
//     float r = sqrt(1 - z * z);
//     float x = r * cos(a);
//     float y = r * sin(a);
//     return vec3(x, y, z);
// }

// vec3 cosineSampleHemiSphere(inout uint seed, vec3 normal) {
//     return normalize(normal + uniformSampleSphere(seed));
// }

// vec4 get_view_position_from_depth(vec2 uv, float depth) {
//     float z = depth * 2 - 1;
//     vec4 clipSpacePosition = vec4(uv * 2.0 - 1.0, z, 1.0);
//     vec4 viewSpacePosition = uniform_data.projection_inv * clipSpacePosition;

//     viewSpacePosition /= viewSpacePosition.w;

//     return viewSpacePosition.xyzw;
// }


// void main() {
// 	uint seed = pcg_hash(uint(gl_FragCoord.x + gl_FragCoord.y * 1000));

// 	vec2 d = uv * 2 - 1;

// 	// vec4 origin = uniform_data.view_inv * vec4(0, 0, 0, 1);
// 	// vec4 target = uniform_data.projection_inv * vec4 (d.x, d.y, 1, 1);
// 	// vec4 direction = uniform_data.view_inv * vec4(normalize(target.xyz), 0);

// 	vec3 pixel_color = vec3(0.0);

// 	int samples_per_pixel = 1;

// 	for (int s = 0; s < samples_per_pixel; s++) {
// 		vec4 origin = uniform_data.view_inv * vec4(0, 0, 0, 1);
// 		vec4 target = uniform_data.projection_inv * vec4 (d.x, d.y, 1, 1);
// 		vec4 direction = uniform_data.view_inv * vec4(normalize(target.xyz), 0);
// 		vec3 accumulated_ray_color = vec3(1.0);

// 		for (int traced_segments = 0; traced_segments < 5; traced_segments++) {
// 			rayQueryEXT rayQuery;
// 			rayQueryInitializeEXT(rayQuery, 
// 								topLevelAS, 
// 								gl_RayFlagsOpaqueEXT, 
// 								0xFF, 
// 								origin.xyz, 
// 								0.001, 
// 								direction.xyz, 
// 								10000);

// 			while(rayQueryProceedEXT(rayQuery)) {}	

// 			if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT) {
// 				hit_info_t hit_info = get_object_hit_info(rayQuery);
// 				// accumulated_ray_color *= vec3(.5) + 0.5 * hit_info.world_normal;
// 				accumulated_ray_color *= hit_info.color;
// 				hit_info.world_normal = faceforward(hit_info.world_normal, direction.xyz, hit_info.world_normal);
// 				origin.xyz = hit_info.world_position + 0.0001 * hit_info.world_normal;
// 				// direction.xyz = reflect(direction.xyz, hit_info.world_normal);
// 				direction.xyz = reflect(direction.xyz, hit_info.world_normal);
// 				// direction.xyz = cosineSampleHemiSphere(seed, hit_info.world_normal) + hit_info.world_position;
// 				continue;
// 			} else {
// 				pixel_color += accumulated_ray_color * sky_color(direction.xyz);
// 				break;
// 			}
// 		}
// 	}

// 	// vec4 origin = uniform_data.view_inv * vec4(0, 0, 0, 1);
// 	// vec4 target = uniform_data.projection_inv * vec4 (d.x, d.y, 1, 1);
// 	// vec4 direction = uniform_data.view_inv * vec4(normalize(target.xyz), 0);
// 	// vec3 accumulated_ray_color = vec3(1.0);

// 	// rayQueryEXT rayQuery;
// 	// rayQueryInitializeEXT(rayQuery, 
// 	// 					  topLevelAS, 
// 	// 					  gl_RayFlagsOpaqueEXT, 
// 	// 					  0xFF, 
// 	// 					  origin.xyz, 
// 	// 					  0.001, 
// 	// 					  direction.xyz, 
// 	// 					  10000);

// 	// while(rayQueryProceedEXT(rayQuery)) {}

// 	// if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT) {
// 	// 	int primitiveID = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);
// 	// 	int instanceID = rayQueryGetIntersectionInstanceIdEXT(rayQuery, true);
// 	// 	int custom_instance_id = rayQueryGetIntersectionInstanceCustomIndexEXT(rayQuery, true);
// 	// 	// uint i0 = indices[3 * primitiveID + 0];
// 	// 	// uint i1 = indices[3 * primitiveID + 1];
// 	// 	// uint i2 = indices[3 * primitiveID + 2];
// 	// 	// vec4 v0 = vertices[i0];
// 	// 	// vec4 v1 = vertices[i1];
// 	// 	// vec4 v2 = vertices[i2];

// 	// 	// rayQueryGetIntersectionInstanceIdEXT

// 	// 	// vec2 alpha_beta = rayQueryGetIntersectionBarycentricsEXT(rayQuery, true);
// 	// 	// vec3 barry_centrics = vec3(1.0 - alpha_beta.x - alpha_beta.y, alpha_beta.x, alpha_beta.y);

// 	// 	pixel_color = vec3((primitiveID + custom_instance_id) / 126.0, (primitiveID + custom_instance_id) / 126.0, (primitiveID + custom_instance_id) / 126.0);

// 	// } else {
// 	// 	pixel_color = accumulated_ray_color * sky_color(direction.xyz);
// 	// }

// 	outColor = vec4(pixel_color / float(samples_per_pixel), 1);	
	
// }