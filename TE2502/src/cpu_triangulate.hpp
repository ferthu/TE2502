#pragma once

#include "graphics/debug_drawer.hpp"
#include "tfile.hpp"
#include "camera.hpp"

namespace cputri
{
	void setup(TFile& tfile);

	void destroy();

	void run(DebugDrawer& dd, Camera& camera, Window& window);

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

	void generate_shader(uint32_t node_index, glm::vec2 min, glm::vec2 max);
	void triangle_process_shader(glm::mat4 vp,
		glm::vec4 camera_position,
		glm::vec2 screen_size,
		float threshold,
		float area_multiplier,
		float curvature_multiplier,
		uint32_t node_index);

	void triangulate_shader(uint32_t node_index);

	void triangulate_borders_shader(uint32_t node_index);

	void triangulate();

	void clear_terrain();

	void process_triangles(Camera& camera, Window& window, float em_threshold, float area_multiplier, float curvature_multiplier);

	void draw_terrain(Frustum& frustum, DebugDrawer& dd, Camera& camera);

	void intersect(Frustum& frustum, DebugDrawer& dd);

	void intersect(Frustum& frustum, DebugDrawer& dd, AabbXZ aabb, uint32_t level, uint32_t x, uint32_t y);
}