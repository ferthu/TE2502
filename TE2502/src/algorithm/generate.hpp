#pragma once

#include "algorithm/common.hpp"
#include "algorithm/cpu_triangulate.hpp"

namespace generate
{
	struct GenerateEdge
	{
		vec4 p1;
		vec4 p2;
		uint p1_index;
		uint p2_index;
		uint connection;
		uint old_triangle_index;
		uint future_index;
		uint pad[3];
	};

	// Constants
	const uint max_border_edges = 100;
	const uint max_triangles_to_remove = 100;
	const uint num_new_triangle_indices = 30;
	const uint test_triangle_buffer_size = 50;

	struct GlobalData
	{
		std::array<GenerateEdge, max_border_edges * 3> generate_edges;

		// Removing triangles
		std::array<uint, max_triangles_to_remove> triangles_to_remove;
		uint triangles_removed = 0;
		uint new_triangle_count;

		uint triangle_count;
		uint vertex_count;

		uint valid_indices[max_triangles_to_remove];

		uint new_triangle_indices[num_new_triangle_indices];
		uint new_triangle_index_count;

		// Seen triangle etc
		uint seen_triangles[test_triangle_buffer_size];
		uint seen_triangle_count;
		uint triangles_to_test[test_triangle_buffer_size];
		uint test_count;
	};


	void remove_old_triangles(TerrainBuffer* tb, GlobalData& g, cuint node_index);
	void add_connection(GlobalData& g, cuint connection_index);
	void generate_triangulate_shader(TerrainBuffer* tb, GlobalData& g, cuint node_index);
	void add_border_point(TerrainBuffer* tb, uint self_node_index, uint other_node_index, vec4 point, bool HARDCORE_DEBUG);
	void remove_marked_triangles(TerrainBuffer* tb, GlobalData& g, uint node_index);
	void remove_marked_triangles2(TerrainBuffer* tb, GlobalData& g, uint node_index);

	int sign(float v);
	void calcLine(vec4 v0, vec4 v1, float& a, float& b, float& c);
	bool is_same_edge(TerrainBuffer* tb, vec4 e1p1, vec4 e1p2, vec3 test_middle, vec4 e2p1, vec4 e2p2, vec3 neighbour_middle, uint neighbour_node_index, uint neighbour_border_index, uint connection_value, bool& valid);
	bool neighbour_exists(uint cx, uint cy, uint local_neighbour_index, TerrainBuffer* tb);

	void generate(TerrainBuffer* tb, GlobalData& g, float* log_filter, uint node_index, vec2 min, vec2 max, cputri::TriData* tri_data);
}