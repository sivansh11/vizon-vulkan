#include "bvh.hpp"


float robust_min(float a, float b) { return a < b ? a : b; }
float robust_max(float a, float b) { return a > b ? a : b; }
float safe_inverse(float x) {
    return std::fabs(x) <= std::numeric_limits<float>::epsilon()
        ? std::copysign(1.0f / std::numeric_limits<float>::epsilon(), x)
        : 1.0f / x;
}


int aabb_t::largest_axis() const {
    const glm::vec3 d = diagonal();
    int axis = 0;
    if (d[axis] < d[1]) axis = 1;
    if (d[axis] < d[2]) axis = 2;
    return axis;
}

float aabb_t::half_area() const {
    glm::vec3 d = diagonal();
    return (d[0] + d[1]) * d[2] + d[0] * d[1];
}

const aabb_t aabb_t::empty() {
    return {glm::vec3{+std::numeric_limits<float>::max()}, glm::vec3{-std::numeric_limits<float>::max()}};
}

aabb_t& aabb_t::extend(const aabb_t& other) {
    min = glm::min(min, other.min);
    max = glm::max(max, other.max);
    return *this;
}

aabb_t& aabb_t::extend(const glm::vec3& point) {
    min = glm::min(min, point);
    max = glm::max(max, point);
    return *this;
}

glm::vec3 aabb_t::diagonal() const {
    return max - min;
}


glm::vec3 ray_t::inverse_direction() const {
    return glm::vec3{ safe_inverse(direction[0]), safe_inverse(direction[1]), safe_inverse(direction[2]) };
}


bool triangle_t::intersect(ray_t& ray) const {
    glm::vec3 e1 = p0 - p1;
    glm::vec3 e2 = p2 - p0;
    glm::vec3 n = glm::cross(e1, e2);

    glm::vec3 c = p0 - ray.origin;
    glm::vec3 r = glm::cross(ray.direction, c);
    float inverse_det = 1.0f / glm::dot(n, ray.direction);

    float u = glm::dot(r, e2) * inverse_det;
    float v = glm::dot(r, e1) * inverse_det;
    float w = 1.0f - u - v;

    if (u >= 0 && v >= 0 && w >= 0) {
        float t = glm::dot(n, c) * inverse_det;
        if (t >= ray.tmin && t <= ray.tmax) {
            ray.tmax = t;
            return true;
        }
    }
    return false;
}


bool node_t::intersect(const ray_t& ray) const {
    glm::vec3 inverse_direction = ray.inverse_direction();
    glm::vec3 tmin = (aabb.min - ray.origin) * inverse_direction;
    glm::vec3 tmax = (aabb.max - ray.origin) * inverse_direction;
    std::tie(tmin, tmax) = std::make_pair(glm::min(tmin, tmax), glm::max(tmin, tmax));
    // return intersection_t
    float _tmin = robust_max(tmin[0], robust_max(tmin[1], robust_max(tmin[2], ray.tmin)));
    float _tmax = robust_min(tmax[0], robust_min(tmax[1], robust_min(tmax[2], ray.tmax)));
    return _tmin <= _tmax;
}


bvh_t bvh_t::build(const aabb_t *aabbs, const glm::vec3 *centers, uint32_t primitive_count) {
    bvh_t bvh{};

    bvh.primitive_indices.resize(primitive_count);
    std::iota(bvh.primitive_indices.begin(), bvh.primitive_indices.end(), 0);

    bvh.nodes.resize(2 * primitive_count - 1);
    bvh.nodes[0].primitive_count = primitive_count;
    bvh.nodes[0].first_index = 0;

    uint32_t node_count = 1;
    build_recursive(bvh, 0, node_count, aabbs, centers);
    bvh.nodes.resize(node_count);
    return bvh;
}

uint32_t bvh_t::depth(uint32_t node_index) const {
    auto& node = nodes[node_index];
    return node.is_leaf() ? 1 : 1 + glm::max(depth(node.first_index), depth(node.first_index + 1));
}


bvh_t::bin_t& bvh_t::bin_t::extend(const bin_t& other) {
    aabb.extend(other.aabb);
    primitive_count += other.primitive_count;
    return *this;
}

uint32_t bvh_t::bin_t::bin_index(int axis, const aabb_t& aabb, const glm::vec3& center) {
    int index = (center[axis] - aabb.min[axis]) * (build_config.bin_count / (aabb.max[axis] - aabb.min[axis]));
    return std::min(build_config.bin_count - 1, static_cast<uint32_t>(std::max(0, index)));
}

bvh_t::split_t bvh_t::split_t::find_best_split(int axis, const bvh_t& bvh, const node_t& node, const aabb_t *aabbs, const glm::vec3 *centers) {
    // todo: change this to alloca
    // bin_t bins[build_config.bin_count] = {};
    bin_t *bins = reinterpret_cast<bin_t *>(alloca(build_config.bin_count * sizeof(bin_t)));
    for (uint32_t i = 0; i < build_config.bin_count; i++) {
        bins[i] = {};
    }

    for (uint32_t i = 0; i < node.primitive_count; i++) {
        uint32_t primitive_index = bvh.primitive_indices[node.first_index + i];
        bin_t& bin = bins[bin_t::bin_index(axis, node.aabb, centers[primitive_index])];
        bin.aabb.extend(aabbs[primitive_index]);
        bin.primitive_count++;
    }

    // todo: change this to alloca
    // float right_cost[build_config.bin_count] = {};
    float *right_cost = reinterpret_cast<float *>(alloca(build_config.bin_count * sizeof(float)));
    for (uint32_t i = 0; i < build_config.bin_count; i++) {
        right_cost[i] = {};
    }

    bin_t left_accumulation, right_accumulation;
    
    for (uint32_t i = build_config.bin_count - 1; i > 0; i--) {
        right_accumulation.extend(bins[i]);
        right_cost[i] = right_accumulation.cost();
    }

    split_t split{};
    split.axis = axis;
    
    for (uint32_t i = 0; i < build_config.bin_count - 1; i++) {
        left_accumulation.extend(bins[i]);
        float cost = left_accumulation.cost() + right_cost[i + 1];

        if (cost < split.cost) {
            split.cost = cost;
            split.right_bin = i + 1;
        }
    }
    return split;
}


void bvh_t::build_recursive(bvh_t& bvh, uint32_t node_index, uint32_t& node_count, const aabb_t *aabbs, const glm::vec3 *centers) {
    node_t& node = bvh.nodes[node_index];
    assert(node.is_leaf());

    node.aabb = aabb_t::empty();
    for (uint32_t i = 0; i < node.primitive_count; i++) 
        node.aabb.extend(aabbs[bvh.primitive_indices[node.first_index + i]]);
    
    if (node.primitive_count <= build_config.min_primitives) 
        return;
    
    split_t min_split;
    for (int axis = 0; axis < 3; axis++) 
        min_split = std::min(min_split, split_t::find_best_split(axis, bvh, node, aabbs, centers));
    
    float leaf_cost = node.aabb.half_area() * (node.primitive_count - build_config.traversal_cost);
    uint32_t first_right;
    if (!min_split || min_split.cost >= leaf_cost) {
        if (node.primitive_count > build_config.max_primitives) {
            int axis = node.aabb.largest_axis();
            std::sort(
                bvh.primitive_indices.begin() + node.first_index,
                bvh.primitive_indices.begin() + node.first_index + node.primitive_count,
                [&](uint32_t i, uint32_t j) {  return centers[i][axis] < centers[j][axis]; });
            first_right = node.first_index + node.primitive_count / 2;
        } else 
            return;
    } else {
        first_right = std::partition(
                bvh.primitive_indices.begin() + node.first_index,
                bvh.primitive_indices.begin() + node.first_index + node.primitive_count,
                [&](uint32_t i) { return bin_t::bin_index(min_split.axis, node.aabb, centers[i]) < min_split.right_bin;}) 
                - bvh.primitive_indices.begin();
    }

    uint32_t first_child = node_count;
    node_t& left = bvh.nodes[first_child];
    node_t& right = bvh.nodes[first_child + 1];
    node_count += 2;

    left.primitive_count = first_right - node.first_index;
    right.primitive_count = node.primitive_count - left.primitive_count;

    left.first_index = node.first_index;
    right.first_index = first_right;

    node.first_index = first_child;
    node.primitive_count = 0;

    build_recursive(bvh, first_child, node_count, aabbs, centers);
    build_recursive(bvh, first_child + 1, node_count, aabbs, centers);
}

const bvh_t::build_config_t bvh_t::build_config;
