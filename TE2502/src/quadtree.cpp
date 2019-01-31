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

Quadtree::Quadtree(float node_side_length, uint32_t levels) : m_node_side_length(node_side_length), m_levels(levels)
{
}

void Quadtree::frustum_cull(Frustum& frustum, DebugDrawer& dd)
{
	intersect(frustum, dd, { {-m_node_side_length * (float)(m_levels * m_levels), -m_node_side_length * (float)(m_levels * m_levels)},
		{m_node_side_length * (float)(m_levels * m_levels), m_node_side_length * (float)(m_levels * m_levels)} }, 0);
}

void Quadtree::move_from(Quadtree&& other)
{
	destroy();

	m_node_side_length = other.m_node_side_length;
	m_levels = other.m_levels;
}

void Quadtree::destroy()
{
}

void Quadtree::intersect(Frustum& frustum, DebugDrawer& dd, AabbXZ aabb, uint32_t level)
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

		return;
	}

	// This node is visible, check children
	if (frustum_aabbxz_intersection(frustum, aabb))
	{
		glm::vec2 mid = (aabb.m_min + aabb.m_max) * 0.5f;
		float mid_x = (aabb.m_min.x + aabb.m_max.x) * 0.5f;
		float mid_z = (aabb.m_min.y + aabb.m_max.y) * 0.5f;

		intersect(frustum, dd, { {aabb.m_min.x, aabb.m_min.y}, {mid.x, mid.y} }, level + 1);
		intersect(frustum, dd, { {aabb.m_min.x, mid_z}, {mid.x, aabb.m_max.y} }, level + 1);
		intersect(frustum, dd, { {mid_x, aabb.m_min.y}, {aabb.m_max.x, mid_z} }, level + 1);
		intersect(frustum, dd, { {mid.x, mid.y}, {aabb.m_max.x, aabb.m_max.y} }, level + 1);
	}
}
