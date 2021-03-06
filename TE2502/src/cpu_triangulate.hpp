#pragma once

#include "graphics/debug_drawer.hpp"
#include "tfile.hpp"
#include "camera.hpp"

namespace cputri
{
	void setup(TFile& tfile);

	void destroy();

	void run(DebugDrawer& dd, Camera& main_camera, Camera& current_camera, Window& window, bool show_imgui);

	// Finds a free chunk in m_buffer and returns it index, or INVALID if none was found
	uint32_t find_chunk();

	// For a node at the given position, return its index into m_buffer
	uint32_t get_offset(uint32_t node_x, uint32_t node_z);

	// Get offset for indices for index i in m_buffer
	uint64_t get_index_offset_of_node(uint32_t i);

	// Get offset for indices for index i in m_buffer
	uint64_t get_vertex_offset_of_node(uint32_t i);

	// Get offset for indices for index i in m_buffer
	uint64_t get_offset_of_node(uint32_t i);

	// Shifts the quadtree if required
	void shift_quadtree(glm::vec3 camera_pos);

	void generate_shader(uint32_t node_index, glm::vec2 min, glm::vec2 max);
	void triangle_process_shader(glm::mat4 vp,
		glm::vec4 camera_position,
		glm::vec2 screen_size,
		float threshold,
		float area_multiplier,
		float curvature_multiplier,
		uint32_t node_index);

	void triangulate();

	void triangulate_shader(uint32_t node_index);

	void replace_connection_index(uint32_t node_index, uint32_t triangle_to_check, uint32_t index_to_replace, uint32_t new_value);

	void clear_terrain();

	void process_triangles(Camera& camera, Window& window, float em_threshold, float area_multiplier, float curvature_multiplier);

	void draw_terrain(Frustum& frustum, DebugDrawer& dd, Camera& camera, Window& window);

	void intersect(Frustum& frustum, DebugDrawer& dd, glm::vec3 camera_pos);

	void intersect(Frustum& frustum, DebugDrawer& dd, AabbXZ aabb, uint32_t level, uint32_t x, uint32_t y);

	int intersect_triangle(glm::vec3 r_o, glm::vec3 r_d, glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, float* t);
}