#include "algorithm/generate.hpp"
#include <vector>
#include <iostream>

namespace generate
{
	void replace_connection_index(GlobalData& g, cuint triangle_to_check, cuint index_to_replace, cuint new_value)
	{
		if (triangle_to_check < INVALID - 9)
		{
			for (uint tt = 0; tt < 3; ++tt)
			{
				const uint triangle_index = g.triangle_connections[triangle_to_check * 3 + tt];
				if (triangle_index == index_to_replace)
				{
					g.triangle_connections[triangle_to_check * 3 + tt] = new_value;
					break;
				}
			}
		}
	}

	void reset_iteration(GlobalData& g)
	{
		g.seen_triangle_count = 0;
		g.test_count = 0;
	}

	uint get_index_of_point(TerrainData & node, vec4 p)
	{
		for (uint pp = 0; pp < node.vertex_count; ++pp)
		{
			if (node.positions[pp] == p)
				return pp;
		}

		// If not found, add point
		node.positions[node.vertex_count] = p;
		return node.vertex_count++;
	}

	void remove_old_triangles(TerrainBuffer* tb, GlobalData& g)
	{
		// Remove old triangles
		for (int j = int(g.triangles_removed) - 1; j >= 0; --j)
		{
			const uint index = g.triangles_to_remove[j];

			const uint last_triangle = g.gen_index_count / 3 - 1;

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
				replace_connection_index(g, g.triangle_connections[last_triangle * 3 + ii], last_triangle, index);
			}

			// Remove triangle
			if (index < last_triangle)
			{
				g.indices[index * 3 + 0] = g.indices[last_triangle * 3 + 0];
				g.indices[index * 3 + 1] = g.indices[last_triangle * 3 + 1];
				g.indices[index * 3 + 2] = g.indices[last_triangle * 3 + 2];
				g.triangles[index].circumcentre = g.triangles[last_triangle].circumcentre;
				g.triangles[index].circumradius2 = g.triangles[last_triangle].circumradius2;
				g.triangle_connections[index * 3 + 0] = g.triangle_connections[last_triangle * 3 + 0];
				g.triangle_connections[index * 3 + 1] = g.triangle_connections[last_triangle * 3 + 1];
				g.triangle_connections[index * 3 + 2] = g.triangle_connections[last_triangle * 3 + 2];

				for (uint tt = 0; tt < g.new_triangle_index_count; ++tt)
				{
					const uint triangle_index = g.new_triangle_indices[tt];
					if (triangle_index == last_triangle)
						g.new_triangle_indices[tt] = index;
				}
			}

			g.gen_index_count -= 3;

			// Update the rest of the new points' triangle index after updating triangles in node
			for (uint ii = 0; ii < g.gen_new_points_count; ++ii)
			{
				if (g.new_points_triangles[ii] == index)
				{
					bool found = false;
					const vec4 new_point = g.new_points[ii];

					// Look through all newly added triangles only
					for (uint tt = 0; tt < g.new_triangle_index_count; ++tt)
					{
						const uint triangle_index = g.new_triangle_indices[tt];
						const vec2 circumcentre = g.triangles[triangle_index].circumcentre;
						const float circumradius2 = g.triangles[triangle_index].circumradius2;

						const float dx = new_point.x - circumcentre.x;
						const float dy = new_point.z - circumcentre.y;

						// Find the first triangle whose cc contains the point
						if (dx * dx + dy * dy < circumradius2)
						{
							g.new_points_triangles[ii] = triangle_index;
							found = true;
							break;
						}
					}

					// Else look through all triangles
					if (!found)
					{
						for (uint tri = 0; tri < g.gen_index_count / 3; ++tri)
						{
							const vec2 circumcentre = g.triangles[tri].circumcentre;
							const float circumradius2 = g.triangles[tri].circumradius2;

							const float dx = new_point.x - circumcentre.x;
							const float dy = new_point.z - circumcentre.y;

							// Find the first triangle whose cc contains the point
							if (dx * dx + dy * dy < circumradius2)
							{
								g.new_points_triangles[ii] = tri;
								found = true;
								break;
							}
						}
					}

					if (!found)
					{
						/*cputri::debug_lines.back().push_back(vec3(new_point));
						cputri::debug_lines.back().push_back(vec3(new_point) + vec3(0, 10, 0));
						cputri::debug_lines.back().push_back({1, 0, 0});*/
					}
				}
				else if (g.new_points_triangles[ii] == last_triangle)
				{
					g.new_points_triangles[ii] = index;
				}
			}
		}

		g.triangles_removed = 0;
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

	void generate_triangulate_shader(TerrainBuffer* tb, GlobalData& g)
	{
		const uint new_points_count = g.gen_new_points_count;

		g.triangles_removed = 0;

		for (uint n = 0; n < new_points_count; ++n)
		{
			//if (true) // Supertris
			//{
			//	cputri::debug_lines.push_back(std::vector<vec3>());
			//}

			const vec4 current_point = g.new_points[n];

			g.seen_triangle_count = 1;
			g.test_count = 1;

			const uint start_index = g.new_points_triangles[n];
			g.seen_triangles[0] = start_index;
			g.triangles_to_test[0] = start_index;
			g.new_triangle_index_count = 0;

			bool finish = false;
			while (g.test_count != 0 && !finish)
			{
				const uint triangle_index = g.triangles_to_test[--g.test_count];
				const vec2 circumcentre = g.triangles[triangle_index].circumcentre;
				const float circumradius2 = g.triangles[triangle_index].circumradius2;

				const float dx = current_point.x - circumcentre.x;
				const float dy = current_point.z - circumcentre.y;

				if (dx * dx + dy * dy < circumradius2)
				{
					// Add triangle edges to edge buffer
					const uint index0 = g.indices[triangle_index * 3 + 0];
					const uint index1 = g.indices[triangle_index * 3 + 1];
					const uint index2 = g.indices[triangle_index * 3 + 2];
					const vec4 p0 = vec4(vec3(g.positions[index0]), 1.0f);
					const vec4 p1 = vec4(vec3(g.positions[index1]), 1.0f);
					const vec4 p2 = vec4(vec3(g.positions[index2]), 1.0f);

					// Store edges to be removed
					uint tr = g.triangles_removed++;
					if (tr >= max_triangles_to_remove || tr >= max_border_edges)
					{
						finish = true;
						break;
					}

					uint ec = tr * 3;
					// Edge 0
					g.generate_edges[ec + 0].p1 = p0;
					g.generate_edges[ec + 0].p2 = p1;
					g.generate_edges[ec + 0].p1_index = index0;
					g.generate_edges[ec + 0].p2_index = index1;
					g.generate_edges[ec + 0].connection = g.triangle_connections[triangle_index * 3 + 0];
					g.generate_edges[ec + 0].old_triangle_index = triangle_index;
					// Edge 1
					g.generate_edges[ec + 1].p1 = p1;
					g.generate_edges[ec + 1].p2 = p2;
					g.generate_edges[ec + 1].p1_index = index1;
					g.generate_edges[ec + 1].p2_index = index2;
					g.generate_edges[ec + 1].connection = g.triangle_connections[triangle_index * 3 + 1];
					g.generate_edges[ec + 1].old_triangle_index = triangle_index;
					// Edge 2
					g.generate_edges[ec + 2].p1 = p2;
					g.generate_edges[ec + 2].p2 = p0;
					g.generate_edges[ec + 2].p1_index = index2;
					g.generate_edges[ec + 2].p2_index = index0;
					g.generate_edges[ec + 2].connection = g.triangle_connections[triangle_index * 3 + 2];
					g.generate_edges[ec + 2].old_triangle_index = triangle_index;

					// Mark the triangle to be removed later
					g.triangles_to_remove[tr] = triangle_index;

					// Add neighbour triangles to be tested
					for (uint ss = 0; ss < 3 && !finish; ++ss)
					{
						const uint index = g.triangle_connections[triangle_index * 3 + ss];

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
					if ((i != j) &&
						(g.generate_edges[i].p1 == g.generate_edges[j].p1 &&
							g.generate_edges[i].p2 == g.generate_edges[j].p2) ||
							(g.generate_edges[i].p1 == g.generate_edges[j].p2 &&
								g.generate_edges[i].p2 == g.generate_edges[j].p1))
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
					g.generate_edges[j].future_index = g.gen_index_count / 3 + g.new_triangle_count;
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
				const uint index_count = g.gen_index_count;
				g.indices[index_count + 0] = g.generate_edges[i].p1_index;
				g.indices[index_count + 1] = g.generate_edges[i].p2_index;
				g.indices[index_count + 2] = g.gen_vertex_count;

				const uint triangle_count = index_count / 3;
				g.new_triangle_indices[g.new_triangle_index_count++] = triangle_count;

				// Set circumcircles for the new triangle
				const float a = distance(vec2(P.x, P.z), vec2(Q.x, Q.z));
				const float b = distance(vec2(P.x, P.z), vec2(R.x, R.z));
				const float c = distance(vec2(R.x, R.z), vec2(Q.x, Q.z));

				const vec2 cc_center = find_circum_center(vec2(P.x, P.z), vec2(Q.x, Q.z), vec2(R.x, R.z));
				const float cc_radius2 = find_circum_radius_squared(a, b, c);

				g.triangles[triangle_count].circumcentre = cc_center;
				g.triangles[triangle_count].circumradius2 = cc_radius2;

				// Connections
				g.triangle_connections[index_count + 0] = g.generate_edges[i].connection;
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
							g.triangle_connections[index_count + 2 - ss] = g.generate_edges[test_index].future_index;
							found = true;
						}
						else if (edges[ss] == g.generate_edges[test_index].p2)
						{
							g.triangle_connections[index_count + 2 - ss] = g.generate_edges[test_index].future_index;
							found = true;
						}
					}
					if (!found)
					{
						g.triangle_connections[index_count + 2 - ss] = INVALID;
						if (!already_added && g.gen_border_count < MAX_BORDER_TRIANGLE_COUNT)
						{
							already_added = true;
							g.border_triangle_indices[g.gen_border_count++] = g.generate_edges[i].future_index;
						}
					}
				}

				replace_connection_index(g, g.generate_edges[i].connection, g.generate_edges[i].old_triangle_index, g.generate_edges[i].future_index);

				g.gen_index_count += 3;
			}

			remove_old_triangles(tb, g);

			// Insert new point
			g.positions[g.gen_vertex_count++] = current_point;

			g.triangles_removed = 0;

			/*if (n < 10)*/
			//{
			//	for (uint i = 0; i < g.gen_index_count; i += 3)
			//	{
			//		vec3 p0 = g.positions[g.indices[i + 0]];
			//		vec3 p1 = g.positions[g.indices[i + 1]];
			//		vec3 p2 = g.positions[g.indices[i + 2]];
			//		vec3 mid = (p0 + p1 + p2) / 3.0f;

			//		cputri::debug_lines.back().push_back(p0);
			//		cputri::debug_lines.back().push_back(p1);
			//		cputri::debug_lines.back().push_back(vec3(0, 1, 0));

			//		cputri::debug_lines.back().push_back(p1);
			//		cputri::debug_lines.back().push_back(p2);
			//		cputri::debug_lines.back().push_back(vec3(0, 1, 0));

			//		cputri::debug_lines.back().push_back(p2);
			//		cputri::debug_lines.back().push_back(p0);
			//		cputri::debug_lines.back().push_back(vec3(0, 1, 0));

			//		cputri::debug_lines.back().push_back(mid);
			//		cputri::debug_lines.back().push_back(mid + vec3(0, 50, 0));
			//		cputri::debug_lines.back().push_back(vec3(0, 1, 1));

			//		//cputri::debug_lines.back().push_back(mid);
			//		//cputri::debug_lines.back().push_back(vec3(g.triangles[i / 3].circumcentre.x, 0, g.triangles[i / 3].circumcentre.y));
			//		//cputri::debug_lines.back().push_back(vec3(0, 1, 1));

			//		//const uint steps = 20;
			//		//const float angle = 3.14159265f * 2.0f / steps;
			//		//for (uint jj = 0; jj < steps + 1; ++jj)
			//		//{
			//		//	float cc_radius = sqrt(g.triangles[i / 3].circumradius2);
			//		//	vec3 cc_mid = { g.triangles[i / 3].circumcentre.x, 0, g.triangles[i / 3].circumcentre.y };

			//		//	cputri::debug_lines.back().push_back(cc_mid + vec3(sinf(angle * jj) * cc_radius, 0.0f, cosf(angle * jj) * cc_radius));
			//		//	cputri::debug_lines.back().push_back(cc_mid + vec3(sinf(angle * (jj + 1)) * cc_radius, 0.0f, cosf(angle * (jj + 1)) * cc_radius));
			//		//	cputri::debug_lines.back().push_back({ 0, 0, 1 });
			//		//}
			//	}
			//}
		}

		g.gen_new_points_count = 0;
	}

	void remove_marked_triangles(TerrainBuffer* tb, GlobalData& g)
	{
		// Remove the outer triangles/supertriangles
		for (int j = int(g.triangles_removed) - 1; j >= 0; --j)
		{
			const uint index = g.triangles_to_remove[j];

			const uint last_triangle = g.gen_index_count / 3 - 1;

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
				const uint triangle_to_check = g.triangle_connections[index * 3 + ii];
				if (triangle_to_check < INVALID - 9)
				{
					// Find the side that points back to the current triangle
					for (uint tt = 0; tt < 3; ++tt)
					{
						const uint triangle_index = g.triangle_connections[triangle_to_check * 3 + tt];
						if (triangle_index == index)
						{
							// Mark that side connection as INVALID since that triangle is being removed
							g.triangle_connections[triangle_to_check * 3 + tt] = UNKNOWN;
						}
					}
				}
			}
			for (uint ii = 0; ii < 3; ++ii)
			{
				replace_connection_index(g, g.triangle_connections[last_triangle * 3 + ii], last_triangle, index);
			}

			// Remove triangle
			if (index < last_triangle)
			{
				g.indices[index * 3 + 0] = g.indices[last_triangle * 3 + 0];
				g.indices[index * 3 + 1] = g.indices[last_triangle * 3 + 1];
				g.indices[index * 3 + 2] = g.indices[last_triangle * 3 + 2];
				g.triangles[index].circumcentre = g.triangles[last_triangle].circumcentre;
				g.triangles[index].circumradius2 = g.triangles[last_triangle].circumradius2;
				g.triangle_connections[index * 3 + 0] = g.triangle_connections[last_triangle * 3 + 0];
				g.triangle_connections[index * 3 + 1] = g.triangle_connections[last_triangle * 3 + 1];
				g.triangle_connections[index * 3 + 2] = g.triangle_connections[last_triangle * 3 + 2];

				for (int ii = 0; ii < j; ++ii)
				{
					if (g.triangles_to_remove[ii] == last_triangle)
						g.triangles_to_remove[ii] = index;
				}
			}

			g.gen_index_count -= 3;
		}

		g.triangles_removed = 0;
	}

	void remove_marked_triangles2(TerrainBuffer* tb, GlobalData& g, uint node_index)
	{
		// Remove the outer triangles/supertriangles
		for (int j = int(g.triangles_removed) - 1; j >= 0; --j)
		{
			const uint index = g.triangles_to_remove[j];

			const uint last_triangle = g.gen_index_count / 3 - 1;

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
				replace_connection_index(g, g.triangle_connections[last_triangle * 3 + ii], last_triangle, index);
			}

			// Remove triangle
			if (index < last_triangle)
			{
				g.indices[index * 3 + 0] = g.indices[last_triangle * 3 + 0];
				g.indices[index * 3 + 1] = g.indices[last_triangle * 3 + 1];
				g.indices[index * 3 + 2] = g.indices[last_triangle * 3 + 2];
				g.triangles[index].circumcentre = g.triangles[last_triangle].circumcentre;
				g.triangles[index].circumradius2 = g.triangles[last_triangle].circumradius2;
				g.triangle_connections[index * 3 + 0] = g.triangle_connections[last_triangle * 3 + 0];
				g.triangle_connections[index * 3 + 1] = g.triangle_connections[last_triangle * 3 + 1];
				g.triangle_connections[index * 3 + 2] = g.triangle_connections[last_triangle * 3 + 2];

				for (int ii = 0; ii < j; ++ii)
				{
					if (g.triangles_to_remove[ii] == last_triangle)
						g.triangles_to_remove[ii] = index;
				}
			}

			g.gen_index_count -= 3;
		}

		g.triangles_removed = 0;
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
			float a, b, c;
			calcLine(e1p1, e1p2, a, b, c);
			if (sign(a * test_middle.x + b * test_middle.z + c) != sign(a * neighbour_middle.x + b * neighbour_middle.z + c))
			{
				if (tb->data[neighbour_node_index].triangle_connections[neighbour_border_index] == connection_value ||
					tb->data[neighbour_node_index].triangle_connections[neighbour_border_index] == UNKNOWN)
				{
					// Update connection value (required if it is = UNKNOWN)
					tb->data[neighbour_node_index].triangle_connections[neighbour_border_index] = connection_value;
				}

				return true;
			}
			else
			{
				valid = false;
			}
		}

		return false;
	}

	EdgeComp is_same_edge(vec4 e1p1, vec4 e1p2, vec3 test_middle, vec4 e2p1, vec4 e2p2, vec3 neighbour_middle, uint neighbour_border_value)
	{
		if (((e1p1 == e2p1 && e1p2 == e2p2) ||
			(e1p2 == e2p1 && e1p1 == e2p2)))
		{
			float a, b, c;
			calcLine(e1p1, e1p2, a, b, c);
			if (sign(a * test_middle.x + b * test_middle.z + c) != sign(a * neighbour_middle.x + b * neighbour_middle.z + c))
				return EdgeComp::VALID;
			else
				return EdgeComp::INVALID;
		}

		return EdgeComp::NO_MATCH;
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

	void generate(TerrainBuffer* tb, GlobalData& g, float* log_filter, cputri::TriData* tri_data, GenerateInfo* gen_info, uint num_nodes)
	{
		bool HARDCORE_DEBUG = false;
		if (tri_data->debug_generation)
		{
			cputri::debug_lines.clear();
			HARDCORE_DEBUG = true;
		}

		const int nodes_per_side = 1 << quadtree_levels;
		const uint GRID_SIDE = TERRAIN_GENERATE_GRID_SIDE;

		const float side = gen_info[0].max.x - gen_info[0].min.x;
		const float max_triangle_side_length = (side / (GRID_SIDE - 1)) * 2.0f;

		// Reset supernode values
		g.gen_index_count = 0;
		g.gen_vertex_count = 0;
		g.gen_new_points_count = 0;
		g.gen_border_count = 0;

		// Min and max for supernode
		vec2 min = gen_info[0].min;
		vec2 max = gen_info[0].max;

		for (uint ii = 0; ii < num_nodes; ++ii)
		{
			tb->data[gen_info[ii].index].min = gen_info[ii].min;
			tb->data[gen_info[ii].index].max = gen_info[ii].max;

			if (gen_info[ii].min.x < min.x)
				min.x = gen_info[ii].min.x;
			if (gen_info[ii].min.y < min.y)
				min.y = gen_info[ii].min.y;

			if (gen_info[ii].max.x > max.x)
				max.x = gen_info[ii].max.x;
			if (gen_info[ii].max.y > max.y)
				max.y = gen_info[ii].max.y;
		}

		min -= vec2(side, side);
		max += vec2(side, side);

		// Add supertriangles
		{
			g.gen_index_count = 6;
			g.gen_vertex_count = 4;

			vec4 p0 = vec4(min.x - side, 0, min.y - side, 1);
			vec4 p1 = vec4(max.x + side, 0, min.y - side, 1);
			vec4 p2 = vec4(max.x + side, 0, max.y + side, 1);
			vec4 p3 = vec4(min.x - side, 0, max.y + side, 1);

			g.positions[0] = p0;
			g.positions[1] = p1;
			g.positions[2] = p2;
			g.positions[3] = p3;

			g.indices[0] = 0;
			g.indices[1] = 1;
			g.indices[2] = 2;
			g.indices[3] = 2;
			g.indices[4] = 3;
			g.indices[5] = 0;

			const vec2 P = vec2(p0.x, p0.z);
			const vec2 Q = vec2(p1.x, p1.z);
			const vec2 R = vec2(p2.x, p2.z);
			const vec2 S = vec2(p3.x, p3.z);
			g.triangles[0].circumcentre = find_circum_center(P, Q, R);
			g.triangles[0].circumradius2 = find_circum_radius_squared(P, Q, R);
			g.triangles[1].circumcentre = find_circum_center(R, S, P);
			g.triangles[1].circumradius2 = find_circum_radius_squared(R, S, P);

			g.triangle_connections[0 + 0] = UNKNOWN;
			g.triangle_connections[0 + 1] = UNKNOWN;
			g.triangle_connections[0 + 2] = 1;
			g.triangle_connections[3 + 0] = UNKNOWN;
			g.triangle_connections[3 + 1] = UNKNOWN;
			g.triangle_connections[3 + 2] = 0;
		}

		// Add additional supertri points
		uint new_points_x = 2 + uint((max.x - min.x) / side / 2.0f);
		uint new_points_y = 2 + uint((max.y - min.y) / side / 2.0f);

		for (uint ii = 0; ii < new_points_x; ii++)
		{
			add_supernode_new_point(vec4(min.x + (max.x - min.x) * (float(ii) / float(new_points_x)), 0.0f, min.y, 1.0f), g);
			add_supernode_new_point(vec4(min.x + (max.x - min.x) * (float(ii) / float(new_points_x)), 0.0f, max.y, 1.0f), g);
		}

		for (uint ii = 0; ii < new_points_y; ii++)
		{
			add_supernode_new_point(vec4(min.x, 0.0f, min.y + (max.y - min.y) * (float(ii) / float(new_points_y)), 1.0f), g);
			add_supernode_new_point(vec4(max.x, 0.0f, min.y + (max.y - min.y) * (float(ii) / float(new_points_y)), 1.0f), g);
		}

		const uint num_supertri_points = 4 + g.gen_new_points_count;

		if (HARDCORE_DEBUG) // Supertris
		{
			cputri::debug_lines.push_back(std::vector<vec3>());

			for (uint i = 0; i < g.gen_index_count; i += 3)
			{
				vec3 p0 = g.positions[g.indices[i + 0]];
				vec3 p1 = g.positions[g.indices[i + 1]];
				vec3 p2 = g.positions[g.indices[i + 2]];

				cputri::debug_lines.back().push_back(p0);
				cputri::debug_lines.back().push_back(p1);
				cputri::debug_lines.back().push_back(vec3(0, 1, 0));

				cputri::debug_lines.back().push_back(p1);
				cputri::debug_lines.back().push_back(p2);
				cputri::debug_lines.back().push_back(vec3(0, 1, 0));

				cputri::debug_lines.back().push_back(p2);
				cputri::debug_lines.back().push_back(p0);
				cputri::debug_lines.back().push_back(vec3(0, 1, 0));

				const uint steps = 20;
				const float angle = 3.14159265f * 2.0f / steps;
				for (uint jj = 0; jj < steps + 1; ++jj)
				{
					float cc_radius = sqrt(g.triangles[i].circumradius2);
					vec3 cc_mid = { g.triangles[i].circumcentre.x, 0, g.triangles[i].circumcentre.y };

					cputri::debug_lines.back().push_back(cc_mid + vec3(sinf(angle * jj) * cc_radius, 0.0f, cosf(angle * jj) * cc_radius));
					cputri::debug_lines.back().push_back(cc_mid + vec3(sinf(angle * (jj + 1)) * cc_radius, 0.0f, cosf(angle * (jj + 1)) * cc_radius));
					cputri::debug_lines.back().push_back({ 0, 0, 1 });
				}
			}
		}

		// For every node, add static points
		for (uint nn = 0; nn < num_nodes; ++nn)
		{
			// Generate static positions
			for (uint i = 0; i < GRID_SIDE * GRID_SIDE; ++i)
			{
				float x = gen_info[nn].min.x + ((i % GRID_SIDE) / float(GRID_SIDE - 1)) * side + (side / GRID_SIDE) * 0.5f * ((i / GRID_SIDE) % 2);
				float z = gen_info[nn].min.y + float(i / GRID_SIDE) / float(GRID_SIDE - 1) * side;

				if ((i % GRID_SIDE) / float(GRID_SIDE - 1) == 0)
					x = gen_info[nn].min.x;
				else if ((i % GRID_SIDE) / float(GRID_SIDE - 1) > 0.99f)
					x = gen_info[nn].max.x;

				// If this is the middle point, set is as the start point for this node
				if (i == (GRID_SIDE * GRID_SIDE) / 2)
					// +4 for supertriangle points
					g.gen_starting_points[nn] = 4 + g.gen_new_points_count;	// Before add_supernode_new_point(), which increments g.new_triangle_count

				add_supernode_new_point(vec4(x, terrain(vec2(x, z)) - 0.5, z, curvature(vec3(x, 0.0f, z), log_filter)), g);
			}
		}

		// For every node, add points from neighbours
		for (uint nn = 0; nn < num_nodes; ++nn)
		{
			const int cx = int((gen_info[nn].min.x - tb->quadtree_min->x + 1) / side);  // current node x
			const int cy = int((gen_info[nn].min.y - tb->quadtree_min->y + 1) / side);  // current node z/y

			for (int y = -1; y <= 1; ++y)
			{
				for (int x = -1; x <= 1; ++x)
				{
					if (!valid_node_neighbour(tb, cx, cy, x, y) || (x == 0 && y == 0))
						continue;

					const int nx = cx + x;
					const int ny = cy + y;
					const uint neighbour_index = tb->quadtree_index_map[ny * nodes_per_side + nx];

					const uint triangle_count = tb->data[neighbour_index].border_count;

					// Check each border triangle
					for (uint tt = 0; tt < triangle_count; ++tt)
					{
						const uint triangle_index = tb->data[neighbour_index].border_triangle_indices[tt];
						const vec4 p0 = tb->data[neighbour_index].positions[tb->data[neighbour_index].indices[triangle_index * 3 + 0]];
						const vec4 p1 = tb->data[neighbour_index].positions[tb->data[neighbour_index].indices[triangle_index * 3 + 1]];
						const vec4 p2 = tb->data[neighbour_index].positions[tb->data[neighbour_index].indices[triangle_index * 3 + 2]];

						// Find the side(s) that actually faces the border (and therefore has an INVALID connection)
						if (tb->data[neighbour_index].triangle_connections[triangle_index * 3 + 0] == INVALID - 4 + y * 3 + x)
						{
							add_supernode_new_point(p0, g);
							add_supernode_new_point(p1, g);
						}
						if (tb->data[neighbour_index].triangle_connections[triangle_index * 3 + 1] == INVALID - 4 + y * 3 + x)
						{
							add_supernode_new_point(p1, g);
							add_supernode_new_point(p2, g);
						}
						if (tb->data[neighbour_index].triangle_connections[triangle_index * 3 + 2] == INVALID - 4 + y * 3 + x)
						{
							add_supernode_new_point(p2, g);
							add_supernode_new_point(p0, g);
						}
					}
				}
			}
		}

		if (HARDCORE_DEBUG) // Points
		{
			cputri::debug_lines.push_back(std::vector<vec3>());

			for (uint i = 0; i < g.gen_new_points_count; ++i)
			{
				cputri::debug_lines.back().push_back(g.new_points[i]);
				cputri::debug_lines.back().push_back(vec3(g.new_points[i]) + vec3(0, 500, 0));
				cputri::debug_lines.back().push_back(vec3(0, 1, 0));
			}
		}


		if (HARDCORE_DEBUG) // Failed points
		{
			cputri::debug_lines.push_back(std::vector<vec3>());
		}
		// Triangulate
		generate_triangulate_shader(tb, g);
		g.triangles_removed = 0;
		g.gen_new_points_count = 0;

		if (HARDCORE_DEBUG) // Starting points
		{
			cputri::debug_lines.push_back(std::vector<vec3>());

			for (uint i = 0; i < num_nodes; ++i)
			{
				cputri::debug_lines.back().push_back(g.positions[g.gen_starting_points[i]]);
				cputri::debug_lines.back().push_back(g.positions[g.gen_starting_points[i]] + vec4(0, -10, 0, 1));
				cputri::debug_lines.back().push_back(vec3(1, 1, 0));
			}
		}

		// Find out and mark which triangles are part of the outer triangles/supertriangles
		uint triangle_count = g.gen_index_count / 3;
		for (uint tt = 0; tt < triangle_count; ++tt)
		{
			if (g.indices[tt * 3 + 0] < num_supertri_points ||
				g.indices[tt * 3 + 1] < num_supertri_points ||
				g.indices[tt * 3 + 2] < num_supertri_points)
			{
				uint tr = g.triangles_removed++;
				// Mark the triangle to be removed later
				g.triangles_to_remove[tr] = tt;
			}
		}

		if (HARDCORE_DEBUG) // Before removing supertris
		{
			cputri::debug_lines.push_back(std::vector<vec3>());

			for (uint i = 0; i < g.gen_index_count; i += 3)
			{
				vec3 p0 = g.positions[g.indices[i + 0]];
				vec3 p1 = g.positions[g.indices[i + 1]];
				vec3 p2 = g.positions[g.indices[i + 2]];

				cputri::debug_lines.back().push_back(p0);
				cputri::debug_lines.back().push_back(p1);
				cputri::debug_lines.back().push_back(vec3(0, 1, 0));

				cputri::debug_lines.back().push_back(p1);
				cputri::debug_lines.back().push_back(p2);
				cputri::debug_lines.back().push_back(vec3(0, 1, 0));

				cputri::debug_lines.back().push_back(p2);
				cputri::debug_lines.back().push_back(p0);
				cputri::debug_lines.back().push_back(vec3(0, 1, 0));
			}
		}

		// Remove the marked triangles
		remove_marked_triangles(tb, g);

		if (HARDCORE_DEBUG) // After removing supertris
		{
			cputri::debug_lines.push_back(std::vector<vec3>());

			for (uint i = 0; i < g.gen_index_count; i += 3)
			{
				vec3 p0 = g.positions[g.indices[i + 0]];
				vec3 p1 = g.positions[g.indices[i + 1]];
				vec3 p2 = g.positions[g.indices[i + 2]];

				cputri::debug_lines.back().push_back(p0 + vec3(0, 105, 0));
				cputri::debug_lines.back().push_back(p1 + vec3(0, 105, 0));
				cputri::debug_lines.back().push_back(vec3(0, 1, 0));

				cputri::debug_lines.back().push_back(p1 + vec3(0, 105, 0));
				cputri::debug_lines.back().push_back(p2 + vec3(0, 105, 0));
				cputri::debug_lines.back().push_back(vec3(0, 1, 0));

				cputri::debug_lines.back().push_back(p2 + vec3(0, 105, 0));
				cputri::debug_lines.back().push_back(p0 + vec3(0, 105, 0));
				cputri::debug_lines.back().push_back(vec3(0, 1, 0));

				cputri::debug_lines.back().push_back((p0 + p1 + p2) / 3.0f + vec3(0, 105, 0));
				cputri::debug_lines.back().push_back((p0 + p1 + p2) / 3.0f + vec3(0, 110, 0));
				cputri::debug_lines.back().push_back(vec3(0, 0, 1));
			}
		}

		// Fill target array
		for (uint ind = 0; ind < g.gen_index_count / 3; ++ind)
		{
			g.gen_triangle_targets[ind] = INVALID;
		}
		g.gen_marked_count = 0;

		if (HARDCORE_DEBUG) // Iteration 1
		{
			cputri::debug_lines.push_back(std::vector<vec3>());
		}

		// For every node, find starting triangle from starting point and iterate from starting triangle
		// Stop when reaching neighbour edge
		// If triangle center is outside node, mark the triangle and stop
		// Place valid triangles in node
		for (uint nn = 0; nn < num_nodes; ++nn)
		{
			reset_iteration(g);

			TerrainData& node = tb->data[gen_info[nn].index];

			node.index_count = 0;
			node.vertex_count = 0;
			node.old_vertex_count = 0;
			node.border_count = 0;
			node.new_points_count = 0;

			// Find starting triangle
			const uint index_count = g.gen_index_count;
			for (uint ii = 0; ii < index_count; ++ii)
			{
				if (g.indices[ii] == g.gen_starting_points[nn])
				{
					g.seen_triangles[g.seen_triangle_count++] = ii / 3;
					g.triangles_to_test[g.test_count++] = ii / 3;
					break;
				}
			}

			if (HARDCORE_DEBUG && g.test_count > 0) // Start tris
			{
				const uint test_triangle = g.triangles_to_test[g.test_count - 1];
				const vec4 sides[3] = { g.positions[g.indices[test_triangle * 3 + 0]],
										g.positions[g.indices[test_triangle * 3 + 1]],
										g.positions[g.indices[test_triangle * 3 + 2]] };

				vec4 m = (sides[0] + sides[1] + sides[2]) / 3.0f;

				cputri::debug_lines.back().push_back(vec3(m) + vec3(0, 105, 0));
				cputri::debug_lines.back().push_back(vec3(m) + vec3(0, 135, 0));
				cputri::debug_lines.back().push_back(vec3(0, 1, 0));
			}

			// Iterate
			while (g.test_count != 0)
			{
				const uint test_triangle = g.triangles_to_test[--g.test_count];
				const vec4 sides[3] = { g.positions[g.indices[test_triangle * 3 + 0]],
										g.positions[g.indices[test_triangle * 3 + 1]],
										g.positions[g.indices[test_triangle * 3 + 2]] };

				const vec3 test_middle = (vec3(sides[0]) + vec3(sides[1]) + vec3(sides[2])) / 3.0f;

				if (test_middle.x < gen_info[nn].min.x || test_middle.x > gen_info[nn].max.x ||
					test_middle.z < gen_info[nn].min.y || test_middle.z > gen_info[nn].max.y)
				{
					g.gen_marked_triangles[g.gen_marked_count].gen_index = test_triangle;
					g.gen_marked_triangles[g.gen_marked_count].node = nn;
					++g.gen_marked_count;

					if (HARDCORE_DEBUG)
					{
						cputri::debug_lines.back().push_back(vec3(sides[0]) + vec3(0, 104, 0));
						cputri::debug_lines.back().push_back(vec3(sides[1]) + vec3(0, 104, 0));
						cputri::debug_lines.back().push_back(vec3(1, 1, 0));

						cputri::debug_lines.back().push_back(vec3(sides[1]) + vec3(0, 104, 0));
						cputri::debug_lines.back().push_back(vec3(sides[2]) + vec3(0, 104, 0));
						cputri::debug_lines.back().push_back(vec3(1, 1, 0));

						cputri::debug_lines.back().push_back(vec3(sides[2]) + vec3(0, 104, 0));
						cputri::debug_lines.back().push_back(vec3(sides[0]) + vec3(0, 104, 0));
						cputri::debug_lines.back().push_back(vec3(1, 1, 0));
					}

					continue;
				}

				// If border connections are found, they are stored here
				uint connections[3] = { UNKNOWN, UNKNOWN, UNKNOWN };

				uint triangles_to_add[3];
				uint triangles_to_add_count = 0;

				// For each side of the triangle
				for (uint ss = 0; ss < 3; ++ss)
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

							const int nx = gen_info[nn].x + x;
							const int ny = gen_info[nn].y + y;

							if (valid_node_neighbour(tb, gen_info[nn].x, gen_info[nn].y, x, y))
							{
								const uint neighbour_index = tb->quadtree_index_map[ny * nodes_per_side + nx];
								TerrainData& neighbour = tb->data[neighbour_index];
								const uint triangle_count = neighbour.border_count;

								// Check each border triangle
								for (uint tt = 0; tt < triangle_count && !found; ++tt)
								{
									const uint border_triangle_index = neighbour.border_triangle_indices[tt];

									const vec4 e2p[3] = {
										neighbour.positions[neighbour.indices[border_triangle_index * 3 + 0]],
										neighbour.positions[neighbour.indices[border_triangle_index * 3 + 1]],
										neighbour.positions[neighbour.indices[border_triangle_index * 3 + 2]]
									};
									const vec4 neighbour_middle = (e2p[0] + e2p[1] + e2p[2]) / 3.f;

									uint local_neighbour_index = (y + 1) * 3 + x + 1;

									// Neighbour edge needs to point to current node to be valid
									uint required_connection = INVALID - (8 - local_neighbour_index);

									// For each side of neighbour triangle
									for (uint ns = 0; ns < 3 && !found; ++ns)
									{
										uint neighbour_connection = neighbour.triangle_connections[border_triangle_index * 3 + ns];
										EdgeComp edge_val = is_same_edge(e1p1, e1p2, test_middle, e2p[ns], e2p[(ns + 1) % 3], neighbour_middle, neighbour_connection);

										if (edge_val == EdgeComp::NO_MATCH)
											continue;

										found = true;

										if (neighbour_connection == UNKNOWN)
											neighbour.triangle_connections[border_triangle_index * 3 + ns] = required_connection;

										connections[ss] = INVALID - local_neighbour_index;
										// Set connection in supernode to avoid the connection being overwritten later
										g.triangle_connections[test_triangle * 3 + ss] = INVALID - local_neighbour_index;

										if (HARDCORE_DEBUG)
										{
											cputri::debug_lines.back().push_back(vec3(e1p1) + vec3(0, 105, 0));
											cputri::debug_lines.back().push_back(vec3(e1p2) + vec3(0, 105, 0));
											cputri::debug_lines.back().push_back(vec3(1, 1, 1));
										}
									}
								}
							}
						}
					}

					// If no matching border was found, neighbour triangle should be tested as well
					if (!found)
					{
						uint neighbour = g.triangle_connections[test_triangle * 3 + ss];

						if (neighbour < INVALID - 9)
						{
							triangles_to_add[triangles_to_add_count] = neighbour;
							++triangles_to_add_count;
						}

					}
				}

				if (HARDCORE_DEBUG)
				{
					cputri::debug_lines.back().push_back(vec3(sides[0]) + vec3(0, 104, 0));
					cputri::debug_lines.back().push_back(vec3(sides[1]) + vec3(0, 104, 0));
					cputri::debug_lines.back().push_back(vec3(0, 1, 0));

					cputri::debug_lines.back().push_back(vec3(sides[1]) + vec3(0, 104, 0));
					cputri::debug_lines.back().push_back(vec3(sides[2]) + vec3(0, 104, 0));
					cputri::debug_lines.back().push_back(vec3(0, 1, 0));

					cputri::debug_lines.back().push_back(vec3(sides[2]) + vec3(0, 104, 0));
					cputri::debug_lines.back().push_back(vec3(sides[0]) + vec3(0, 104, 0));
					cputri::debug_lines.back().push_back(vec3(0, 1, 0));
				}

				// Test added triangles
				for (uint ii = 0; ii < triangles_to_add_count; ii++)
				{
					bool already_tested = false;

					for (uint seen = 0; seen < g.seen_triangle_count; seen++)
					{
						if (g.seen_triangles[seen] == triangles_to_add[ii])
						{
							already_tested = true;
							break;
						}
					}

					if (HARDCORE_DEBUG && g.gen_triangle_targets[triangles_to_add[ii]] != INVALID)
					{
						vec3 pn[3] =
						{
							g.positions[g.indices[triangles_to_add[ii] * 3 + 0]],
							g.positions[g.indices[triangles_to_add[ii] * 3 + 1]],
							g.positions[g.indices[triangles_to_add[ii] * 3 + 2]]
						};

						vec3 nm = (pn[0] + pn[1] + pn[2]) / 3.0f;

						cputri::debug_lines.back().push_back(vec3(test_middle) + vec3(0, 110, 0));
						cputri::debug_lines.back().push_back(vec3(nm) + vec3(0, 105, 0));
						cputri::debug_lines.back().push_back(vec3(1, 0.5, 0.5f));
					}

					if (!already_tested)
					{
						g.seen_triangles[g.seen_triangle_count] = triangles_to_add[ii];
						++g.seen_triangle_count;
						g.triangles_to_test[g.test_count] = triangles_to_add[ii];
						++g.test_count;
					}
				}

				// Add tested triangle to node
				const uint new_index = node.index_count;
				node.indices[new_index + 0] = get_index_of_point(node, sides[0]);
				node.indices[new_index + 1] = get_index_of_point(node, sides[1]);
				node.indices[new_index + 2] = get_index_of_point(node, sides[2]);
				node.index_count += 3;

				node.triangles[new_index / 3].circumcentre = find_circum_center(
					vec2(sides[0].x, sides[0].z),
					vec2(sides[1].x, sides[1].z),
					vec2(sides[2].x, sides[2].z));
				node.triangles[new_index / 3].circumradius2 = find_circum_radius_squared(
					vec2(sides[0].x, sides[0].z),
					vec2(sides[1].x, sides[1].z),
					vec2(sides[2].x, sides[2].z));

				if (connections[0] != UNKNOWN)
					node.triangle_connections[new_index + 0] = connections[0];
				if (connections[1] != UNKNOWN)
					node.triangle_connections[new_index + 1] = connections[1];
				if (connections[2] != UNKNOWN)
					node.triangle_connections[new_index + 2] = connections[2];

				g.gen_triangle_targets[test_triangle] = nn;
				g.gen_triangle_new_indices[test_triangle] = new_index / 3;
			}
		}

		// Set node is_invalid to false
		for (uint nn = 0; nn < num_nodes; ++nn)
		{
			tb->data[gen_info[nn].index].is_invalid = false;
		}

		if (HARDCORE_DEBUG) // Iteration 2
		{
			cputri::debug_lines.push_back(std::vector<vec3>());
		}

		// Iterate from marked triangles
		// Stop when reaching neighbour edge
		// This handles remaining marked triangles
		// Place valid triangles in node
		for (uint mm = 0; mm < g.gen_marked_count; ++mm)
		{
			reset_iteration(g);

			TerrainData& node = tb->data[gen_info[g.gen_marked_triangles[mm].node].index];

			// Index in gen_info
			uint gi = g.gen_marked_triangles[mm].node;

			// Initialize data
			g.triangles_to_test[0] = g.gen_marked_triangles[mm].gen_index;
			g.test_count = 1;
			g.seen_triangles[0] = g.gen_marked_triangles[mm].gen_index;
			g.seen_triangle_count = 1;

			if (HARDCORE_DEBUG) // Start tris
			{
				const uint test_triangle = g.triangles_to_test[g.test_count - 1];
				const vec4 sides[3] = { g.positions[g.indices[test_triangle * 3 + 0]],
										g.positions[g.indices[test_triangle * 3 + 1]],
										g.positions[g.indices[test_triangle * 3 + 2]] };

				vec4 m = (sides[0] + sides[1] + sides[2]) / 3.0f;

				cputri::debug_lines.back().push_back(vec3(m) + vec3(0, 105, 0));
				cputri::debug_lines.back().push_back(vec3(m) + vec3(0, 135, 0));
				cputri::debug_lines.back().push_back(vec3(0, 1, 0));
			}

			// Iterate
			while (g.test_count != 0)
			{
				const uint test_triangle = g.triangles_to_test[--g.test_count];

				// If this triangle has been placed in a node, skip it
				if (g.gen_triangle_targets[test_triangle] != INVALID)
					continue;

				const vec4 sides[3] = { g.positions[g.indices[test_triangle * 3 + 0]],
										g.positions[g.indices[test_triangle * 3 + 1]],
										g.positions[g.indices[test_triangle * 3 + 2]] };

				const vec3 test_middle = (vec3(sides[0]) + vec3(sides[1]) + vec3(sides[2])) / 3.0f;

				bool remove = false;
				vec3 remove_col = { 1, 1, 1 };

				// If a triangle side is longer than allowed, triangle is invalid
				if (length(sides[0] - sides[1]) > max_triangle_side_length ||
					length(sides[1] - sides[2]) > max_triangle_side_length ||
					length(sides[2] - sides[0]) > max_triangle_side_length)
				{
					remove = true;
					remove_col = { 1, 0, 0 };
				}

				// If triangle middle is inside an invalid node, triangle is invalid
				{
					// X/Y node coords of triangle middle

					/*int x = (test_middle.x - tb->quadtree_min.x) / side;
					int y = (test_middle.z - tb->quadtree_min.y) / side;*/

					float x_mid = test_middle.x;
					float y_mid = test_middle.z;
					int x_count = 0;
					int y_count = 0;

					while (x_mid < node.min.x)
					{
						x_count--;
						x_mid += side;
					}
					while (x_mid > node.max.x)
					{
						x_count++;
						x_mid -= side;
					}
					while (y_mid < node.min.y)
					{
						y_count--;
						y_mid += side;
					}
					while (y_mid > node.max.y)
					{
						y_count++;
						y_mid -= side;
					}

					int x = gen_info[g.gen_marked_triangles[mm].node].x + x_count;
					int y = gen_info[g.gen_marked_triangles[mm].node].y + y_count;

					if (x < 0 || y < 0 || x >= (1 << quadtree_levels) || y >= (1 << quadtree_levels))
					{
						remove = true;
						remove_col = { (x + 10) / (1 << quadtree_levels), (y + 10) / (1 << quadtree_levels), 1 };
					}

					if (!remove)
					{
						uint index = tb->quadtree_index_map[y * (1 << quadtree_levels) + x];

						if (index == INVALID)
						{
							remove = true;
							remove_col = { 0.5f, 0.5f, 0.8f };
						}
						if (index != INVALID && tb->data[index].is_invalid)
						{
							remove = true;
							remove_col = { 0.8f, 0.8f, 0.3f };
						}
					}
				}

				// Replace connections for neighbours
				if (remove)
				{
					for (uint ss = 0; ss < 3; ++ss)
					{
						uint neighbour = g.triangle_connections[test_triangle * 3 + ss];
						replace_connection_index(g, neighbour, test_triangle, UNKNOWN);
					}

					if (HARDCORE_DEBUG)
					{
						cputri::debug_lines.back().push_back(vec3(sides[0]) + vec3(0, 104, 0));
						cputri::debug_lines.back().push_back(vec3(sides[1]) + vec3(0, 104, 0));
						cputri::debug_lines.back().push_back(remove_col);

						cputri::debug_lines.back().push_back(vec3(sides[1]) + vec3(0, 104, 0));
						cputri::debug_lines.back().push_back(vec3(sides[2]) + vec3(0, 104, 0));
						cputri::debug_lines.back().push_back(remove_col);

						cputri::debug_lines.back().push_back(vec3(sides[2]) + vec3(0, 104, 0));
						cputri::debug_lines.back().push_back(vec3(sides[0]) + vec3(0, 104, 0));
						cputri::debug_lines.back().push_back(remove_col);
					}

					continue;
				}

				// If border connections are found, they are stored here
				uint connections[3] = { UNKNOWN, UNKNOWN, UNKNOWN };

				uint triangles_to_add[3];
				uint triangles_to_add_count = 0;

				bool triangle_valid = true;

				// For each side of the triangle
				for (uint ss = 0; ss < 3; ++ss)
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

							const int nx = gen_info[gi].x + x;
							const int ny = gen_info[gi].y + y;

							if (valid_node_neighbour(tb, gen_info[gi].x, gen_info[gi].y, x, y))
							{
								const uint neighbour_index = tb->quadtree_index_map[ny * nodes_per_side + nx];
								TerrainData& neighbour = tb->data[neighbour_index];
								const uint triangle_count = neighbour.border_count;

								// Check each border triangle
								for (uint tt = 0; tt < triangle_count && !found; ++tt)
								{
									const uint border_triangle_index = neighbour.border_triangle_indices[tt];

									const vec4 e2p[3] = {
										neighbour.positions[neighbour.indices[border_triangle_index * 3 + 0]],
										neighbour.positions[neighbour.indices[border_triangle_index * 3 + 1]],
										neighbour.positions[neighbour.indices[border_triangle_index * 3 + 2]]
									};
									const vec4 neighbour_middle = (e2p[0] + e2p[1] + e2p[2]) / 3.f;

									uint local_neighbour_index = (y + 1) * 3 + x + 1;

									// Neighbour edge needs to point to current node to be valid
									uint required_connection = INVALID - (8 - local_neighbour_index);

									// For each side of neighbour triangle
									for (uint ns = 0; ns < 3 && !found; ++ns)
									{
										uint neighbour_connection = neighbour.triangle_connections[border_triangle_index * 3 + ns];
										EdgeComp edge_val = is_same_edge(e1p1, e1p2, test_middle, e2p[ns], e2p[(ns + 1) % 3], neighbour_middle, neighbour_connection);

										if (edge_val == EdgeComp::NO_MATCH)
											continue;

										found = true;

										if (edge_val == EdgeComp::INVALID)
											triangle_valid = false;

										if (neighbour_connection == UNKNOWN)
											neighbour.triangle_connections[border_triangle_index * 3 + ns] = required_connection;

										connections[ss] = INVALID - local_neighbour_index;
										// Set connection in supernode to avoid the connection being overwritten later
										g.triangle_connections[test_triangle * 3 + ss] = INVALID - local_neighbour_index;

										if (HARDCORE_DEBUG)
										{
											cputri::debug_lines.back().push_back(vec3(e1p1) + vec3(0, 106, 0));
											cputri::debug_lines.back().push_back(vec3(e1p2) + vec3(0, 106, 0));
											cputri::debug_lines.back().push_back(vec3(0, 1, 1));
										}
									}
								}
							}
						}
					}

					// If no matching border was found, neighbour triangle should be tested as well
					if (!found)
					{
						uint neighbour = g.triangle_connections[test_triangle * 3 + ss];

						if (neighbour < INVALID - 9)
						{
							triangles_to_add[triangles_to_add_count] = neighbour;
							++triangles_to_add_count;
						}
					}
				}

				if (triangle_valid)
				{
					// Test added triangles
					for (uint ii = 0; ii < triangles_to_add_count; ii++)
					{
						bool already_tested = false;

						for (uint seen = 0; seen < g.seen_triangle_count; seen++)
						{
							if (g.seen_triangles[seen] == triangles_to_add[ii])
							{
								already_tested = true;
								break;
							}
						}

						if (HARDCORE_DEBUG && g.gen_triangle_targets[triangles_to_add[ii]] != INVALID)
						{
							vec3 pn[3] =
							{
								g.positions[g.indices[triangles_to_add[ii] * 3 + 0]],
								g.positions[g.indices[triangles_to_add[ii] * 3 + 1]],
								g.positions[g.indices[triangles_to_add[ii] * 3 + 2]]
							};

							vec3 nm = (pn[0] + pn[1] + pn[2]) / 3.0f;

							cputri::debug_lines.back().push_back(vec3(test_middle) + vec3(0, 110, 0));
							cputri::debug_lines.back().push_back(vec3(nm) + vec3(0, 105, 0));
							cputri::debug_lines.back().push_back(vec3(1, 0.5, 0.5f));
						}

						if (!already_tested)
						{
							g.seen_triangles[g.seen_triangle_count] = triangles_to_add[ii];
							++g.seen_triangle_count;
							g.triangles_to_test[g.test_count] = triangles_to_add[ii];
							++g.test_count;
						}
					}

					if (HARDCORE_DEBUG)
					{
						cputri::debug_lines.back().push_back(vec3(sides[0]) + vec3(0, 104, 0));
						cputri::debug_lines.back().push_back(vec3(sides[1]) + vec3(0, 104, 0));
						cputri::debug_lines.back().push_back(vec3(0, 1, 0));

						cputri::debug_lines.back().push_back(vec3(sides[1]) + vec3(0, 104, 0));
						cputri::debug_lines.back().push_back(vec3(sides[2]) + vec3(0, 104, 0));
						cputri::debug_lines.back().push_back(vec3(0, 1, 0));

						cputri::debug_lines.back().push_back(vec3(sides[2]) + vec3(0, 104, 0));
						cputri::debug_lines.back().push_back(vec3(sides[0]) + vec3(0, 104, 0));
						cputri::debug_lines.back().push_back(vec3(0, 1, 0));
					}

					// Add tested triangle to node
					const uint new_index = node.index_count;
					node.indices[new_index + 0] = get_index_of_point(node, sides[0]);
					node.indices[new_index + 1] = get_index_of_point(node, sides[1]);
					node.indices[new_index + 2] = get_index_of_point(node, sides[2]);
					node.index_count += 3;

					node.triangles[new_index / 3].circumcentre = find_circum_center(
						vec2(sides[0].x, sides[0].z),
						vec2(sides[1].x, sides[1].z),
						vec2(sides[2].x, sides[2].z));
					node.triangles[new_index / 3].circumradius2 = find_circum_radius_squared(
						vec2(sides[0].x, sides[0].z),
						vec2(sides[1].x, sides[1].z),
						vec2(sides[2].x, sides[2].z));

					if (connections[0] != UNKNOWN)
						node.triangle_connections[new_index + 0] = connections[0];
					if (connections[1] != UNKNOWN)
						node.triangle_connections[new_index + 1] = connections[1];
					if (connections[2] != UNKNOWN)
						node.triangle_connections[new_index + 2] = connections[2];

					g.gen_triangle_targets[test_triangle] = gi;
					g.gen_triangle_new_indices[test_triangle] = new_index / 3;
				}
				else
				{
					if (HARDCORE_DEBUG)
					{
						cputri::debug_lines.back().push_back(vec3(sides[0]) + vec3(0, 104, 0));
						cputri::debug_lines.back().push_back(vec3(sides[1]) + vec3(0, 104, 0));
						cputri::debug_lines.back().push_back(vec3(1, 1, 0.5f));

						cputri::debug_lines.back().push_back(vec3(sides[1]) + vec3(0, 104, 0));
						cputri::debug_lines.back().push_back(vec3(sides[2]) + vec3(0, 104, 0));
						cputri::debug_lines.back().push_back(vec3(1, 1, 0.5f));

						cputri::debug_lines.back().push_back(vec3(sides[2]) + vec3(0, 104, 0));
						cputri::debug_lines.back().push_back(vec3(sides[0]) + vec3(0, 104, 0));
						cputri::debug_lines.back().push_back(vec3(1, 1, 0.5f));
					}
				}
			}
		}

		// Reset marked counter
		g.gen_marked_count = 0;

		if (HARDCORE_DEBUG) // Invalid tris
		{
			cputri::debug_lines.push_back(std::vector<vec3>());
		}

		// For every triangle, set its connections in new node
		for (uint tri = 0; tri < g.gen_index_count / 3; ++tri)
		{
			if (g.gen_triangle_targets[tri] != INVALID)
			{
				TerrainData& node = tb->data[gen_info[g.gen_triangle_targets[tri]].index];

				// Triangle index in new node
				uint local_index = g.gen_triangle_new_indices[tri];

				// X/Y pos of node
				int x = gen_info[g.gen_triangle_targets[tri]].x;
				int y = gen_info[g.gen_triangle_targets[tri]].y;

				// For every connection
				for (uint cc = 0; cc < 3; ++cc)
				{
					uint con = g.triangle_connections[tri * 3 + cc];

					if (con >= INVALID - 9)
					{
						if (con == UNKNOWN)
							node.triangle_connections[local_index * 3 + cc] = UNKNOWN;
						else
						{
							// Connection has already been set when searching through neighbour nodes
						}
					}
					// If connected triangle is in the same node
					else if (g.gen_triangle_targets[tri] == g.gen_triangle_targets[con])
					{
						if (g.gen_triangle_targets[tri] >= num_nodes)
							bool a = true;

						node.triangle_connections[local_index * 3 + cc] = g.gen_triangle_new_indices[con];
					}
					// Else point to node of connected triangle
					else if (g.gen_triangle_targets[con] != INVALID)
					{
						// X/Y pos of neighbour node
						int con_x = gen_info[g.gen_triangle_targets[con]].x;
						int con_y = gen_info[g.gen_triangle_targets[con]].y;

						int rel_x = con_x - x;
						int rel_y = con_y - y;

						int neighbour_local_ind = (rel_y + 1) * 3 + rel_x + 1;

						if (neighbour_local_ind < 0 || neighbour_local_ind > 8)
						{
							bool a = false;
						}

						node.triangle_connections[local_index * 3 + cc] = INVALID - neighbour_local_ind;
					}
				}
			}
			else if (HARDCORE_DEBUG)
			{
				vec3 p0 = g.positions[g.indices[tri * 3 + 0]];
				vec3 p1 = g.positions[g.indices[tri * 3 + 1]];
				vec3 p2 = g.positions[g.indices[tri * 3 + 2]];

				cputri::debug_lines.back().push_back(p0);
				cputri::debug_lines.back().push_back(p1);
				cputri::debug_lines.back().push_back(vec3(1, 0, 0));

				cputri::debug_lines.back().push_back(p1);
				cputri::debug_lines.back().push_back(p2);
				cputri::debug_lines.back().push_back(vec3(1, 0, 0));

				cputri::debug_lines.back().push_back(p2);
				cputri::debug_lines.back().push_back(p0);
				cputri::debug_lines.back().push_back(vec3(1, 0, 0));
			}
		}

		if (HARDCORE_DEBUG)	// Inter-node connections
		{
			for (uint ii = 0; ii < 10; ++ii)
			{
				cputri::debug_lines.push_back({});

				for (uint nn = 0; nn < num_nodes; ++nn)
				{
					TerrainData& node = tb->data[gen_info[nn].index];

					for (uint tri = 0; tri < node.index_count / 3; ++tri)
					{
						vec3 p0 = vec3(node.positions[node.indices[tri * 3 + 0]]) + vec3(0, 107, 0);
						vec3 p1 = vec3(node.positions[node.indices[tri * 3 + 1]]) + vec3(0, 107, 0);
						vec3 p2 = vec3(node.positions[node.indices[tri * 3 + 2]]) + vec3(0, 107, 0);

						if (node.triangle_connections[tri * 3 + 0] == INVALID - ii)
						{
							cputri::debug_lines.back().push_back(p0);
							cputri::debug_lines.back().push_back(p1);
							cputri::debug_lines.back().push_back(vec3(0, 1, 0));
						}
						if (node.triangle_connections[tri * 3 + 1] == INVALID - ii)
						{
							cputri::debug_lines.back().push_back(p1);
							cputri::debug_lines.back().push_back(p2);
							cputri::debug_lines.back().push_back(vec3(0, 1, 0));
						}
						if (node.triangle_connections[tri * 3 + 2] == INVALID - ii)
						{
							cputri::debug_lines.back().push_back(p2);
							cputri::debug_lines.back().push_back(p0);
							cputri::debug_lines.back().push_back(vec3(0, 1, 0));
						}
					}
				}
			}
		}

		// For every node, set border count and border indices
		for (uint nn = 0; nn < num_nodes; ++nn)
		{
			TerrainData& node = tb->data[gen_info[nn].index];

			node.border_count = 0;

			for (uint tri = 0; tri < node.index_count / 3; ++tri)
			{
				if (node.triangle_connections[tri * 3 + 0] >= INVALID - 9 ||
					node.triangle_connections[tri * 3 + 1] >= INVALID - 9 ||
					node.triangle_connections[tri * 3 + 2] >= INVALID - 9)
				{
					node.border_triangle_indices[node.border_count] = tri;
					++node.border_count;
				}
			}
		}
	}

	bool valid_node_neighbour(TerrainBuffer* tb, uint cx, uint cy, int x_diff, int y_diff)
	{
		int x = int(cx) + x_diff;
		int y = int(cy) + y_diff;

		if (x >= 0 && y >= 0 && x < (1 << quadtree_levels) && y < (1 << quadtree_levels))
		{
			uint index = tb->quadtree_index_map[y * (1 << quadtree_levels) + x];

			if (index != INVALID && !tb->data[index].is_invalid)
				return true;
		}

		return false;
	}
	void add_supernode_new_point(vec4 p, GlobalData& g)
	{
		for (uint pp = 0; pp < g.gen_new_points_count; pp++)
		{
			if (p == g.new_points[pp])
				return;
		}

		g.new_points[g.gen_new_points_count] = p;
		g.new_points_triangles[g.gen_new_points_count] = 0;
		++g.gen_new_points_count;
	}
}