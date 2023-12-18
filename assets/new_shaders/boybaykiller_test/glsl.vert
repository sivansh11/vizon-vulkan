#version 450

layout (location = 0) in vec3 vertex_position;
// layout (location = 1) in vec3 vertex_normal;
layout (location = 2) in vec2 vertex_uv;
// layout (location = 3) in vec3 vertex_tangent;
// layout (location = 4) in vec3 vertex_bitangent;

layout (location = 0) out vec2 fragment_uv;
layout (location = 1) out vec3 fragment_ndc;

layout (set = 0, binding = 0) uniform voxelization_ubo_t {
    mat4 voxel_grid_matrix;  // create_ortho_off_center(grid_min, grid_max)
};

layout (push_constant) uniform push_constant_t {
    int render_axis;
};

void main() {
    vec4 frag_pos = /* model_matrix * */ vec4(vertex_position, 1);

    fragment_uv = vertex_uv;

    fragment_ndc = (voxel_grid_matrix * frag_pos).xyz; // going from [grid_min, grid_max] -> [-1, 1]

    gl_Position = vec4(fragment_ndc, 1);
    // if (render_axis == 0) gl_Position = gl_Position.xyzw;
    if (render_axis == 1) gl_Position = gl_Position.zyxw;
    if (render_axis == 2) gl_Position = gl_Position.xzyw;
}