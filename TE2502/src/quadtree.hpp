#pragma once

#include "graphics/debug_drawer.hpp"
#include "math/geometry.hpp"

class Quadtree
{
public:
	Quadtree() {}
	~Quadtree();

	Quadtree(Quadtree&& other);
	Quadtree& operator=(Quadtree&& other);

	Quadtree(float node_side_length, uint32_t levels);

	void frustum_cull(Frustum& frustum, DebugDrawer& dd);

private:
	// Move other into this
	void move_from(Quadtree&& other);

	// Destroys object
	void destroy();

	// Test
	void intersect(Frustum& frustum, DebugDrawer& dd, AabbXZ aabb, uint32_t level);

	float m_node_side_length;
	uint32_t m_levels;
};

