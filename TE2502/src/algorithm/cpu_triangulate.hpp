#pragma once

#include "graphics/debug_drawer.hpp"
#include "tfile.hpp"
#include "camera.hpp"

typedef uint32_t uint;

namespace cputri
{
	void setup(TFile& tfile);

	void destroy(VulkanContext& context, GPUBuffer& cpu_buffer);

	void run(DebugDrawer& dd, Camera& main_camera, Camera& current_camera, Window& window, bool show_imgui, bool refine);

	// Draws terrain. Requires an active render pass
	void draw(GraphicsQueue& queue, GPUBuffer& gpu_buffer, GPUBuffer& cpu_buffer);

	// Upload necessary data to GPU buffer
	void upload(GraphicsQueue& queue, GPUBuffer& gpu_buffer, GPUBuffer& cpu_buffer);

	// Finds a free chunk in m_buffer and returns it index, or INVALID if none was found
	uint32_t find_chunk();

	// For a node at the given position, return its index into m_buffer
	uint32_t get_offset(uint node_x, uint node_z);

	// Get offset for indices for index i in cpu buffer
	uint64_t get_cpu_index_offset_of_node(uint i);

	// Get offset for indices for index i in cpu buffer
	uint64_t get_cpu_vertex_offset_of_node(uint i);

	// Get offset for indices for index i in cpu buffer
	uint64_t get_cpu_offset_of_node(uint i);

	// Get offset for indices for index i in gpu buffer
	uint64_t get_gpu_index_offset_of_node(uint i);

	// Get offset for indices for index i in gpu buffer
	uint64_t get_gpu_vertex_offset_of_node(uint i);

	// Get offset for indices for index i in gpu buffer
	uint64_t get_gpu_offset_of_node(uint i);

	void filter_setup();

	void gpu_data_setup(VulkanContext& context, GPUMemory& gpu_mem, GPUMemory& cpu_mem, GPUBuffer& gpu_buffer, GPUBuffer& cpu_buffer);

	// Shifts the quadtree if required
	void shift_quadtree(glm::vec3 camera_pos);

	void triangulate();

	void clear_terrain();

	void process_triangles(Camera& camera, Window& window, float em_threshold, float area_multiplier, float curvature_multiplier);

	void draw_terrain(Frustum& frustum, DebugDrawer& dd, Camera& camera, Window& window);

	void intersect(Frustum& frustum, DebugDrawer& dd, glm::vec3 camera_pos);

	void intersect(Frustum& frustum, DebugDrawer& dd, AabbXZ aabb, uint level, uint x, uint y);

	int intersect_triangle(glm::vec3 r_o, glm::vec3 r_d, glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, float* t);
}