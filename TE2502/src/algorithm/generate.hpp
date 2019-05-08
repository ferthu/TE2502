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
	const uint max_border_edges = 6000;
	const uint max_triangles_to_remove = 6000;
	const uint num_new_triangle_indices = 1800;
	const uint test_triangle_buffer_size = 3000;

	const uint gen_indices = 20000;
	const uint gen_vertices = 7000;
	const uint gen_border_tris = 1000;
	const uint gen_new_points = 2048;

	struct MarkedTriangle
	{
		// Index of triangle in supernode
		uint gen_index;

		// Index (in gen_info) of node that encountered the triangle
		uint node;
	};

	struct GlobalData
	{
		std::array<GenerateEdge, max_border_edges * 3> generate_edges;

		// Removing triangles
		std::array<uint, max_triangles_to_remove> triangles_to_remove;
		uint triangles_removed = 0;

		uint new_triangle_count;
		uint triangle_count;

		uint valid_indices[max_triangles_to_remove];

		uint new_triangle_indices[num_new_triangle_indices];
		uint new_triangle_index_count;

		// Seen triangle etc
		uint seen_triangles[test_triangle_buffer_size];
		uint seen_triangle_count;
		uint triangles_to_test[test_triangle_buffer_size];
		uint test_count;

		// ------------------------------------------
		// Temporary supernode
		Array<uint, gen_indices> indices;
		Array<vec4, gen_vertices> positions;
		Array<Triangle, gen_indices / 3> triangles;
		Array<uint, gen_indices> triangle_connections;
		Array<uint, gen_border_tris> border_triangle_indices;
		Array<vec4, gen_new_points> new_points;
		Array<uint, gen_new_points> new_points_triangles;
		
		uint gen_index_count;
		uint gen_vertex_count;
		uint gen_new_points_count;
		uint gen_border_count;

		Array<uint, gen_indices / 3> gen_triangle_targets;	// Index into GenerateInfo array specifying the target node for corresponding triangle
		Array<uint, gen_indices / 3> gen_triangle_new_indices;	// Index of corresponding triangle in new node
		Array<uint, num_nodes> gen_starting_points; // gen_starting_points[i] specifies the point to start iteration from in node gen_info[i]
		Array<MarkedTriangle, 6000> gen_marked_triangles;
		uint gen_marked_count;
		// ------------------------------------------
	};


	void remove_old_triangles(TerrainBuffer* tb, GlobalData& g);
	void add_connection(GlobalData& g, cuint connection_index);
	void generate_triangulate_shader(TerrainBuffer* tb, GlobalData& g);
	void remove_marked_triangles(TerrainBuffer* tb, GlobalData& g);
	void remove_marked_triangles2(TerrainBuffer* tb, GlobalData& g, uint node_index);

	int sign(float v);
	void calcLine(vec4 v0, vec4 v1, float& a, float& b, float& c);

	enum class EdgeComp { NO_MATCH, VALID, INVALID };
	EdgeComp is_same_edge(vec4 e1p1, vec4 e1p2, vec3 test_middle, vec4 e2p1, vec4 e2p2, vec3 neighbour_middle, uint neighbour_border_value);

	bool neighbour_exists(uint cx, uint cy, uint local_neighbour_index, TerrainBuffer* tb);

	void generate(TerrainBuffer* tb, GlobalData& g, float* log_filter, cputri::TriData* tri_data, GenerateInfo* gen_info, uint num_nodes);
	bool valid_node_neighbour(TerrainBuffer* tb, uint cx, uint cy, int x_diff, int y_diff);
	
	// Add point to supernode new points list if it does not already exist
	void add_supernode_new_point(vec4 p, GlobalData& g);

	void replace_connection_index(GlobalData& g, cuint triangle_to_check, cuint index_to_replace, cuint new_value);

	void reset_iteration(GlobalData& g);

	// Finds a point in a node and returns it index. The point is added if not found
	uint get_index_of_point(TerrainData& node, vec4 p);
}