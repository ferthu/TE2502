#pragma once

#include "algorithm/common.hpp"

namespace process
{
	bool clip(vec4 p);
	void triangle_process(TerrainBuffer* tb, Quadtree& quadtree, float* log_filter, mat4 vp, vec4 camera_position, vec2 screen_size, float threshold, float area_multiplier, float curvature_multiplier, cuint node_index);
}