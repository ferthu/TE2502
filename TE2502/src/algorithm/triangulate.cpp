#define NOMINMAX
#include "algorithm/triangulate.hpp"

namespace triangulate
{
	void remove_old_triangles(TerrainBuffer* tb, GlobalData& g)
	{
		// Remove old triangles
		for (int j = int(g.triangles_removed) - 1; j >= 0; --j)
		{
			const uint index = g.triangles_to_remove[j];
			const uint global_node_index = g.ltg[g.owning_node[j]];

			const uint last_triangle = tb->data[global_node_index].index_count / 3 - 1;

			tb->data[global_node_index].lowest_indices_index_changed = min(tb->data[global_node_index].lowest_indices_index_changed, index * 3);

			// Loop through remaining triangles to remove and update any that are equal to last_triangle
			for (int ii = 0; ii < j; ++ii)
			{
				if (g.triangles_to_remove[ii] == last_triangle && g.owning_node[j] == g.owning_node[ii])
				{
					g.triangles_to_remove[ii] = index;
					break;
				}
			}

			for (uint ii = 0; ii < 3; ++ii)
			{
				replace_connection_index(tb, global_node_index, tb->data[global_node_index].triangle_connections[last_triangle * 3 + ii], last_triangle, index);
			}

			// Fix border indices
			for (uint ss = 0; ss < 3; ++ss)
			{
				if (tb->data[global_node_index].triangle_connections[index * 3 + ss] >= INVALID - 9 ||
					tb->data[global_node_index].triangle_connections[last_triangle * 3 + ss] >= INVALID - 9)
				{
					uint count = tb->data[global_node_index].border_count;
					for (uint tt = 0; tt < count; ++tt)
					{
						if (tb->data[global_node_index].border_triangle_indices[tt] == index)
						{
							tb->data[global_node_index].border_triangle_indices[tt] = tb->data[global_node_index].border_triangle_indices[count - 1];
							--tb->data[global_node_index].border_count;
							--count;
						}
						if (tb->data[global_node_index].border_triangle_indices[tt] == last_triangle)
						{
							tb->data[global_node_index].border_triangle_indices[tt] = index;
						}
					}
					break;
				}
			}

			// Remove triangle
			if (index < last_triangle)
			{
				tb->data[global_node_index].indices[index * 3 + 0] = tb->data[global_node_index].indices[last_triangle * 3 + 0];
				tb->data[global_node_index].indices[index * 3 + 1] = tb->data[global_node_index].indices[last_triangle * 3 + 1];
				tb->data[global_node_index].indices[index * 3 + 2] = tb->data[global_node_index].indices[last_triangle * 3 + 2];
				tb->data[global_node_index].triangles[index].circumcentre = tb->data[global_node_index].triangles[last_triangle].circumcentre;
				tb->data[global_node_index].triangles[index].circumradius2 = tb->data[global_node_index].triangles[last_triangle].circumradius2;
				tb->data[global_node_index].triangle_connections[index * 3 + 0] = tb->data[global_node_index].triangle_connections[last_triangle * 3 + 0];
				tb->data[global_node_index].triangle_connections[index * 3 + 1] = tb->data[global_node_index].triangle_connections[last_triangle * 3 + 1];
				tb->data[global_node_index].triangle_connections[index * 3 + 2] = tb->data[global_node_index].triangle_connections[last_triangle * 3 + 2];

				for (uint tt = 0; tt < g.new_triangle_index_count[g.owning_node[j]]; ++tt)
				{
					const uint triangle_index = g.new_triangle_indices[g.owning_node[j] * num_new_triangle_indices + tt];
					if (triangle_index == last_triangle)
						g.new_triangle_indices[g.owning_node[j] * num_new_triangle_indices + tt] = index;
				}
			}

			tb->data[global_node_index].index_count -= 3;

			// Update the rest of the new points' triangle index after updating triangles in node
			for (uint ii = 0; ii < tb->data[global_node_index].new_points_count; ++ii)
			{
				if (tb->data[global_node_index].new_points_triangles[ii] == index)
				{
					// Look through all newly added triangles only
					for (uint tt = 0; tt < g.new_triangle_index_count[g.owning_node[j]]; ++tt)
					{
						const uint triangle_index = g.new_triangle_indices[g.owning_node[j] * num_new_triangle_indices + tt];
						const vec4 new_point = tb->data[global_node_index].new_points[ii];
						const vec2 circumcentre = tb->data[global_node_index].triangles[triangle_index].circumcentre;
						const float circumradius2 = tb->data[global_node_index].triangles[triangle_index].circumradius2;

						const float dx = new_point.x - circumcentre.x;
						const float dy = new_point.z - circumcentre.y;

						// Find the first triangle whose cc contains the point
						if (dx * dx + dy * dy < circumradius2)
						{
							tb->data[global_node_index].new_points_triangles[ii] = triangle_index;
							break;
						}
					}
				}
				else if (tb->data[global_node_index].new_points_triangles[ii] == last_triangle)
				{
					tb->data[global_node_index].new_points_triangles[ii] = index;
				}
			}
		}
	}

	void add_connection(GlobalData& g, cuint local_node_index, cuint connection_index)
	{
		// Check if it has already been seen
		for (uint ii = 0; ii < g.seen_triangle_count; ++ii)
		{
			if (local_node_index == g.seen_triangle_owners[ii] && connection_index == g.seen_triangles[ii])
			{
				return;
			}
		}

		g.seen_triangles[g.seen_triangle_count] = connection_index;
		g.seen_triangle_owners[g.seen_triangle_count] = local_node_index;
		++g.seen_triangle_count;
		g.triangles_to_test[g.test_count] = connection_index;
		g.test_triangle_owners[g.test_count] = local_node_index;
		++g.test_count;
	}


	void triangulate(TerrainBuffer* tb, GlobalData& g, cuint node_index, cputri::TriData* tri_data)
	{
		if (tri_data->refine_node != -1 && tri_data->refine_node != node_index)
			return;

		const uint new_points_count = tb->data[node_index].new_points_count;
		if (new_points_count == 0)
			return;

		const vec2 node_min = tb->data[node_index].min;
		const vec2 node_max = tb->data[node_index].max;
		const float side = node_max.x - node_min.x;

		const int nodes_per_side = 1 << quadtree_levels;

		const int cx = int((node_min.x - tb->quadtree_min.x + 1) / side);  // current node x
		const int cy = int((node_min.y - tb->quadtree_min.y + 1) / side);  // current node z/y

		uint nodes_new_points_count[9];

		// Setup ltg
		for (int y = -1; y <= 1; ++y)
		{
			for (int x = -1; x <= 1; ++x)
			{
				const int nx = cx + x;
				const int ny = cy + y;
				if (nx >= 0 && nx < nodes_per_side && ny >= 0 && ny < nodes_per_side)
				{
					g.ltg[(y + 1) * 3 + x + 1] = tb->quadtree_index_map[ny * nodes_per_side + nx];
				}
				else
				{
					g.ltg[(y + 1) * 3 + x + 1] = INVALID;
				}
			}
		}

		g.triangles_removed = 0;

		uint counter = 0;
		for (int n = (int)new_points_count - 1; n >= 0 && counter < (uint)tri_data->refine_vertices; --n, ++counter)
		//for (uint n = 0; n < new_points_count && n < TERRAIN_GENERATE_NUM_VERTICES; ++n)
		{
			const vec4 current_point = tb->data[node_index].new_points[n];

			// Reset
			for (uint ii = 0; ii < 9; ++ii)
			{
				nodes_new_points_count[ii] = 0;
				g.new_triangle_index_count[ii] = 0;
			}

			g.seen_triangle_count = 1;
			g.test_count = 1;
			bool checked_borders = false;

			const uint start_index = tb->data[node_index].new_points_triangles[n];
			g.seen_triangles[0] = start_index;
			g.seen_triangle_owners[0] = SELF_INDEX;
			g.triangles_to_test[0] = start_index;
			g.test_triangle_owners[0] = SELF_INDEX;

			bool finish = false;
			while (g.test_count != 0 && !finish)
			{
				const uint triangle_index = g.triangles_to_test[--g.test_count];
				const uint local_owner_index = g.test_triangle_owners[g.test_count];
				const uint global_owner_index = g.ltg[local_owner_index];
				const vec2 circumcentre = tb->data[global_owner_index].triangles[triangle_index].circumcentre;
				const float circumradius2 = tb->data[global_owner_index].triangles[triangle_index].circumradius2;

				const float dx = current_point.x - circumcentre.x;
				const float dy = current_point.z - circumcentre.y;

				if (dx * dx + dy * dy < circumradius2)
				{
					// Add triangle edges to edge buffer
					const uint index0 = tb->data[global_owner_index].indices[triangle_index * 3 + 0];
					const uint index1 = tb->data[global_owner_index].indices[triangle_index * 3 + 1];
					const uint index2 = tb->data[global_owner_index].indices[triangle_index * 3 + 2];
					const vec4 p0 = tb->data[global_owner_index].positions[index0];
					const vec4 p1 = tb->data[global_owner_index].positions[index1];
					const vec4 p2 = tb->data[global_owner_index].positions[index2];

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
					g.edges[ec + 0].p1 = biggest_point ? p0 : p1;
					g.edges[ec + 0].p2 = !biggest_point ? p0 : p1;
					g.edges[ec + 0].p1_index = biggest_point ? index0 : index1;
					g.edges[ec + 0].p2_index = !biggest_point ? index0 : index1;
					g.edges[ec + 0].node_index = local_owner_index;
					g.edges[ec + 0].connection = tb->data[global_owner_index].triangle_connections[triangle_index * 3 + 0];
					g.edges[ec + 0].old_triangle_index = triangle_index;
					// Edge 1
					biggest_point = p1.y < p2.y;
					g.edges[ec + 1].p1 = biggest_point ? p1 : p2;
					g.edges[ec + 1].p2 = !biggest_point ? p1 : p2;
					g.edges[ec + 1].p1_index = biggest_point ? index1 : index2;
					g.edges[ec + 1].p2_index = !biggest_point ? index1 : index2;
					g.edges[ec + 1].node_index = local_owner_index;
					g.edges[ec + 1].connection = tb->data[global_owner_index].triangle_connections[triangle_index * 3 + 1];
					g.edges[ec + 1].old_triangle_index = triangle_index;
					// Edge 2
					biggest_point = p2.y < p0.y;
					g.edges[ec + 2].p1 = biggest_point ? p2 : p0;
					g.edges[ec + 2].p2 = !biggest_point ? p2 : p0;
					g.edges[ec + 2].p1_index = biggest_point ? index2 : index0;
					g.edges[ec + 2].p2_index = !biggest_point ? index2 : index0;
					g.edges[ec + 2].node_index = local_owner_index;
					g.edges[ec + 2].connection = tb->data[global_owner_index].triangle_connections[triangle_index * 3 + 2];
					g.edges[ec + 2].old_triangle_index = triangle_index;

					// Mark the triangle to be removed later
					g.triangles_to_remove[tr] = triangle_index;
					g.owning_node[tr] = local_owner_index;

					// Add neighbour triangles to be tested
					for (uint ss = 0; ss < 3 && !finish; ++ss)
					{
						const uint index = tb->data[global_owner_index].triangle_connections[triangle_index * 3 + ss];

						if (index <= INVALID - 9)
						{
							if (g.seen_triangle_count >= test_triangle_buffer_size || g.test_count >= test_triangle_buffer_size)
							{
								finish = true;
								break;
							}

							add_connection(g, local_owner_index, index);
						}
						else if (!checked_borders)
						{
							checked_borders = true;
							// Check the internal border triangles
							uint node = g.ltg[SELF_INDEX];
							const uint triangle_count = tb->data[node].border_count;
							for (uint tt = 0; tt < triangle_count; ++tt)
							{
								const uint border_triangle = tb->data[node].border_triangle_indices[tt];
								const vec2 cc = tb->data[node].triangles[border_triangle].circumcentre;
								const float cr2 = tb->data[node].triangles[border_triangle].circumradius2;

								const float ddx = current_point.x - cc.x;
								const float ddy = current_point.z - cc.y;

								if (ddx * ddx + ddy * ddy < cr2)
								{
									if (g.seen_triangle_count >= test_triangle_buffer_size || g.test_count >= test_triangle_buffer_size)
									{
										finish = true;
										break;
									}

									add_connection(g, SELF_INDEX, border_triangle);
								}
							}

							// Check neighbour nodes
							for (uint nn = 0; nn < 9 && !finish; ++nn)
							{
								if (nn != SELF_INDEX)
								{
									const vec2 adjusted_max = node_max + vec2(side) * ADJUST_PERCENTAGE;
									const vec2 adjusted_min = node_min - vec2(side) * ADJUST_PERCENTAGE;
									if (current_point.x >= adjusted_min.x && current_point.x <= adjusted_max.x
										&& current_point.z >= adjusted_min.y && current_point.z <= adjusted_max.y)
									{
										const uint node = g.ltg[nn];
										if (node != INVALID)
										{
											const uint triangle_count = tb->data[node].border_count;
											for (uint tt = 0; tt < triangle_count; ++tt)
											{
												const uint border_triangle = tb->data[node].border_triangle_indices[tt];
												const vec2 cc = tb->data[node].triangles[border_triangle].circumcentre;
												const float cr2 = tb->data[node].triangles[border_triangle].circumradius2;

												const float ddx = current_point.x - cc.x;
												const float ddy = current_point.z - cc.y;

												if (ddx * ddx + ddy * ddy < cr2)
												{
													if (g.seen_triangle_count >= test_triangle_buffer_size || g.test_count >= test_triangle_buffer_size)
													{
														finish = true;
														break;
													}

													add_connection(g, nn, border_triangle);
												}
											}
										}
									}
								}
							}
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
						g.edges[i].p1 == g.edges[j].p1 &&
						g.edges[i].p2 == g.edges[j].p2)
					{
						// Mark as invalid
						g.edges[j].p1.y = INVALID_HEIGHT;
						found = true;
					}
				}
				if (found)
					g.edges[i].p1.y = INVALID_HEIGHT;
			}

			// Count the number of new triangles to create
			g.new_triangle_count = 0;
			for (uint j = 0; j < edge_count && j < max_triangles_to_remove * 3; ++j)
			{
				if (g.edges[j].p1.y != INVALID_HEIGHT)
				{
					g.valid_indices[g.new_triangle_count++] = j;
					nodes_new_points_count[g.edges[j].node_index]++;
				}
			}


			struct moved_point
			{
				vec4 point;
				uint index;			// Index of point in new node
				uint node_index;	// Local index of node that the point was placed in
			};

			const uint MAX_MOVED_POINTS = 10;

			// Array of points moved into other nodes
			std::array<moved_point, MAX_MOVED_POINTS> moved_points;
			uint moved_points_count = 0;

			std::array<uint, 9> participating_nodes;
			uint participation_count = 0;

			// True if this point should be skipped due to an array being full
			bool skip = false;

			for (uint edge = 0; edge < g.new_triangle_count; ++edge)
			{
				// Calculate participating nodes
				bool found = false;
				for (uint jj = 0; jj < participation_count; ++jj)
				{
					if (g.edges[g.valid_indices[edge]].node_index == participating_nodes[jj])
					{
						found = true;
						break;
					}
				}
				if (!found)
				{
					participating_nodes[participation_count] = g.edges[g.valid_indices[edge]].node_index;

					++participation_count;
				}
			}

			for (uint pp = 0; pp < 9; ++pp)
			{
				if (g.ltg[pp] != INVALID)
				{
					int offset = 0;
					if (pp == 4)
						offset = 500;

					if (tb->data[g.ltg[pp]].index_count + g.new_triangle_count * 3 >= num_indices - offset ||
						tb->data[g.ltg[pp]].vertex_count + g.new_triangle_count * 2 >= num_vertices - offset ||
						tb->data[g.ltg[pp]].border_count + g.new_triangle_count >= max_border_triangle_count - offset)
					{
						skip = true;
						break;
					}
				}
			}

			if (skip)
				break;

			// Move triangles to correct node
			for (uint edge = 0; edge < g.new_triangle_count; ++edge)
			{
				uint i = g.valid_indices[edge];
				vec3 p0 = vec3(g.edges[i].p1);
				vec3 p1 = vec3(g.edges[i].p2);
				vec3 p2 = vec3(current_point);

				// Check if triangle is in another node
				const vec3 triangle_mid = (vec3(p0) + vec3(p1) + vec3(p2)) / 3.0f;

				bool move_triangle = false;

				uint x_index = 1;
				uint y_index = 1;

				if (triangle_mid.x > node_max.x)
					x_index = 2;
				else if (triangle_mid.x < node_min.x)
					x_index = 0;
				if (triangle_mid.z > node_max.y)
					y_index = 2;
				else if (triangle_mid.z < node_min.y)
					y_index = 0;

				uint local_node_index = y_index * 3 + x_index;

				if (local_node_index != g.edges[i].node_index && g.ltg[local_node_index] != INVALID &&
					moved_points_count < MAX_MOVED_POINTS - 2)
				{
					move_triangle = true;

					bool found = false;
					// Add target node to participating nodes if required
					for (uint jj = 0; jj < participation_count; ++jj)
					{
						if (participating_nodes[jj] == local_node_index)
							found = true;
					}

					if (!found)
					{
						participating_nodes[participation_count] = local_node_index;

						++participation_count;
					}
				}

				uint old_old_triangle_index = INVALID;
				uint old_node_index = INVALID;

				if (move_triangle)
				{
					old_old_triangle_index = g.edges[i].old_triangle_index;
					old_node_index = g.edges[i].node_index;

					g.edges[i].old_triangle_index = INVALID;
					g.edges[i].node_index = local_node_index;
				}

				g.edges[i].future_index = tb->data[g.ltg[g.edges[i].node_index]].index_count / 3;
				tb->data[g.ltg[g.edges[i].node_index]].index_count += 3;

				if (move_triangle)
				{
					bool is_border = false;

					if (g.edges[i].connection <= INVALID - 9)
					{
						// Check if old neighbour is a border triangle
						for (uint border = 0; border < 3; ++border)
						{
							if (tb->data[g.ltg[old_node_index]].triangle_connections[g.edges[i].connection * 3 + border] >= INVALID - 9)
							{
								is_border = true;
								break;
							}
						}

						// Make old neighbour triangle a border triangle if it is not already
						if (!is_border)
						{
							tb->data[g.ltg[old_node_index]].border_triangle_indices[tb->data[g.ltg[old_node_index]].border_count] = g.edges[i].connection;
							++tb->data[g.ltg[old_node_index]].border_count;
						}
					}

					// Remove connection from old neighbour
					replace_connection_index(tb, g.ltg[old_node_index], g.edges[i].connection, old_old_triangle_index, INVALID - (4 + (int)g.edges[i].node_index - (int)old_node_index));

					uint border_count = tb->data[g.ltg[g.edges[i].node_index]].border_count;

					bool connected = false;

					// Loop through border triangles of target node to find connection
					for (uint border_tri = 0; border_tri < border_count; ++border_tri)
					{
						const uint border_index = tb->data[g.ltg[g.edges[i].node_index]].border_triangle_indices[border_tri];

						uint inds[3];
						inds[0] = tb->data[g.ltg[g.edges[i].node_index]].indices[border_index * 3 + 0];
						inds[1] = tb->data[g.ltg[g.edges[i].node_index]].indices[border_index * 3 + 1];
						inds[2] = tb->data[g.ltg[g.edges[i].node_index]].indices[border_index * 3 + 2];

						vec3 p[3];
						p[0] = tb->data[g.ltg[g.edges[i].node_index]].positions[inds[0]];
						p[1] = tb->data[g.ltg[g.edges[i].node_index]].positions[inds[1]];
						p[2] = tb->data[g.ltg[g.edges[i].node_index]].positions[inds[2]];

						// For every edge in border triangle
						for (uint bb = 0; bb < 3; ++bb)
						{
							if (p[bb] == vec3(g.edges[i].p1) && p[(bb + 1) % 3] == vec3(g.edges[i].p2) ||
								p[bb] == vec3(g.edges[i].p2) && p[(bb + 1) % 3] == vec3(g.edges[i].p1))
							{
								// Set connection
								g.edges[i].connection = border_index;
								tb->data[g.ltg[g.edges[i].node_index]].triangle_connections[border_index * 3 + bb] = g.edges[i].future_index;

								// Set indices
								if (p[bb] == vec3(g.edges[i].p1))
								{
									g.edges[i].p1_index = inds[bb];
									g.edges[i].p2_index = inds[(bb + 1) % 3];
								}
								else
								{
									g.edges[i].p2_index = inds[bb];
									g.edges[i].p1_index = inds[(bb + 1) % 3];
								}

								// Check if neighbour triangle is still a border triangle
								bool border_triangle = false;
								for (uint cc = 0; cc < 3; ++cc)
								{
									if (tb->data[g.ltg[g.edges[i].node_index]].triangle_connections[border_index * 3 + cc] >= INVALID - 9)
									{
										border_triangle = true;
										break;
									}
								}

								// If neighbour is not a border triangle anymore, remove it from border triangle list
								if (!border_triangle)
								{
									tb->data[g.ltg[g.edges[i].node_index]].border_triangle_indices[border_tri] =
										tb->data[g.ltg[g.edges[i].node_index]].border_triangle_indices[tb->data[g.ltg[g.edges[i].node_index]].border_count - 1];
									--tb->data[g.ltg[g.edges[i].node_index]].border_count;
									--border_count;
								}

								connected = true;
								break;
							}
						}
					}

					// If the triangle did not connect to an existing border triangle, points need to be transferred to the target node
					if (!connected)
					{
						bool p1_found = false;
						bool p2_found = false;

						g.edges[i].connection = INVALID - (4 + (int)old_node_index - (int)g.edges[i].node_index);

						// Check moved_points for a match, use that index if found
						for (uint mp = 0; mp < moved_points_count && (!p1_found || !p2_found); ++mp)
						{
							if (!p1_found && g.edges[i].p1 == moved_points[mp].point)
							{
								p1_found = true;
								g.edges[i].p1_index = moved_points[mp].index;
							}
							else if (!p2_found && g.edges[i].p2 == moved_points[mp].point)
							{
								p2_found = true;
								g.edges[i].p2_index = moved_points[mp].index;
							}
						}

						if (!p1_found || !p2_found)
						{
							// For every border triangle, set indices of points already within the node
							for (uint border_tri = 0; border_tri < border_count && (!p1_found || !p2_found); ++border_tri)
							{
								const uint border_index = tb->data[g.ltg[g.edges[i].node_index]].border_triangle_indices[border_tri];

								uint inds[3];
								inds[0] = tb->data[g.ltg[g.edges[i].node_index]].indices[border_index * 3 + 0];
								inds[1] = tb->data[g.ltg[g.edges[i].node_index]].indices[border_index * 3 + 1];
								inds[2] = tb->data[g.ltg[g.edges[i].node_index]].indices[border_index * 3 + 2];

								// Vertices of border triangle
								vec4 p[3];
								p[0] = tb->data[g.ltg[g.edges[i].node_index]].positions[inds[0]];
								p[1] = tb->data[g.ltg[g.edges[i].node_index]].positions[inds[1]];
								p[2] = tb->data[g.ltg[g.edges[i].node_index]].positions[inds[2]];

								for (uint bb = 0; bb < 3 && (!p1_found || !p2_found); ++bb)
								{
									if (!p1_found && p[bb] == g.edges[i].p1)
									{
										p1_found = true;

										g.edges[i].p1_index = inds[bb];
									}
									else if (!p2_found && p[bb] == g.edges[i].p2)
									{
										p2_found = true;

										g.edges[i].p2_index = inds[bb];
									}
								}
							}

							// If the points are not within the node, add them
							if (!p1_found)
							{
								moved_points[moved_points_count].node_index = g.edges[i].node_index;
								moved_points[moved_points_count].point = g.edges[i].p1;
								moved_points[moved_points_count].index = tb->data[g.ltg[g.edges[i].node_index]].vertex_count;

								g.edges[i].p1_index = moved_points[moved_points_count].index;

								tb->data[g.ltg[g.edges[i].node_index]].positions[tb->data[g.ltg[g.edges[i].node_index]].vertex_count] = g.edges[i].p1;

								++tb->data[g.ltg[g.edges[i].node_index]].vertex_count;
								++moved_points_count;
							}
							if (!p2_found)
							{
								moved_points[moved_points_count].node_index = g.edges[i].node_index;
								moved_points[moved_points_count].point = g.edges[i].p2;
								moved_points[moved_points_count].index = tb->data[g.ltg[g.edges[i].node_index]].vertex_count;

								g.edges[i].p2_index = moved_points[moved_points_count].index;

								tb->data[g.ltg[g.edges[i].node_index]].positions[tb->data[g.ltg[g.edges[i].node_index]].vertex_count] = g.edges[i].p2;

								++tb->data[g.ltg[g.edges[i].node_index]].vertex_count;
								++moved_points_count;
							}
						}
					}
				}
			}


			// Add to the triangle list all triangles formed between the point and the edges of the enclosing polygon
			for (uint ii = 0; ii < g.new_triangle_count; ++ii)
			{
				uint i = g.valid_indices[ii];
				vec3 P = vec3(g.edges[i].p1);
				vec3 Q = vec3(g.edges[i].p2);
				vec3 R = vec3(current_point);

				// Make sure winding order is correct
				const vec3 nor = cross(R - P, Q - P);
				if (nor.y > 0)
				{
					vec4 temp = g.edges[i].p1;
					g.edges[i].p1 = g.edges[i].p2;
					g.edges[i].p2 = temp;
					uint temp2 = g.edges[i].p1_index;
					g.edges[i].p1_index = g.edges[i].p2_index;
					g.edges[i].p2_index = temp2;
				}

				// Set indices for the new triangle
				const uint index = g.edges[i].future_index * 3;
				tb->data[g.ltg[g.edges[i].node_index]].indices[index + 0] = g.edges[i].p1_index;
				tb->data[g.ltg[g.edges[i].node_index]].indices[index + 1] = g.edges[i].p2_index;
				tb->data[g.ltg[g.edges[i].node_index]].indices[index + 2] = tb->data[g.ltg[g.edges[i].node_index]].vertex_count;

				const uint triangle_count = g.edges[i].future_index;
				g.new_triangle_indices[g.edges[i].node_index * num_new_triangle_indices + g.new_triangle_index_count[g.edges[i].node_index]] = triangle_count;
				++g.new_triangle_index_count[g.edges[i].node_index];

				// Set circumcircles for the new triangle
				float a = distance(vec2(P.x, P.z), vec2(Q.x, Q.z));
				float b = distance(vec2(P.x, P.z), vec2(R.x, R.z));
				float c = distance(vec2(R.x, R.z), vec2(Q.x, Q.z));

				const vec2 cc_center = find_circum_center(vec2(P.x, P.z), vec2(Q.x, Q.z), vec2(R.x, R.z));
				const float cc_radius2 = find_circum_radius_squared(a, b, c);
				const float cc_radius = sqrt(cc_radius2);

				tb->data[g.ltg[g.edges[i].node_index]].triangles[triangle_count].circumcentre = cc_center;
				tb->data[g.ltg[g.edges[i].node_index]].triangles[triangle_count].circumradius2 = cc_radius2;

				// Connections
				tb->data[g.ltg[g.edges[i].node_index]].triangle_connections[index + 0] = g.edges[i].connection;
				const vec4 edges[2] = { g.edges[i].p1, g.edges[i].p2 };
				bool already_added = false;
				if (g.edges[i].connection >= INVALID - 9 && tb->data[g.ltg[g.edges[i].node_index]].border_count < MAX_BORDER_TRIANGLE_COUNT)
				{
					already_added = true;
					tb->data[g.ltg[g.edges[i].node_index]].border_triangle_indices[tb->data[g.ltg[g.edges[i].node_index]].border_count] = g.edges[i].future_index;
					++tb->data[g.ltg[g.edges[i].node_index]].border_count;
				}

				for (uint ss = 0; ss < 2; ++ss)  // The two other sides
				{
					bool is_border = false;
					// Search through all other new triangles that have been added to find possible neighbours/connections
					for (uint ee = 0; ee < g.new_triangle_count; ++ee)
					{
						uint test_index = g.valid_indices[ee];
						if (test_index == i)
							continue;
						// Check each pair of points in the triangle if they match
						if (edges[ss] == g.edges[test_index].p1 || edges[ss] == g.edges[test_index].p2)
						{
							if (g.edges[i].node_index == g.edges[test_index].node_index)
								tb->data[g.ltg[g.edges[i].node_index]].triangle_connections[index + 2 - ss] = g.edges[test_index].future_index;
							else
							{
								tb->data[g.ltg[g.edges[i].node_index]].triangle_connections[index + 2 - ss] = INVALID - (4 + (int)g.edges[test_index].node_index - (int)g.edges[i].node_index);
								is_border = true;
							}
							break;
						}
					}

					if (is_border && !already_added && tb->data[g.ltg[g.edges[i].node_index]].border_count < MAX_BORDER_TRIANGLE_COUNT)
					{
						already_added = true;
						tb->data[g.ltg[g.edges[i].node_index]].border_triangle_indices[tb->data[g.ltg[g.edges[i].node_index]].border_count++] = g.edges[i].future_index;
					}
				}

				if (g.edges[i].old_triangle_index != INVALID)
					replace_connection_index(tb, g.ltg[g.edges[i].node_index], g.edges[i].connection, g.edges[i].old_triangle_index, g.edges[i].future_index);
			}

			remove_old_triangles(tb, g);

			// Insert new point
			for (uint jj = 0; jj < participation_count; ++jj)
			{
				tb->data[g.ltg[participating_nodes[jj]]].positions[tb->data[g.ltg[participating_nodes[jj]]].vertex_count] = current_point;
				++tb->data[g.ltg[participating_nodes[jj]]].vertex_count;
				tb->data[g.ltg[participating_nodes[jj]]].has_data_to_copy = true;
			}

			g.triangles_removed = 0;
		}

		//tb->data[node_index].new_points_count = 0;
		tb->data[node_index].new_points_count -= std::min((uint)tri_data->refine_vertices, new_points_count);
	}
}
