#pragma once

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <array>
#include "array.hpp"

#include "terrain_interface.h"

using namespace glm;
typedef uint32_t uint;
typedef const uint32_t cuint;

const uint INVALID = ~0u;
const uint UNKNOWN = INVALID - 9;

const float TERRAIN_GENERATE_TOTAL_SIDE_LENGTH = max_view_dist * 2 + 100;
constexpr uint TERRAIN_GENERATE_NUM_INDICES = 6000;
constexpr uint TERRAIN_GENERATE_NUM_VERTICES = 2000;
constexpr uint TERRAIN_GENERATE_GRID_SIDE = 3;
constexpr uint TRIANGULATE_MAX_NEW_POINTS = 1024;
constexpr uint QUADTREE_LEVELS = 6;
constexpr uint MAX_BORDER_TRIANGLE_COUNT = 700;

static_assert(TERRAIN_GENERATE_GRID_SIDE % 2 == 1, "TERRAIN_GENERATE_GRID_SIDE must be an uneven number.");

constexpr uint num_indices = TERRAIN_GENERATE_NUM_INDICES;
constexpr uint num_vertices = TERRAIN_GENERATE_NUM_VERTICES;
constexpr uint num_nodes = (1 << QUADTREE_LEVELS) * (1 << QUADTREE_LEVELS);
constexpr uint num_new_points = TRIANGULATE_MAX_NEW_POINTS;
constexpr uint quadtree_levels = QUADTREE_LEVELS;
constexpr uint max_border_triangle_count = MAX_BORDER_TRIANGLE_COUNT;

const float gaussian_width = 1.0f;
const int filter_radius = 1;	// Side length of grid is filter_radius * 2 + 1
const int filter_side = filter_radius * 2 + 1;

struct Triangle
{
	vec2 circumcentre;
	float circumradius2;
};

struct TerrainData
{
	std::array<uint, TERRAIN_GENERATE_NUM_INDICES> indices;
	std::array<vec4, TERRAIN_GENERATE_NUM_VERTICES> positions;
	std::array<Triangle, TERRAIN_GENERATE_NUM_INDICES / 3> triangles;
	std::array<uint, TERRAIN_GENERATE_NUM_INDICES> triangle_connections;
	std::array<uint, MAX_BORDER_TRIANGLE_COUNT> border_triangle_indices;
	std::array<vec4, TRIANGULATE_MAX_NEW_POINTS> new_points;
	std::array<uint, TRIANGULATE_MAX_NEW_POINTS> new_points_triangles;

	bool is_invalid;

	uint index_count;
	uint vertex_count;
	uint new_points_count;
	uint draw_index_count;

	uint border_count;

	vec2 min;
	vec2 max;

	// Copy data info
	bool has_data_to_copy;
	uint old_vertex_count;
	uint lowest_indices_index_changed;
};

const uint quadtree_data_size = (1 << QUADTREE_LEVELS) * (1 << QUADTREE_LEVELS) + 4;

struct TerrainBuffer
{
	uint* quadtree_index_map;
	vec2* quadtree_min;
	vec2* quadtree_max;
	Array<TerrainData, num_nodes> data;
};


struct GenerateInfo
{
	vec2 min;
	vec2 max;
	uint index;
	uint x;
	uint y;
};
struct Quadtree
{
	// Number and array of indices to nodes that needs to generate terrain
	uint num_generate_nodes;
	GenerateInfo generate_nodes[num_nodes];
	uint num_generate_nodes_draw;
	GenerateInfo generate_nodes_draw[num_nodes];

	uint generated_triangle_count;

	// Number and array of indices to nodes that needs to draw terrain
	uint num_draw_nodes;
	uint draw_nodes[num_nodes];
	uint num_draw_nodes_draw;
	uint draw_nodes_draw[num_nodes];

	uint drawn_triangle_count;
	uint new_points_added;

	float total_side_length;

	// Max number of active nodes
	uint64_t max_nodes;

	// For chunk i of m_buffer, quadtree.buffer_index_filled[i] is true if that chunk is used by a node
	bool buffer_index_filled[num_nodes];

	uint node_index_to_buffer_index[num_nodes];

	uint64_t node_memory_size;

	vec2 node_size;

	vec2 quadtree_min;
	vec2 quadtree_max;

	const float quadtree_shift_distance = TERRAIN_GENERATE_TOTAL_SIDE_LENGTH / 2.f - TERRAIN_GENERATE_TOTAL_SIDE_LENGTH / (1 << quadtree_levels);
};

void replace_connection_index(TerrainBuffer* tb, cuint node_index, cuint triangle_to_check, cuint index_to_replace, cuint new_value);
float curvature(vec3 p, float* log_filter);

void line_from_points(vec2 p1, vec2 p2, float& a, float& b, float& c);
void perpendicular_bisector_from_line(vec2 p1, vec2 p2, float& a, float& b, float& c);
vec2 line_line_intersection(float a1, float b1, float c1, float a2, float b2, float c2);
vec2 find_circum_center(vec2 P, vec2 Q, vec2 R);
float find_circum_radius_squared(float a, float b, float c);
float find_circum_radius_squared(vec2 P, vec2 Q, vec2 R);
