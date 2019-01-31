#pragma once

#include <vulkan/vulkan.h>

#include "graphics/debug_drawer.hpp"
#include "math/geometry.hpp"
#include "graphics/gpu_memory.hpp"
#include "graphics/gpu_buffer.hpp"

class Quadtree
{
public:
	Quadtree() {}
	~Quadtree();

	Quadtree(Quadtree&& other);
	Quadtree& operator=(Quadtree&& other);

	Quadtree(float total_side_length, uint32_t levels, uint32_t max_nodes, uint32_t max_node_indices, uint32_t max_node_vertices);

	void frustum_cull(Frustum& frustum, DebugDrawer& dd);

private:
	// Move other into this
	void move_from(Quadtree&& other);

	// Destroys object
	void destroy();

	// Recursive intersection
	void intersect(Frustum& frustum, DebugDrawer& dd, AabbXZ aabb, uint32_t level, uint32_t x, uint32_t y);

	// Finds a free chunk in m_buffer and returns it index, or INVALID if none was found
	uint32_t find_chunk();

	// For a node at the given position, return its index into m_buffer
	uint32_t get_offset(uint32_t node_x, uint32_t node_z);

	// GPU memory used for backing buffer
	GPUMemory m_memory;

	// Contains terrain indices + vertices for quadtree nodes
	GPUBuffer m_buffer;

	// Max number of indices and vertices per node
	uint32_t m_max_indices;
	uint32_t m_max_vertices;

	// Max number of active nodes
	uint32_t m_max_nodes;

	// Size and number of levels in quadtree
	float m_total_side_length;
	uint32_t m_levels;

	// For every possible node, store an index into m_buffer. The chunk of m_buffer pointed to contains mesh data for that node
	uint32_t* m_node_index_to_buffer_index;

	// For chunk i of m_buffer, m_buffer_index_filled[i] is true if that chunk is used by a node
	bool* m_buffer_index_filled;

	const uint32_t INVALID = ~0u;
};

