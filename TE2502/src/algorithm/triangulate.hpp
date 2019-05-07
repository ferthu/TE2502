#pragma once

#include "algorithm/common.hpp"
#include "algorithm/cpu_triangulate.hpp"

namespace triangulate
{
	struct BorderEdge
	{
		vec4 p1;
		vec4 p2;
		uint p1_index;
		uint p2_index;
		uint node_index; // Local node format
		uint connection;
		uint old_triangle_index;
		uint future_index;
		uint pad[2];
	};

	// Constants
	const uint max_border_edges = 100;
	const uint max_triangles_to_remove = 100;
	const uint num_new_triangle_indices = 30;
	const uint test_triangle_buffer_size = 50;

#define EPSILON 1.0f - 0.0001f
#define SELF_INDEX 4

	const float INVALID_HEIGHT = 10000.0f;

	struct GlobalData
	{
		std::array<BorderEdge, max_border_edges * 3> edges;

		// Removing triangles
		std::array<uint, max_triangles_to_remove> triangles_to_remove;
		std::array<uint, max_triangles_to_remove> owning_node;
		uint triangles_removed = 0;
		uint new_triangle_count;

		uint valid_indices[max_triangles_to_remove];

		// Convert local node format to global
		uint ltg[9];

		// Seen triangle etc
		uint seen_triangles[test_triangle_buffer_size];
		uint seen_triangle_count;
		uint triangles_to_test[test_triangle_buffer_size];
		uint test_count;
		uint seen_triangle_owners[test_triangle_buffer_size]; // Local node format
		uint test_triangle_owners[test_triangle_buffer_size]; // Local node format

		uint new_triangle_indices[9 * num_new_triangle_indices];
		uint new_triangle_index_count[9];
	};


	void remove_old_triangles(TerrainBuffer* tb, GlobalData& g);
	void add_connection(GlobalData& g, cuint local_node_index, cuint connection_index);
	void triangulate(Quadtree& quadtree, TerrainBuffer* tb, GlobalData& g, cuint node_index, cputri::TriData* tri_data);
}