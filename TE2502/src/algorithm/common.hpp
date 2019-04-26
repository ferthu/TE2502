#pragma once

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <array>

using namespace glm;
typedef uint32_t uint;
typedef const uint32_t cuint;

const uint INVALID = ~0u;

constexpr auto TERRAIN_GENERATE_TOTAL_SIDE_LENGTH = 1000;
constexpr auto TERRAIN_GENERATE_NUM_INDICES = 12000;
constexpr auto TERRAIN_GENERATE_NUM_VERTICES = 4000;
constexpr auto TERRAIN_GENERATE_NUM_NODES = 16;
constexpr auto TERRAIN_GENERATE_GRID_SIDE = 3;
constexpr auto TRIANGULATE_MAX_NEW_POINTS = 1024;
constexpr auto QUADTREE_LEVELS = 2;
constexpr auto MAX_BORDER_TRIANGLE_COUNT = 2000;
constexpr auto ADJUST_PERCENTAGE = 0.35f;

constexpr uint num_indices = TERRAIN_GENERATE_NUM_INDICES;
constexpr uint num_vertices = TERRAIN_GENERATE_NUM_VERTICES;
constexpr uint num_nodes = TERRAIN_GENERATE_NUM_NODES;
constexpr uint num_new_points = TRIANGULATE_MAX_NEW_POINTS;
constexpr uint quadtree_levels = QUADTREE_LEVELS;
constexpr uint max_border_triangle_count = MAX_BORDER_TRIANGLE_COUNT;

const float gaussian_width = 1.0f;
const int filter_radius = 2;	// Side length of grid is filter_radius * 2 + 1
const int filter_side = filter_radius * 2 + 1;

struct Triangle
{
	vec2 circumcentre;
	float circumradius2;
	uint pad;
};

struct BufferNodeHeader
{
	uint vertex_count;
	uint new_points_count;
	uint pad;

	vec2 min;
	vec2 max;

	uint border_count;
	std::array<uint, MAX_BORDER_TRIANGLE_COUNT> border_triangle_indices;
};

struct TerrainData
{
	uint index_count;
	uint instance_count;
	uint first_index;
	int  vertex_offset;
	uint first_instance;

	// struct BufferNodeHeader {
	uint vertex_count;
	uint new_points_count;
	uint pad;

	vec2 min;
	vec2 max;

	uint border_count;
	std::array<uint, MAX_BORDER_TRIANGLE_COUNT> border_triangle_indices;
	// }

	std::array<uint, TERRAIN_GENERATE_NUM_INDICES> indices;
	std::array<vec4, TERRAIN_GENERATE_NUM_VERTICES> positions;
	std::array<Triangle, TERRAIN_GENERATE_NUM_INDICES / 3> triangles;
	std::array<uint, TERRAIN_GENERATE_NUM_INDICES> triangle_connections;
	std::array<vec4, TRIANGULATE_MAX_NEW_POINTS> new_points;
	std::array<uint, TRIANGULATE_MAX_NEW_POINTS> new_points_triangles;
};

const uint quadtree_data_size = (1 << QUADTREE_LEVELS) * (1 << QUADTREE_LEVELS) + 4;
const uint pad_size = 16 - (quadtree_data_size % 16);

struct TerrainBuffer
{
	std::array<uint, (1 << QUADTREE_LEVELS) * (1 << QUADTREE_LEVELS)> quadtree_index_map;
	vec2 quadtree_min;
	vec2 quadtree_max;
	std::array<uint, pad_size> pad;
	std::array<TerrainData, TERRAIN_GENERATE_NUM_NODES> data;
};


struct GenerateInfo
{
	vec2 min;
	vec2 max;
	uint index;
};
struct Quadtree {
	// Number and array of indices to nodes that needs to generate terrain
	uint num_generate_nodes;
	GenerateInfo* generate_nodes;

	// Number and array of indices to nodes that needs to draw terrain
	uint num_draw_nodes;
	uint* draw_nodes;

	float total_side_length;

	// Max number of active nodes
	uint64_t max_nodes;

	// For chunk i of m_buffer, quadtree.buffer_index_filled[i] is true if that chunk is used by a node
	bool* buffer_index_filled;

	uint* node_index_to_buffer_index;

	uint64_t node_memory_size;
	vec2* quadtree_minmax;

	vec2 node_size;

	const float quadtree_shift_distance = 100.0f;
};

void replace_connection_index(TerrainBuffer* tb, cuint node_index, cuint triangle_to_check, cuint index_to_replace, cuint new_value);
float curvature(vec3 p, float* log_filter);

// TERRAIN STUFF
float mix(float a, float b, float i);
float clamp(float a, float min, float max);

const mat2 rotate2D = mat2(1.3623f, 1.7531f, -1.7131f, 1.4623f);
float hash12(vec2 p);
float noise(vec2 x);
float terrain(vec2 p);
float height_to_surface(vec3 p);


void line_from_points(vec2 p1, vec2 p2, float& a, float& b, float& c);
void perpendicular_bisector_from_line(vec2 p1, vec2 p2, float& a, float& b, float& c);
vec2 line_line_intersection(float a1, float b1, float c1, float a2, float b2, float c2);
vec2 find_circum_center(vec2 P, vec2 Q, vec2 R);
float find_circum_radius_squared(float a, float b, float c);
float find_circum_radius_squared(vec2 P, vec2 Q, vec2 R);
