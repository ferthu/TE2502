#include "quadtree.hpp"

Quadtree::~Quadtree()
{
	destroy();
}

Quadtree::Quadtree(Quadtree&& other)
{
	move_from(std::move(other));
}

Quadtree& Quadtree::operator=(Quadtree&& other)
{
	if (this != &other)
		move_from(std::move(other));

	return *this;
}

Quadtree::Quadtree(float total_side_length, uint32_t levels, uint32_t max_nodes, uint32_t max_node_indices, uint32_t max_node_vertices) 
	: m_total_side_length(total_side_length), m_levels(levels), m_max_nodes(max_nodes), m_max_indices(max_node_indices), m_max_vertices(max_node_vertices)
{
	assert(levels > 0);

	// (1 << levels) is number of nodes per axis
	m_node_index_to_buffer_index = new uint32_t[(1 << levels) * (1 << levels)];
	memset(m_node_index_to_buffer_index, INVALID, (1 << levels) * (1 << levels) * sizeof(uint32_t));

	m_buffer_index_filled = new bool[max_nodes];
	memset(m_buffer_index_filled, 0, max_nodes * sizeof(bool));
}

void Quadtree::frustum_cull(Frustum& frustum, DebugDrawer& dd)
{
	float half_length = m_total_side_length * 0.5f;

	intersect(frustum, dd, { {-half_length, -half_length},
		{half_length, half_length} }, 0, 0, 0);
}

void Quadtree::move_from(Quadtree&& other)
{
	destroy();

	m_memory = std::move(other.m_memory);
	m_buffer = std::move(other.m_buffer);

	m_max_indices = other.m_max_indices;
	m_max_vertices = other.m_max_vertices;

	m_max_nodes = other.m_max_nodes;

	m_total_side_length = other.m_total_side_length;
	m_levels = other.m_levels;

	m_node_index_to_buffer_index = other.m_node_index_to_buffer_index;
	other.m_node_index_to_buffer_index = nullptr;

	m_buffer_index_filled = other.m_buffer_index_filled;
	other.m_buffer_index_filled = nullptr;
}

void Quadtree::destroy()
{
	delete[] m_node_index_to_buffer_index;
	delete[] m_buffer_index_filled;
}

void Quadtree::intersect(Frustum& frustum, DebugDrawer& dd, AabbXZ aabb, uint32_t level, uint32_t x, uint32_t y)
{
	if (level == m_levels)
	{
		float minx = aabb.m_min.x + (aabb.m_max.x - aabb.m_min.x) * 0.05f;
		float maxx = aabb.m_min.x + (aabb.m_max.x - aabb.m_min.x) * 0.95f;

		float minz = aabb.m_min.y + (aabb.m_max.y - aabb.m_min.y) * 0.05f;
		float maxz = aabb.m_min.y + (aabb.m_max.y - aabb.m_min.y) * 0.95f;

		dd.draw_line({ minx, 0, minz }, { minx, 0, maxz }, { 1, 1, 0 });
		dd.draw_line({ minx, 0, minz }, { maxx, 0, minz }, { 1, 1, 0 });

		dd.draw_line({ maxx, 0, maxz }, { maxx, 0, minz }, { 1, 1, 0 });
		dd.draw_line({ maxx, 0, maxz }, { minx, 0, maxz }, { 1, 1, 0 });

		// Index into m_node_index_to_buffer_index
		uint32_t index = (1 << m_levels) * y + x;
		if (m_node_index_to_buffer_index[index] == INVALID)
		{
			// Visible node does not have data

			uint32_t new_index = find_chunk();
			if (new_index != INVALID)
			{
				m_buffer_index_filled[new_index] = true;
				m_node_index_to_buffer_index[index] = new_index;

				// m_buffer[new_index] needs to be filled with data
				// ...
			}
			else
			{
				// No space left! Ignore for now...
			}
		}
		else
		{
			// Visible node has data, draw it

			// m_buffer[m_node_index_to_buffer_index[index]] needs to be drawn
		}

		return;
	}

	// This node is visible, check children
	if (1 || frustum_aabbxz_intersection(frustum, aabb))
	{
		glm::vec2 mid = (aabb.m_min + aabb.m_max) * 0.5f;
		float mid_x = (aabb.m_min.x + aabb.m_max.x) * 0.5f;
		float mid_z = (aabb.m_min.y + aabb.m_max.y) * 0.5f;

		intersect(frustum, dd, { {aabb.m_min.x, aabb.m_min.y}, {mid.x, mid.y} }, level + 1, (x << 1)    , (y << 1)    );
		intersect(frustum, dd, { {aabb.m_min.x, mid_z}, {mid.x, aabb.m_max.y} }, level + 1, (x << 1)    , (y << 1) + 1);
		intersect(frustum, dd, { {mid_x, aabb.m_min.y}, {aabb.m_max.x, mid_z} }, level + 1, (x << 1) + 1, (y << 1)    );
		intersect(frustum, dd, { {mid.x, mid.y}, {aabb.m_max.x, aabb.m_max.y} }, level + 1, (x << 1) + 1, (y << 1) + 1);
	}
}

uint32_t Quadtree::find_chunk()
{
	for (uint32_t i = 0; i < m_max_nodes; i++)
	{
		if (!m_buffer_index_filled[i])
			return i;
	}

	return INVALID;
}

uint32_t Quadtree::get_offset(uint32_t node_x, uint32_t node_z)
{
	assert(node_x >= 0 && node_x < (1 << m_levels));
	assert(node_z >= 0 && node_z < (1 << m_levels));

	return m_node_index_to_buffer_index[node_x + (1 << m_levels) * node_z];
}
