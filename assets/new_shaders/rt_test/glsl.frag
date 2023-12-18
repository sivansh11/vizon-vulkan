#version 450
#extension GL_EXT_scalar_block_layout : enable

#define invalid_hit_id uint(-1)

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 out_color;

// maybe make these vec4s to gain ez perf 
struct aabb_t {
    vec3 min, max;
};

struct triangle_t {
    vec3 p0, p1, p2;
};

struct node_t {
    aabb_t aabb;
    uint primitive_count;
    uint first_index;
};

struct hit_t {
    uint primitive_id;
};

layout (set = 0, binding = 0, scalar) readonly buffer triangles_ssbo {
    triangle_t triangles[];
};

layout (set = 0, binding = 1, scalar) readonly buffer nodes_ssbo {
    node_t nodes[];
};

layout (set = 0, binding = 2, scalar) readonly buffer indices_ssbo {
    uint indices[];
};

layout (set = 0, binding = 3, scalar) uniform ubo {
    mat4 view;
    mat4 projection;

    mat4 inverse_view;
    mat4 inverse_projection;

    int num_tris;

    vec3 eye;
    vec3 dir;
    vec3 up;
    vec3 right;
};

hit_t closest_hit(in vec3 origin, in vec3 direction, inout float tmin, inout float tmax);
vec2 node_intersect(in node_t node, in vec3 origin, in vec3 direction, in float tmin, in float tmax);
bool node_is_leaf(in node_t node);
bool triangle_intersect(in triangle_t triangle, in vec3 origin, in vec3 direction, inout float tmin, inout float tmax);
bool is_hit(in hit_t hit);

void main() {
    vec2 d = uv * 2 - 1;

	vec4 origin = inverse_view * vec4(0, 0, 0, 1);
	vec4 target = inverse_projection * vec4 (d.x, d.y, 1, 1);
	vec4 direction = inverse_view * vec4(normalize(target.xyz), 0);

    // vec2 d = uv * 2.0 - 1.0;

    // vec3 origin = eye;
    // vec3 direction = dir + d.x * right + d.y * up;

    float tmin = 0;
    float tmax = 3.402823466e+38;

    hit_t hit;
    hit.primitive_id = invalid_hit_id;
    #if 1
    hit = closest_hit(origin.xyz, direction.xyz, tmin, tmax);
    #else
    for (int i = 0; i < num_tris; i++) {
        if (triangle_intersect(triangles[i], origin.xyz, direction.xyz, tmin, tmax)) {
            hit.primitive_id = i;
        }
    }
    #endif  


    if (is_hit(hit)) {
        // out_color = vec4(hit.primitive_id * 37.0 / 255.0, hit.primitive_id * 91.0 / 255.0, hit.primitive_id * 51.0 / 255.0, 1);
        out_color = vec4(1, 1, 1, 1);
    } else {
        out_color = vec4(0, 0, 0, 0);
    }
}

bool is_hit(in hit_t hit) {
    return hit.primitive_id != invalid_hit_id;
}

vec2 node_intersect(in node_t node, in vec3 origin, in vec3 direction, in float tmin, in float tmax) {
    vec3 inverse_direction = 1.0 / direction;

    vec3 tmin_vec = (node.aabb.min - origin) * inverse_direction;
    vec3 tmax_vec = (node.aabb.max - origin) * inverse_direction;

    vec3 t0 = min(tmin_vec, tmax_vec);
    vec3 t1 = max(tmin_vec, tmax_vec);

    float _tmin = max(t0.x, max(t0.y, max(t0.z, tmin)));
    float _tmax = min(t1.x, min(t1.y, min(t1.z, tmax)));

    return vec2(_tmin, _tmax);
}

bool node_is_leaf(in node_t node) {
    return node.primitive_count > 0;
}

bool triangle_intersect(in triangle_t triangle, in vec3 origin, in vec3 direction, inout float tmin, inout float tmax) {
    vec3 e1 = triangle.p0 - triangle.p1;
    vec3 e2 = triangle.p2 - triangle.p0;
    vec3 n = cross(e1, e2);

    vec3 c = triangle.p0 - origin;
    vec3 r = cross(direction, c);
    float inverse_det = 1.0 / dot(n, direction);

    float u = dot(r, e2) * inverse_det;
    float v = dot(r, e1) * inverse_det;
    float w = 1.0 - u - v;

    if (u >= 0 && v >= 0 && w >= 0) {
        float t = dot(n, c) * inverse_det;
        if (t >= tmin && t <= tmax) {
            tmax = t;
            return true;
        }
    }
    return false;
}

hit_t closest_hit(in vec3 origin, in vec3 direction, inout float tmin, inout float tmax) {
    hit_t hit;
    hit.primitive_id = invalid_hit_id;

    #if 0
    const uint MAX_STACK_SIZE = 64;
    uint stack[MAX_STACK_SIZE];
    uint stack_ptr = 0;
    stack[stack_ptr++] = 0;  // root node id

    while (stack_ptr != 0 && stack_ptr < MAX_STACK_SIZE) {
        node_t node = nodes[stack[stack_ptr--]];
        vec2 intr = node_intersect(node, origin, direction, tmin, tmax);
        if (!(intr.y >= intr.x)) 
            continue;
        
        if (node_is_leaf(node)) {
            // hit_t hit;
            // hit.primitive_id = 1;
            // return hit;
            for (uint i = 0; i < node.primitive_count; i++) {
                uint primitive_id = indices[node.first_index + i];
                if (triangle_intersect(triangles[primitive_id], origin, direction, tmin, tmax))
                    hit.primitive_id = primitive_id;
            }
        } else {
            stack[stack_ptr++] = node.first_index;
            stack[stack_ptr++] = node.first_index + 1;
        }
    }
    #else
    const uint MAX_STACK_SIZE = 32;
    const bool is_any = true;
    uint stack[MAX_STACK_SIZE];
    uint stack_size = 0;
    uint top = 1;

    while (true) {
        node_t left = nodes[top];
        node_t right = nodes[top + 1];

        vec2 intr_left = node_intersect(left, origin, direction, tmin, tmax);
        vec2 intr_right = node_intersect(right, origin, direction, tmin, tmax);
        bool hit_left = intr_left.y >= intr_left.x;
        bool hit_right = intr_right.y >= intr_right.x;

        uint primitive_count = (hit_left ? left.primitive_count : 0) + (hit_right ? right.primitive_count : 0);
        if (primitive_count > 0) {
            uint first_primitive = hit_left && node_is_leaf(left) ? left.first_index : right.first_index;
            for (uint i = 0; i < primitive_count; i++) {
                triangle_t triangle = triangles[first_primitive + i];
                if (triangle_intersect(triangle, origin, direction, tmin, tmax)) {
                    hit.primitive_id = first_primitive + i;
                    if (is_any)
                        return hit;
                }
            }
            hit_left = hit_left && !node_is_leaf(left);
            hit_right = hit_right && !node_is_leaf(right);

        }

        if (hit_left) {
            if (hit_right) {
                uint first_child = left.first_index;
                uint second_child = right.first_index;
                if (!is_any && intr_left.x > intr_right.x) {
                    uint tmp = first_child;
                    first_child = second_child;
                    second_child = tmp;
                }
                stack[stack_size++] = second_child;
                top = first_child;
            } else {
                top = left.first_index;
            }
            continue;
        } else if (hit_right) {
            top = right.first_index;
            continue;
        }

        if (stack_size == 0) 
            break;
        top = stack[--stack_size];
    }

    #endif
    return hit;
}