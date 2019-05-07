#pragma once

#include "graphics/debug_drawer.hpp"
#include "tfile.hpp"
#include "camera.hpp"

#include <mutex>

typedef uint32_t uint;
using namespace glm;

namespace cputri
{
	extern std::vector<std::vector<glm::vec3>> debug_lines;

	struct TriData
	{
		DebugDrawer* dd;

		float mc_fov;
		vec3 mc_pos;
		mat4 mc_view;
		mat4 mc_vp;
		Frustum mc_frustum;

		float cc_fov;
		vec3 cc_pos;
		mat4 cc_view;
		mat4 cc_vp;
		Frustum cc_frustum;

		vec2 mouse_pos;
		vec2 window_size;

		bool triangulate;

		bool show_debug;
		bool show_hovered;
		int show_node;
		int refine_node;
		// Max new vertices per refine step
		int refine_vertices;
		int sideshow_bob;
		float area_mult;
		float curv_mult;
		float threshold;

		// DEBUG
		int debug_node;
		int debug_stage;

		std::mutex* debug_draw_mutex;
	};

	void setup(TFile& tfile);

	void destroy(VulkanContext& context, GPUBuffer& cpu_buffer);

	void run(TriData* tri_data);

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

	void triangulate(cputri::TriData* tri_data);

	void clear_terrain();

	void process_triangles(TriData* tri_data);

	void draw_terrain(TriData* tri_data);

	void intersect(Frustum& frustum, DebugDrawer& dd, glm::vec3 camera_pos, TriData* tri_data);

	void intersect(Frustum& frustum, DebugDrawer& dd, AabbXZ aabb, uint level, uint x, uint y);

	int intersect_triangle(glm::vec3 r_o, glm::vec3 r_d, glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, float* t);

	// Save current terrain state
	void backup();

	// Restore current terrain state
	void restore();

	// Returns vector of messages indicating hovered triangles
	std::vector<std::string> get_hovered_tris();

	void worker_thread();
}