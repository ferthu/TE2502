#include "algorithm/process.hpp"

namespace process
{
	bool clip(vec4 p)
	{
		return (abs(p.x) <= p.w &&
			abs(p.y) <= p.w &&
			abs(p.z) <= p.w);
	}

	void triangle_process(TerrainBuffer* tb, Quadtree& quadtree, float* log_filter, mat4 vp, vec4 camera_position, float threshold, float area_multiplier, float curvature_multiplier, cuint node_index, cputri::TriData* tri_data)
	{
		if (tri_data->refine_node != -1 && tri_data->refine_node != node_index)
			return;

		if (tb->data[node_index].new_points_count != 0)
			return;

		const vec2 node_min = tb->data[node_index].min;
		const vec2 node_max = tb->data[node_index].max;
		const float side = node_max.x - node_min.x;

		const int cx = int((node_min.x - tb->quadtree_min.x + 1) / side);  // current node x
		const int cy = int((node_min.y - tb->quadtree_min.y + 1) / side);  // current node z/y

		const uint nodes_per_side = 1 << quadtree_levels;

		// Check self and neighbour nodes
		for (int y = -1; y <= 1; ++y)
		{
			for (int x = -1; x <= 1; ++x)
			{
				const int nx = cx + x;
				const int ny = cy + y;
				if (nx >= 0 && nx < (int)nodes_per_side && ny >= 0 && ny < (int)nodes_per_side)
				{
					const uint neighbour_index = quadtree.node_index_to_buffer_index[ny * nodes_per_side + nx];
					if (neighbour_index == INVALID)
					{
						return;
					}
				}
				else
				{
					return;
				}
			}
		}

		const uint index_count = tb->data[node_index].index_count;

		uint new_point_count = 0;

		// For every triangle
		for (uint i = 0; i + 3 <= index_count && new_point_count < TRIANGULATE_MAX_NEW_POINTS; i += 3)
		{
			// Get vertices
			vec4 v0 = tb->data[node_index].positions[tb->data[node_index].indices[i]];
			vec4 v1 = tb->data[node_index].positions[tb->data[node_index].indices[i + 1]];
			vec4 v2 = tb->data[node_index].positions[tb->data[node_index].indices[i + 2]];

			// Get clipspace coordinates
			vec4 c0 = vp * glm::vec4(glm::vec3(v0), 1.0f);
			vec4 c1 = vp * glm::vec4(glm::vec3(v1), 1.0f);
			vec4 c2 = vp * glm::vec4(glm::vec3(v2), 1.0f);

			// Check if any vertex is visible (simple clipping)
			if (clip(c0) || clip(c1) || clip(c2))
			{
				// Calculate screen space area

				c0 /= c0.w;
				c1 /= c1.w;
				c2 /= c2.w;

				// a, b, c is triangle side lengths
				float a = distance(vec2(c0.x, c0.y), vec2(c1.x, c1.y));
				float b = distance(vec2(c0.x, c0.y), vec2(c2.x, c2.y));
				float c = distance(vec2(c1.x, c1.y), vec2(c2.x, c2.y));

				// s is semiperimeter
				float s = (a + b + c) * 0.5f;

				float area = pow(s * (s - a) * (s - b) * (s - c), area_multiplier);

				glm::vec3 mid = (glm::vec3(v0) + glm::vec3(v1) + glm::vec3(v2)) / 3.0f;
				float curv0 = v0.w;		// Curvature is stored in w coordinate
				float curv1 = v1.w;
				float curv2 = v2.w;

				float inv_total_curv = 1.0f / (curv0 + curv1 + curv2);

				// Create linear combination of corners based on curvature
				glm::vec3 curv_point = (curv0 * inv_total_curv * glm::vec3(v0)) + (curv1 * inv_total_curv * glm::vec3(v1)) + (curv2 * inv_total_curv * glm::vec3(v2));

				// Linearly interpolate between triangle middle and curv_point
				glm::vec3 new_pos = mix(mid, curv_point, 0.5f);

				// Y position of potential new point
				float terrain_y = -terrain(glm::vec2(new_pos.x, new_pos.z)) - 0.5f;

				// Transform terrain_y and curv_point to clip space
				glm::vec4 clip_terrain_y = vp * glm::vec4(new_pos.x, terrain_y, new_pos.z, 1.0f);
				glm::vec4 clip_curv_point = vp * glm::vec4(curv_point, 1.0f);
				clip_terrain_y /= clip_terrain_y.w;
				clip_curv_point /= clip_curv_point.w;

				// Screen space distance between current triangle point and new point
				float screen_space_dist = pow(distance(glm::vec2(clip_terrain_y.x, clip_terrain_y.y), glm::vec2(clip_curv_point.x, clip_curv_point.y)), curvature_multiplier);

				// A new point should be added
				if (screen_space_dist * area >= threshold)
				{
					const glm::vec4 point = glm::vec4(new_pos.x, terrain_y, new_pos.z, curvature(vec3(new_pos.x, terrain_y, new_pos.z), log_filter));
					tb->data[node_index].new_points[new_point_count] = point;
					tb->data[node_index].new_points_triangles[new_point_count] = i / 3;
					tb->data[node_index].new_points_count++;
					++new_point_count;
				}
			}
		}
	}
}
