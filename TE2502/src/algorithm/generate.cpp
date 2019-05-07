#include "algorithm/generate.hpp"
#include <vector>
#include <iostream>

namespace generate
{
	void remove_old_triangles(TerrainBuffer* tb, GlobalData& g, cuint node_index)
	{
		TerrainData& node = tb->data[node_index];

		// Remove old triangles
		for (int j = int(g.triangles_removed) - 1; j >= 0; --j)
		{
			const uint index = g.triangles_to_remove[j];

			const uint last_triangle = node.index_count / 3 - 1;

			// Loop through remaining triangles to remove and update any that are equal to last_triangle
			for (int ii = 0; ii < j; ++ii)
			{
				if (g.triangles_to_remove[ii] == last_triangle)
				{
					g.triangles_to_remove[ii] = index;
					break;
				}
			}

			for (uint ii = 0; ii < 3; ++ii)
			{
				replace_connection_index(tb, node_index, node.triangle_connections[last_triangle * 3 + ii], last_triangle, index);
			}

			// Remove triangle
			if (index < last_triangle)
			{
				node.indices[index * 3 + 0] = node.indices[last_triangle * 3 + 0];
				node.indices[index * 3 + 1] = node.indices[last_triangle * 3 + 1];
				node.indices[index * 3 + 2] = node.indices[last_triangle * 3 + 2];
				node.triangles[index].circumcentre = node.triangles[last_triangle].circumcentre;
				node.triangles[index].circumradius2 = node.triangles[last_triangle].circumradius2;
				node.triangle_connections[index * 3 + 0] = node.triangle_connections[last_triangle * 3 + 0];
				node.triangle_connections[index * 3 + 1] = node.triangle_connections[last_triangle * 3 + 1];
				node.triangle_connections[index * 3 + 2] = node.triangle_connections[last_triangle * 3 + 2];

				for (uint tt = 0; tt < g.new_triangle_index_count; ++tt)
				{
					const uint triangle_index = g.new_triangle_indices[tt];
					if (triangle_index == last_triangle)
						g.new_triangle_indices[tt] = index;
				}
			}

			node.index_count -= 3;

			// Update the rest of the new points' triangle index after updating triangles in node
			for (uint ii = 0; ii < node.new_points_count; ++ii)
			{
				if (node.new_points_triangles[ii] == index)
				{
					// Look through all newly added triangles only
					for (uint tt = 0; tt < g.new_triangle_index_count; ++tt)
					{
						const uint triangle_index = g.new_triangle_indices[tt];
						const vec4 new_point = node.new_points[ii];
						const vec2 circumcentre = node.triangles[triangle_index].circumcentre;
						const float circumradius2 = node.triangles[triangle_index].circumradius2;

						const float dx = new_point.x - circumcentre.x;
						const float dy = new_point.z - circumcentre.y;

						// Find the first triangle whose cc contains the point
						if (dx * dx + dy * dy < circumradius2)
						{
							node.new_points_triangles[ii] = triangle_index;
							break;
						}
					}
				}
				else if (node.new_points_triangles[ii] == last_triangle)
				{
					node.new_points_triangles[ii] = index;
				}
			}
		}
	}

	void add_connection(GlobalData& g, cuint connection_index)
	{
		// Check if it has already been seen
		for (uint ii = 0; ii < g.seen_triangle_count; ++ii)
		{
			if (connection_index == g.seen_triangles[ii])
			{
				return;
			}
		}

		g.seen_triangles[g.seen_triangle_count] = connection_index;
		++g.seen_triangle_count;
		g.triangles_to_test[g.test_count] = connection_index;
		++g.test_count;
	}

	void generate_triangulate_shader(TerrainBuffer* tb, GlobalData& g, cuint node_index)
	{
		TerrainData& node = tb->data[node_index];

		const vec2 node_min = node.min;
		const vec2 node_max = node.max;
		const float side = node_max.x - node_min.x;

		const int nodes_per_side = 1 << quadtree_levels;

		const int cx = int((node_min.x - tb->quadtree_min.x + 1) / side);  // current node x
		const int cy = int((node_min.y - tb->quadtree_min.y + 1) / side);  // current node z/y

		const uint new_points_count = node.new_points_count;

		g.triangles_removed = 0;

		for (uint n = 0; n < new_points_count && n < TERRAIN_GENERATE_NUM_VERTICES; ++n)
		{
			const vec4 current_point = node.new_points[n];

			g.seen_triangle_count = 1;
			g.test_count = 1;

			const uint start_index = node.new_points_triangles[n];
			g.seen_triangles[0] = start_index;
			g.triangles_to_test[0] = start_index;
			g.new_triangle_index_count = 0;

			bool finish = false;
			while (g.test_count != 0 && !finish)
			{
				const uint triangle_index = g.triangles_to_test[--g.test_count];
				const vec2 circumcentre = node.triangles[triangle_index].circumcentre;
				const float circumradius2 = node.triangles[triangle_index].circumradius2;

				const float dx = current_point.x - circumcentre.x;
				const float dy = current_point.z - circumcentre.y;

				if (dx * dx + dy * dy < circumradius2)
				{
					// Add triangle edges to edge buffer
					const uint index0 = node.indices[triangle_index * 3 + 0];
					const uint index1 = node.indices[triangle_index * 3 + 1];
					const uint index2 = node.indices[triangle_index * 3 + 2];
					const vec4 p0 = vec4(vec3(node.positions[index0]), 1.0f);
					const vec4 p1 = vec4(vec3(node.positions[index1]), 1.0f);
					const vec4 p2 = vec4(vec3(node.positions[index2]), 1.0f);

					// Store edges to be removed
					uint tr = g.triangles_removed++;
					if (tr >= max_triangles_to_remove || tr >= max_border_edges)
					{
						finish = true;
						break;
					}

					uint ec = tr * 3;
					// Edge 0
					bool biggest_point = p0.y < p1.y;
					g.generate_edges[ec + 0].p1 = biggest_point ? p0 : p1;
					g.generate_edges[ec + 0].p2 = !biggest_point ? p0 : p1;
					g.generate_edges[ec + 0].p1_index = biggest_point ? index0 : index1;
					g.generate_edges[ec + 0].p2_index = !biggest_point ? index0 : index1;
					g.generate_edges[ec + 0].connection = node.triangle_connections[triangle_index * 3 + 0];
					g.generate_edges[ec + 0].old_triangle_index = triangle_index;
					// Edge 1
					biggest_point = p1.y < p2.y;
					g.generate_edges[ec + 1].p1 = biggest_point ? p1 : p2;
					g.generate_edges[ec + 1].p2 = !biggest_point ? p1 : p2;
					g.generate_edges[ec + 1].p1_index = biggest_point ? index1 : index2;
					g.generate_edges[ec + 1].p2_index = !biggest_point ? index1 : index2;
					g.generate_edges[ec + 1].connection = node.triangle_connections[triangle_index * 3 + 1];
					g.generate_edges[ec + 1].old_triangle_index = triangle_index;
					// Edge 2
					biggest_point = p2.y < p0.y;
					g.generate_edges[ec + 2].p1 = biggest_point ? p2 : p0;
					g.generate_edges[ec + 2].p2 = !biggest_point ? p2 : p0;
					g.generate_edges[ec + 2].p1_index = biggest_point ? index2 : index0;
					g.generate_edges[ec + 2].p2_index = !biggest_point ? index2 : index0;
					g.generate_edges[ec + 2].connection = node.triangle_connections[triangle_index * 3 + 2];
					g.generate_edges[ec + 2].old_triangle_index = triangle_index;

					// Mark the triangle to be removed later
					g.triangles_to_remove[tr] = triangle_index;

					// Add neighbour triangles to be tested
					for (uint ss = 0; ss < 3 && !finish; ++ss)
					{
						const uint index = node.triangle_connections[triangle_index * 3 + ss];

						if (index < INVALID - 9)
						{
							if (g.seen_triangle_count >= test_triangle_buffer_size || g.test_count >= test_triangle_buffer_size)
							{
								finish = true;
								break;
							}

							add_connection(g, index);
						}
					}
				}
			}

			if (finish)
			{
				g.triangles_removed = 0;
				continue;
			}

			// Delete all doubly specified edges from edge buffer (this leaves the edges of the enclosing polygon only)
			const uint edge_count = g.triangles_removed * 3;
			for (uint i = 0; i < edge_count; ++i)
			{
				bool found = false;
				for (uint j = 0; j < edge_count; ++j)
				{
					if (i != j &&
						g.generate_edges[i].p1 == g.generate_edges[j].p1 &&
						g.generate_edges[i].p2 == g.generate_edges[j].p2)
					{
						// Mark as invalid
						g.generate_edges[j].p1.w = -1;
						found = true;
					}
				}
				if (found)
					g.generate_edges[i].p1.w = -1;
			}

			// Count the number of new triangles to create
			g.new_triangle_count = 0;
			for (uint j = 0; j < edge_count && j < max_triangles_to_remove * 3; ++j)
			{
				if (g.generate_edges[j].p1.w > -0.5)
				{
					g.generate_edges[j].future_index = node.index_count / 3 + g.new_triangle_count;
					g.valid_indices[g.new_triangle_count++] = j;
				}
			}

			// Add to the triangle list all triangles formed between the point and the edges of the enclosing polygon
			for (uint ii = 0; ii < g.new_triangle_count; ++ii)
			{
				const uint i = g.valid_indices[ii];
				const vec3 P = vec3(g.generate_edges[i].p1);
				const vec3 Q = vec3(g.generate_edges[i].p2);
				const vec3 R = vec3(current_point);

				// Make sure winding order is correct
				const vec3 nor = cross(R - P, Q - P);
				if (nor.y > 0)
				{
					std::swap(g.generate_edges[i].p1, g.generate_edges[i].p2);
					std::swap(g.generate_edges[i].p1_index, g.generate_edges[i].p2_index);
				}

				// Set indices for the new triangle
				const uint index_count = node.index_count;
				node.indices[index_count + 0] = g.generate_edges[i].p1_index;
				node.indices[index_count + 1] = g.generate_edges[i].p2_index;
				node.indices[index_count + 2] = node.vertex_count;

				const uint triangle_count = index_count / 3;
				g.new_triangle_indices[g.new_triangle_index_count++] = triangle_count;

				// Set circumcircles for the new triangle
				const float a = distance(vec2(P.x, P.z), vec2(Q.x, Q.z));
				const float b = distance(vec2(P.x, P.z), vec2(R.x, R.z));
				const float c = distance(vec2(R.x, R.z), vec2(Q.x, Q.z));

				const vec2 cc_center = find_circum_center(vec2(P.x, P.z), vec2(Q.x, Q.z), vec2(R.x, R.z));
				const float cc_radius2 = find_circum_radius_squared(a, b, c);

				node.triangles[triangle_count].circumcentre = cc_center;
				node.triangles[triangle_count].circumradius2 = cc_radius2;

				// Connections
				node.triangle_connections[index_count + 0] = g.generate_edges[i].connection;
				const vec4 edges[2] = { g.generate_edges[i].p1, g.generate_edges[i].p2 };
				bool already_added = false;
				for (uint ss = 0; ss < 2; ++ss)  // The two other sides
				{
					bool found = false;
					// Search through all other new triangles that have been added to find possible neighbours/connections
					for (uint ee = 0; ee < g.new_triangle_count && !found; ++ee)
					{
						uint test_index = g.valid_indices[ee];
						if (test_index == i)
							continue;
						// Check each pair of points in the triangle if they match
						if (edges[ss] == g.generate_edges[test_index].p1)
						{
							node.triangle_connections[index_count + 2 - ss] = g.generate_edges[test_index].future_index;
							found = true;
						}
						else if (edges[ss] == g.generate_edges[test_index].p2)
						{
							node.triangle_connections[index_count + 2 - ss] = g.generate_edges[test_index].future_index;
							found = true;
						}
					}
					if (!found)
					{
						node.triangle_connections[index_count + 2 - ss] = INVALID;
						if (!already_added && node.border_count < MAX_BORDER_TRIANGLE_COUNT)
						{
							already_added = true;
							node.border_triangle_indices[node.border_count++] = g.generate_edges[i].future_index;
						}
					}
				}

				replace_connection_index(tb, node_index, g.generate_edges[i].connection, g.generate_edges[i].old_triangle_index, g.generate_edges[i].future_index);

				node.index_count += 3;
			}

			remove_old_triangles(tb, g, node_index);

			// Insert new point
			node.positions[node.vertex_count++] = current_point;

			g.triangles_removed = 0;
		}

		node.new_points_count = 0;
	}

	void add_border_point(TerrainBuffer* tb, uint self_node_index, uint other_node_index, vec4 point, bool HARDCORE_DEBUG)
	{
		uint count = tb->data[self_node_index].new_points_count;
		for (uint np = 0; np < count; ++np)
		{
			if (tb->data[self_node_index].new_points[np] == point)
				return;
		}

		if (HARDCORE_DEBUG)	// Dynamic points
		{
			cputri::debug_lines.back().push_back(vec3(point) + vec3{ 0, -103, 0 });
			cputri::debug_lines.back().push_back(vec3(point) + vec3{ 0, -2000, 0 });
			cputri::debug_lines.back().push_back(vec3(0.0f, 1.0f, 0.5f));
		}

		tb->data[self_node_index].new_points[tb->data[self_node_index].new_points_count] = point;
		tb->data[self_node_index].new_points_triangles[tb->data[self_node_index].new_points_count++] = 0;
	}

	void remove_marked_triangles(TerrainBuffer* tb, GlobalData& g, uint node_index)
	{
		TerrainData& node = tb->data[node_index];
		// Remove the outer triangles/supertriangles
		for (int j = int(g.triangles_removed) - 1; j >= 0; --j)
		{
			const uint index = g.triangles_to_remove[j];

			const uint last_triangle = node.index_count / 3 - 1;

			// Loop through remaining triangles to remove and update any that are equal to last_triangle
			for (int ii = 0; ii < j; ++ii)
			{
				if (g.triangles_to_remove[ii] == last_triangle)
				{
					g.triangles_to_remove[ii] = index;
					break;
				}
			}

			// Go through all valid connected triangles
			for (uint ii = 0; ii < 3; ++ii)
			{
				const uint triangle_to_check = node.triangle_connections[index * 3 + ii];
				if (triangle_to_check < INVALID - 9)
				{
					// Find the side that points back to the current triangle
					for (uint tt = 0; tt < 3; ++tt)
					{
						const uint triangle_index = node.triangle_connections[triangle_to_check * 3 + tt];
						if (triangle_index == index)
						{
							// Mark that side connection as INVALID since that triangle is being removed
							node.triangle_connections[triangle_to_check * 3 + tt] = UNKNOWN;
						}
					}
				}
			}
			for (uint ii = 0; ii < 3; ++ii)
			{
				replace_connection_index(tb, node_index, node.triangle_connections[last_triangle * 3 + ii], last_triangle, index);
			}

			// Remove triangle
			if (index < last_triangle)
			{
				node.indices[index * 3 + 0] = node.indices[last_triangle * 3 + 0];
				node.indices[index * 3 + 1] = node.indices[last_triangle * 3 + 1];
				node.indices[index * 3 + 2] = node.indices[last_triangle * 3 + 2];
				node.triangles[index].circumcentre = node.triangles[last_triangle].circumcentre;
				node.triangles[index].circumradius2 = node.triangles[last_triangle].circumradius2;
				node.triangle_connections[index * 3 + 0] = node.triangle_connections[last_triangle * 3 + 0];
				node.triangle_connections[index * 3 + 1] = node.triangle_connections[last_triangle * 3 + 1];
				node.triangle_connections[index * 3 + 2] = node.triangle_connections[last_triangle * 3 + 2];

				for (int ii = 0; ii < j; ++ii)
				{
					if (g.triangles_to_remove[ii] == last_triangle)
						g.triangles_to_remove[ii] = index;
				}
			}

			node.index_count -= 3;
		}
	}

	void remove_marked_triangles2(TerrainBuffer* tb, GlobalData& g, uint node_index)
	{
		TerrainData& node = tb->data[node_index];

		// Remove the outer triangles/supertriangles
		for (int j = int(g.triangles_removed) - 1; j >= 0; --j)
		{
			const uint index = g.triangles_to_remove[j];

			const uint last_triangle = node.index_count / 3 - 1;

			// Loop through remaining triangles to remove and update any that are equal to last_triangle
			for (int ii = 0; ii < j; ++ii)
			{
				if (g.triangles_to_remove[ii] == last_triangle)
				{
					g.triangles_to_remove[ii] = index;
					break;
				}
			}

			for (uint ii = 0; ii < 3; ++ii)
			{
				replace_connection_index(tb, node_index, node.triangle_connections[last_triangle * 3 + ii], last_triangle, index);
			}

			// Remove triangle
			if (index < last_triangle)
			{
				node.indices[index * 3 + 0] = node.indices[last_triangle * 3 + 0];
				node.indices[index * 3 + 1] = node.indices[last_triangle * 3 + 1];
				node.indices[index * 3 + 2] = node.indices[last_triangle * 3 + 2];
				node.triangles[index].circumcentre = node.triangles[last_triangle].circumcentre;
				node.triangles[index].circumradius2 = node.triangles[last_triangle].circumradius2;
				node.triangle_connections[index * 3 + 0] = node.triangle_connections[last_triangle * 3 + 0];
				node.triangle_connections[index * 3 + 1] = node.triangle_connections[last_triangle * 3 + 1];
				node.triangle_connections[index * 3 + 2] = node.triangle_connections[last_triangle * 3 + 2];

				for (int ii = 0; ii < j; ++ii)
				{
					if (g.triangles_to_remove[ii] == last_triangle)
						g.triangles_to_remove[ii] = index;
				}
			}

			node.index_count -= 3;
		}
	}

	int sign(float v)
	{
		if (v < 0)
			return -1;
		return 1;
	}

	void calcLine(vec4 v0, vec4 v1, float& a, float& b, float& c)
	{
		a = v0.z - v1.z;
		b = v1.x - v0.x;
		c = -0.5f*(a*(v0.x + v1.x) + b * (v0.z + v1.z));
	}

	bool is_same_edge(TerrainBuffer* tb, vec4 e1p1, vec4 e1p2, vec3 test_middle, vec4 e2p1, vec4 e2p2, vec3 neighbour_middle, uint neighbour_node_index, uint neighbour_border_index, uint connection_value, bool& valid)
	{
		if (((e1p1 == e2p1 && e1p2 == e2p2) ||
			(e1p2 == e2p1 && e1p1 == e2p2)))
		{
			if (tb->data[neighbour_node_index].triangle_connections[neighbour_border_index] == connection_value ||
				tb->data[neighbour_node_index].triangle_connections[neighbour_border_index] == UNKNOWN)
			{
				// Update connection value (required if it is = UNKNOWN)
				tb->data[neighbour_node_index].triangle_connections[neighbour_border_index] = connection_value;
			}
			/*else
			{
				valid = false;
			}*/

			float a, b, c;
			calcLine(e1p1, e1p2, a, b, c);
			if (sign(a * test_middle.x + b * test_middle.z + c) != sign(a * neighbour_middle.x + b * neighbour_middle.z + c))
			{
				return true;
			}
		}

		return false;
	}

	bool neighbour_exists(uint cx, uint cy, uint local_neighbour_index, TerrainBuffer* tb)
	{
		int nx = int(cx) + ((local_neighbour_index % 3) - 1);
		int ny = int(cy) + ((local_neighbour_index / 3) - 1);

		if (nx >= 0 && nx < (1 << quadtree_levels) &&
			ny >= 0 && ny < (1 << quadtree_levels))
		{
			if (tb->quadtree_index_map[ny * (1 << quadtree_levels) + nx] != INVALID &&
				!tb->data[tb->quadtree_index_map[ny * (1 << quadtree_levels) + nx]].is_invalid)
				return true;
		}

		return false;
	}


	void generate(TerrainBuffer* tb, GlobalData& g, float* log_filter, uint node_index, vec2 min, vec2 max, cputri::TriData* tri_data)
	{
		bool HARDCORE_DEBUG = false;
		if (tri_data->debug_node == node_index)
		{
			cputri::debug_lines.clear();
			HARDCORE_DEBUG = true;
		}

		const uint GRID_SIDE = TERRAIN_GENERATE_GRID_SIDE;

		TerrainData& node = tb->data[node_index];

		const vec2 node_min = min;
		const vec2 node_max = max;
		const float side = node_max.x - node_min.x;

		{
			node.index_count = 6;
			node.is_invalid = false;

			node.vertex_count = 4;
			node.new_points_count = GRID_SIDE * GRID_SIDE;

			node.min = min;
			node.max = max;

			node.border_count = 0;

			float temp = side * 1.5f;
			vec4 p0 = vec4(node_min.x - temp, 1, node_min.y - temp, 1);
			vec4 p1 = vec4(node_max.x + temp, 2, node_min.y - temp, 1);
			vec4 p2 = vec4(node_max.x + temp, 3, node_max.y + temp, 1);
			vec4 p3 = vec4(node_min.x - temp, 4, node_max.y + temp, 1);

			node.positions[0] = p0;
			node.positions[1] = p1;
			node.positions[2] = p2;
			node.positions[3] = p3;

			node.indices[0] = 0;
			node.indices[1] = 1;
			node.indices[2] = 2;
			node.indices[3] = 2;
			node.indices[4] = 3;
			node.indices[5] = 0;

			const vec2 P = vec2(p0.x, p0.z);
			const vec2 Q = vec2(p1.x, p1.z);
			const vec2 R = vec2(p2.x, p2.z);
			const vec2 S = vec2(p3.x, p3.z);
			node.triangles[0].circumcentre = find_circum_center(P, Q, R);
			node.triangles[0].circumradius2 = find_circum_radius_squared(P, Q, R);
			node.triangles[1].circumcentre = find_circum_center(R, S, P);
			node.triangles[1].circumradius2 = find_circum_radius_squared(R, S, P);

			node.triangle_connections[0 + 0] = INVALID;
			node.triangle_connections[0 + 1] = INVALID;
			node.triangle_connections[0 + 2] = 1;
			node.triangle_connections[3 + 0] = INVALID;
			node.triangle_connections[3 + 1] = INVALID;
			node.triangle_connections[3 + 2] = 0;

			if (HARDCORE_DEBUG)	// Super triangles
			{
				cputri::debug_lines.push_back(std::vector<vec3>());
				cputri::debug_lines.back().push_back(vec3(p0) + vec3{ 0, -103, 0 });
				cputri::debug_lines.back().push_back(vec3(p1) + vec3{ 0, -103, 0 });
				cputri::debug_lines.back().push_back(vec3(0.0f, 1.0f, 0.5f));

				cputri::debug_lines.back().push_back(vec3(p1) + vec3{ 0, -103, 0 });
				cputri::debug_lines.back().push_back(vec3(p2) + vec3{ 0, -103, 0 });
				cputri::debug_lines.back().push_back(vec3(0.0f, 1.0f, 0.5f));

				cputri::debug_lines.back().push_back(vec3(p2) + vec3{ 0, -103, 0 });
				cputri::debug_lines.back().push_back(vec3(p0) + vec3{ 0, -103, 0 });
				cputri::debug_lines.back().push_back(vec3(0.0f, 1.0f, 0.5f));

				cputri::debug_lines.back().push_back(vec3(p2) + vec3{ 0, -103, 0 });
				cputri::debug_lines.back().push_back(vec3(p3) + vec3{ 0, -103, 0 });
				cputri::debug_lines.back().push_back(vec3(0.0f, 1.0f, 0.5f));

				cputri::debug_lines.back().push_back(vec3(p3) + vec3{ 0, -103, 0 });
				cputri::debug_lines.back().push_back(vec3(p0) + vec3{ 0, -103, 0 });
				cputri::debug_lines.back().push_back(vec3(0.0f, 1.0f, 0.5f));

				cputri::debug_lines.back().push_back(vec3(p0) + vec3{ 0, -103, 0 });
				cputri::debug_lines.back().push_back(vec3(p2) + vec3{ 0, -103, 0 });
				cputri::debug_lines.back().push_back(vec3(0.0f, 1.0f, 0.5f));
			}
		}

		if (HARDCORE_DEBUG)	// Static points
		{
			cputri::debug_lines.push_back(std::vector<vec3>());
		}

		// Generate static positions
		for (uint i = 0; i < GRID_SIDE * GRID_SIDE; ++i)
		{
			float x = min.x + ((i % GRID_SIDE) / float(GRID_SIDE - 1)) * side + (side / GRID_SIDE) * 0.5f * ((i / GRID_SIDE) % 2);
			float z = min.y + float(i / GRID_SIDE) / float(GRID_SIDE - 1) * side;

			if ((i % GRID_SIDE) / float(GRID_SIDE - 1) == 0)
				x = min.x;
			else if ((i % GRID_SIDE) / float(GRID_SIDE - 1) > 0.99f)
				x = max.x;

			node.new_points[i] = vec4(x, -terrain(vec2(x, z)) - 0.5, z, curvature(vec3(x, 0.0f, z), log_filter));
			node.new_points_triangles[i] = 0;

			if (HARDCORE_DEBUG) // Static points
			{
				cputri::debug_lines.back().push_back(vec3(node.new_points[i]) + vec3{ 0, -103, 0 });
				cputri::debug_lines.back().push_back(vec3(node.new_points[i]) + vec3(0, -2000, 0));
				cputri::debug_lines.back().push_back(vec3(0.0f, 1.0f, 0.5f));
			}
		}

		// Triangles

		const int nodes_per_side = 1 << quadtree_levels;

		const int cx = int((node_min.x - tb->quadtree_min.x + 1) / side);  // current node x
		const int cy = int((node_min.y - tb->quadtree_min.y + 1) / side);  // current node z/y

		if (HARDCORE_DEBUG)	// Dynamic points
		{
			cputri::debug_lines.push_back(std::vector<vec3>());
		}

		// Check existing neighbour node border triangles and add border points
		// TODO: Create list of valid nodes instead of checking each time
		for (int y = -1; y <= 1; ++y)
		{
			for (int x = -1; x <= 1; ++x)
			{
				const int nx = cx + x;
				const int ny = cy + y;
				// Check if valid node
				if (nx >= 0 && nx < nodes_per_side && ny >= 0 && ny < nodes_per_side)
				{
					const uint neighbour_index = tb->quadtree_index_map[ny * nodes_per_side + nx];
					if (neighbour_index == INVALID || tb->data[neighbour_index].is_invalid || (x == 0 && y == 0))
						continue;

					const uint triangle_count = tb->data[neighbour_index].border_count;
					// Check each border triangle
					for (uint tt = 0; tt < triangle_count; ++tt)
					{
						const uint triangle_index = tb->data[neighbour_index].border_triangle_indices[tt];
						const vec4 p0 = tb->data[neighbour_index].positions[tb->data[neighbour_index].indices[triangle_index * 3 + 0]];
						const vec4 p1 = tb->data[neighbour_index].positions[tb->data[neighbour_index].indices[triangle_index * 3 + 1]];
						const vec4 p2 = tb->data[neighbour_index].positions[tb->data[neighbour_index].indices[triangle_index * 3 + 2]];

						// Find the side(s) that actually faces the border (and therefore has an INVALID connection)
						if (tb->data[neighbour_index].triangle_connections[triangle_index * 3 + 0] >= INVALID - 9)
						{
							add_border_point(tb, node_index, neighbour_index, p0, HARDCORE_DEBUG);
							add_border_point(tb, node_index, neighbour_index, p1, HARDCORE_DEBUG);
						}
						if (tb->data[neighbour_index].triangle_connections[triangle_index * 3 + 1] >= INVALID - 9)
						{
							add_border_point(tb, node_index, neighbour_index, p1, HARDCORE_DEBUG);
							add_border_point(tb, node_index, neighbour_index, p2, HARDCORE_DEBUG);
						}
						if (tb->data[neighbour_index].triangle_connections[triangle_index * 3 + 2] >= INVALID - 9)
						{
							add_border_point(tb, node_index, neighbour_index, p2, HARDCORE_DEBUG);
							add_border_point(tb, node_index, neighbour_index, p0, HARDCORE_DEBUG);
						}
					}
				}
			}
		}

		// Triangulation
		generate_triangulate_shader(tb, g, node_index);

		if (HARDCORE_DEBUG)	// After triangulating
		{
			cputri::debug_lines.push_back(std::vector<vec3>());

			for (uint ii = 0; ii < node.index_count; ii += 3)
			{
				vec3 p[3] = {
					node.positions[node.indices[ii + 0]],
					node.positions[node.indices[ii + 1]],
					node.positions[node.indices[ii + 2]]
				};

				cputri::debug_lines.back().push_back(vec3(p[0]) + vec3{ 0, -103, 0 });
				cputri::debug_lines.back().push_back(vec3(p[1]) + vec3{ 0, -103, 0 });
				cputri::debug_lines.back().push_back(vec3(0.0f, 1.0f, 0.5f));

				cputri::debug_lines.back().push_back(vec3(p[1]) + vec3{ 0, -103, 0 });
				cputri::debug_lines.back().push_back(vec3(p[2]) + vec3{ 0, -103, 0 });
				cputri::debug_lines.back().push_back(vec3(0.0f, 1.0f, 0.5f));

				cputri::debug_lines.back().push_back(vec3(p[2]) + vec3{ 0, -103, 0 });
				cputri::debug_lines.back().push_back(vec3(p[0]) + vec3{ 0, -103, 0 });
				cputri::debug_lines.back().push_back(vec3(0.0f, 1.0f, 0.5f));
			}

		}

		// Find out and mark which triangles are part of the outer triangles/supertriangles
		const vec4 start_positions[4] = { node.positions[0],
																			node.positions[1],
																			node.positions[2],
																			node.positions[3] };
		uint triangle_count = node.index_count / 3;
		g.triangles_removed = 0;
		for (uint tt = 0; tt < triangle_count; ++tt)
		{
			// Check each of the three points that make up each triangle
			bool found = false;
			for (uint pp = 0; pp < 3 && !found; ++pp)
			{
				const vec4 test_point = node.positions[node.indices[tt * 3 + pp]];
				// Check against all four supertriangle corners
				for (uint sp = 0; sp < 4; ++sp)
				{
					if (test_point == start_positions[sp])
					{
						uint tr = g.triangles_removed++;
						// Mark the triangle to be removed later
						g.triangles_to_remove[tr] = tt;
						found = true;
						break;
					}
				}
			}
		}

		// Remove the marked triangles
		remove_marked_triangles(tb, g, node_index);

		if (HARDCORE_DEBUG)	// After removing supertris
		{
			cputri::debug_lines.push_back(std::vector<vec3>());

			for (uint ii = 0; ii < node.index_count; ii += 3)
			{
				vec3 p[3] = {
					node.positions[node.indices[ii + 0]],
					node.positions[node.indices[ii + 1]],
					node.positions[node.indices[ii + 2]]
				};

				cputri::debug_lines.back().push_back(vec3(p[0]) + vec3{ 0, -103, 0 });
				cputri::debug_lines.back().push_back(vec3(p[1]) + vec3{ 0, -103, 0 });
				cputri::debug_lines.back().push_back(vec3(0.0f, 1.0f, 0.5f));

				cputri::debug_lines.back().push_back(vec3(p[1]) + vec3{ 0, -103, 0 });
				cputri::debug_lines.back().push_back(vec3(p[2]) + vec3{ 0, -103, 0 });
				cputri::debug_lines.back().push_back(vec3(0.0f, 1.0f, 0.5f));

				cputri::debug_lines.back().push_back(vec3(p[2]) + vec3{ 0, -103, 0 });
				cputri::debug_lines.back().push_back(vec3(p[0]) + vec3{ 0, -103, 0 });
				cputri::debug_lines.back().push_back(vec3(0.0f, 1.0f, 0.5f));
			}

		}

		// Restore borders
		triangle_count = node.index_count / 3;
		node.border_count = 0;
		for (uint tt = 0; tt < triangle_count; ++tt)
		{
			for (uint ss = 0; ss < 3; ++ss)
			{
				if (node.triangle_connections[tt * 3 + ss] >= INVALID - 9)
				{
					node.border_triangle_indices[node.border_count++] = tt;
					break;
				}
			}
		}

		if (HARDCORE_DEBUG)	// triangles tested
		{
			cputri::debug_lines.push_back(std::vector<vec3>());
		}

		// TODO: How big should these arrays be?
		uint g_seen_triangles[300];
		uint g_seen_triangle_count = 0;
		uint g_triangles_to_test[100];
		uint g_test_count = 0;

		// A point in the middle of the node (GRID_SIDE must be uneven)
		// +4 because of supertriangles
		const uint start_point = 4 + (GRID_SIDE * GRID_SIDE) / 2;

		const uint index_count = node.index_count;
		for (uint ii = 0; ii < index_count; ++ii)
		{
			if (node.indices[ii] == start_point)
			{
				g_seen_triangles[g_seen_triangle_count++] = ii / 3;
				g_triangles_to_test[g_test_count++] = ii / 3;
				break;
			}
		}

		if (HARDCORE_DEBUG)
		{
			const vec4 sides[3] = { node.positions[node.indices[g_seen_triangles[0] * 3 + 0]],
																node.positions[node.indices[g_seen_triangles[0] * 3 + 1]],
																node.positions[node.indices[g_seen_triangles[0] * 3 + 2]] };
			vec3 mid = (vec3(sides[0]) + vec3(sides[1]) + vec3(sides[2])) / 3.0f;
			cputri::debug_lines.back().push_back(mid + vec3(0, -103, 0));
			cputri::debug_lines.back().push_back(mid + vec3(0, -153, 0));
			cputri::debug_lines.back().push_back(vec3(0.0f, 1.0f, 0.5f));
		}

#ifdef  _DEBUG
		if (g_test_count == 0)
		{
			__debugbreak();
		}
#endif //  _DEBUG

		g.triangles_removed = 0;

		while (g_test_count != 0)
		{
			const uint test_triangle = g_triangles_to_test[--g_test_count];
			const vec4 sides[3] = { node.positions[node.indices[test_triangle * 3 + 0]],
															node.positions[node.indices[test_triangle * 3 + 1]],
															node.positions[node.indices[test_triangle * 3 + 2]] };

			const vec3 test_middle = (vec3(sides[0]) + vec3(sides[1]) + vec3(sides[2])) / 3.0f;



			if (HARDCORE_DEBUG)
			{
				cputri::debug_lines.back().push_back(vec3(sides[0]) + vec3(0, -103, 0));
				cputri::debug_lines.back().push_back(vec3(sides[1]) + vec3(0, -103, 0));
				cputri::debug_lines.back().push_back(vec3(0.0f, 1.0f, 0.5f));

				cputri::debug_lines.back().push_back(vec3(sides[1]) + vec3(0, -103, 0));
				cputri::debug_lines.back().push_back(vec3(sides[2]) + vec3(0, -103, 0));
				cputri::debug_lines.back().push_back(vec3(0.0f, 1.0f, 0.5f));

				cputri::debug_lines.back().push_back(vec3(sides[2]) + vec3(0, -103, 0));
				cputri::debug_lines.back().push_back(vec3(sides[0]) + vec3(0, -103, 0));
				cputri::debug_lines.back().push_back(vec3(0.0f, 1.0f, 0.5f));
			}

			uint triangles_to_add[3];
			uint triangles_to_add_count = 0;

			// If this triangle is not valid, iteration should not continue
			bool triangle_valid = true;

			uint matching_sides_found = 0;

			// For each side of the triangle
			for (uint ss = 0; ss < 3 && triangle_valid; ++ss)
			{
				bool found = false;
				vec4 e1p1 = sides[ss];
				vec4 e1p2 = sides[(ss + 1) % 3];

				// Check each neighbour node
				for (int y = -1; y <= 1 && !found; ++y)
				{
					for (int x = -1; x <= 1 && !found; ++x)
					{
						// Skip self
						if (x == 0 && y == 0)
							continue;

						const int nx = cx + x;
						const int ny = cy + y;
						// Check if valid node
						if (nx >= 0 && nx < nodes_per_side && ny >= 0 && ny < nodes_per_side)
						{
							const uint neighbour_index = tb->quadtree_index_map[ny * nodes_per_side + nx];
							if (neighbour_index == INVALID || tb->data[neighbour_index].is_invalid)
								continue;

							const uint triangle_count = tb->data[neighbour_index].border_count;
							// Check each border triangle
							for (uint tt = 0; tt < triangle_count && !found; ++tt)
							{
								const uint border_triangle_index = tb->data[neighbour_index].border_triangle_indices[tt];

								const vec4 e2p0 = tb->data[neighbour_index].positions[tb->data[neighbour_index].indices[border_triangle_index * 3 + 0]];
								const vec4 e2p1 = tb->data[neighbour_index].positions[tb->data[neighbour_index].indices[border_triangle_index * 3 + 1]];
								const vec4 e2p2 = tb->data[neighbour_index].positions[tb->data[neighbour_index].indices[border_triangle_index * 3 + 2]];
								const vec4 neighbour_middle = (e2p0 + e2p1 + e2p2) / 3.f;

								uint local_neighbour_index = (y + 1) * 3 + x + 1;

								// Neighbour edge needs to point to current node to be valid
								uint required_connection = INVALID - (8 - local_neighbour_index);

								// Test if current edge matches neighbour triangle edge
								if (is_same_edge(tb, e1p1, e1p2, test_middle, e2p0, e2p1, neighbour_middle, neighbour_index, border_triangle_index * 3 + 0, required_connection, triangle_valid)
									|| is_same_edge(tb, e1p1, e1p2, test_middle, e2p1, e2p2, neighbour_middle, neighbour_index, border_triangle_index * 3 + 1, required_connection, triangle_valid)
									|| is_same_edge(tb, e1p1, e1p2, test_middle, e2p2, e2p0, neighbour_middle, neighbour_index, border_triangle_index * 3 + 2, required_connection, triangle_valid))
								{
									if (triangle_valid)
									{
										// Set connection of tested triangle to point to tested neighbour node
										node.triangle_connections[test_triangle * 3 + ss] = INVALID - (4 + y * 3 + x);
									}

									found = true;
									++matching_sides_found;
								}

								if (!triangle_valid && HARDCORE_DEBUG)
								{
									cputri::debug_lines.back().push_back(vec3(sides[0]) + vec3(0, -104, 0));
									cputri::debug_lines.back().push_back(vec3(sides[1]) + vec3(0, -104, 0));
									cputri::debug_lines.back().push_back(vec3(1.0f, 1.0f, 0.0f));

									cputri::debug_lines.back().push_back(vec3(sides[1]) + vec3(0, -104, 0));
									cputri::debug_lines.back().push_back(vec3(sides[2]) + vec3(0, -104, 0));
									cputri::debug_lines.back().push_back(vec3(1.0f, 1.0f, 0.0f));

									cputri::debug_lines.back().push_back(vec3(sides[2]) + vec3(0, -104, 0));
									cputri::debug_lines.back().push_back(vec3(sides[0]) + vec3(0, -104, 0));
									cputri::debug_lines.back().push_back(vec3(1.0f, 1.0f, 0.0f));
								}
							}
						}
					}
				}

				// If no matching border was found, neighbour triangle should be tested as well
				uint neighbour = node.triangle_connections[test_triangle * 3 + ss];

				if (neighbour < INVALID - 9)
				{
					triangles_to_add[triangles_to_add_count] = neighbour;
					++triangles_to_add_count;
				}
			}

			if (matching_sides_found < 2)
			{
				// Check which node the triangle center is inside
				int tri_x = cx;
				int tri_y = cy;

				if (test_middle.x > node.max.x)
					++tri_x;
				else if (test_middle.x < node.min.x)
					--tri_x;
				if (test_middle.z > node.max.y)
					++tri_y;
				else if (test_middle.z < node.min.y)
					--tri_y;

				// If triangle center is not in this node
				if (tri_x != cx || tri_y != cy)
				{
					// Local index of the node which contains triangle center
					uint tri_node_index;

					if (tri_x < 0 || tri_y < 0 ||
						tri_x > nodes_per_side || tri_y > nodes_per_side)
						tri_node_index = INVALID;
					else
						tri_node_index = tb->quadtree_index_map[tri_y * nodes_per_side + tri_x];

					// If that node does not exist, triangle is invalid
					if (tri_node_index == INVALID || tb->data[tri_node_index].is_invalid)
					{
						triangle_valid = false;

						if (HARDCORE_DEBUG)
						{
							cputri::debug_lines.back().push_back(vec3(sides[0]) + vec3(0, -104, 0));
							cputri::debug_lines.back().push_back(vec3(sides[1]) + vec3(0, -104, 0));
							cputri::debug_lines.back().push_back(vec3(1.0f, 0.0f, 0.5f));

							cputri::debug_lines.back().push_back(vec3(sides[1]) + vec3(0, -104, 0));
							cputri::debug_lines.back().push_back(vec3(sides[2]) + vec3(0, -104, 0));
							cputri::debug_lines.back().push_back(vec3(1.0f, 0.0f, 0.5f));

							cputri::debug_lines.back().push_back(vec3(sides[2]) + vec3(0, -104, 0));
							cputri::debug_lines.back().push_back(vec3(sides[0]) + vec3(0, -104, 0));
							cputri::debug_lines.back().push_back(vec3(1.0f, 0.0f, 0.5f));
						}
					}
				}
			}

			// Test added triangles
			if (triangle_valid)
			{
				for (uint ii = 0; ii < triangles_to_add_count; ii++)
				{
					bool already_tested = false;

					for (uint seen = 0; seen < g_seen_triangle_count; seen++)
					{
						if (g_seen_triangles[seen] == triangles_to_add[ii])
						{
							already_tested = true;
							break;
						}
					}

					if (!already_tested)
					{
						if (HARDCORE_DEBUG)
						{
							const vec4 ns[3] = { node.positions[node.indices[triangles_to_add[ii] * 3 + 0]],
															node.positions[node.indices[triangles_to_add[ii] * 3 + 1]],
															node.positions[node.indices[triangles_to_add[ii] * 3 + 2]] };

							const vec3 tm = (vec3(ns[0]) + vec3(ns[1]) + vec3(ns[2])) / 3.0f;

							cputri::debug_lines.back().push_back(vec3(test_middle) + vec3(0, -104, 0));
							cputri::debug_lines.back().push_back(vec3(tm) + vec3(0, -103, 0));
							cputri::debug_lines.back().push_back(vec3(0.7f, 1.0f, 0.7f));
						}

						g_seen_triangles[g_seen_triangle_count] = triangles_to_add[ii];
						++g_seen_triangle_count;
						g_triangles_to_test[g_test_count] = triangles_to_add[ii];
						++g_test_count;
					}
				}
			}
			else
			{
				// Remove this triangle
				g.triangles_to_remove[g.triangles_removed] = test_triangle;
				++g.triangles_removed;

				// Mark neighbour triangle connections as invalid
				for (uint tt = 0; tt < 3; ++tt)
				{
					const uint triangle_index = node.triangle_connections[test_triangle * 3 + tt];
					if (triangle_index >= INVALID - 9)
						continue;

					for (uint nt = 0; nt < 3; ++nt)
					{
						const uint neighbour_index = node.triangle_connections[triangle_index * 3 + nt];
						
						if (neighbour_index == test_triangle)
						{
							node.triangle_connections[triangle_index * 3 + nt] = UNKNOWN;
						}
					}
				}
			}
		}

		// Find the triangles not seen in the search and mark them for removal
		for (uint tt = 0; tt < node.index_count / 3; ++tt)
		{
			bool seen = false;

			for (uint ss = 0; ss < g_seen_triangle_count; ++ss)
			{
				if (g_seen_triangles[ss] == tt)
				{
					seen = true;
					break;
				}
			}

			if (!seen)
			{
				g.triangles_to_remove[g.triangles_removed] = tt;
				++g.triangles_removed;
			}
		}

		if (g.triangles_removed > (node.index_count / 3) - 2)
			std::cout << node_index << "\n";

		// Remove the marked triangles
		remove_marked_triangles2(tb, g, node_index);

		// Restore borders
		triangle_count = node.index_count / 3;
		node.border_count = 0;
		for (uint tt = 0; tt < triangle_count; ++tt)
		{
			for (uint ss = 0; ss < 3; ++ss)
			{
				if (node.triangle_connections[tt * 3 + ss] >= INVALID - 9)
				{
					node.border_triangle_indices[node.border_count++] = tt;
					break;
				}
			}
		}
	}
}