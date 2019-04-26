#include "vulkan/vulkan.h"
#include "cpu_triangulate.hpp"
#include "graphics/window.hpp"
#include "utilities.hpp"

#include "algorithm/generate.hpp"
#include "algorithm/process.hpp"
#include "algorithm/triangulate.hpp"
#include "imgui/imgui.h"

// Fritjof when coding in this file:
//    ,,,,,
//   d T^T b
//    \___/ 
//      |    n
//     /|\  |||b
//    / | \  /
//   /  |  \/
//   |  |
//   0 / \ 
//    /   \ 
//   /     \
//   |     |
//   |     |
//  _|     |_

namespace cputri
{
	using namespace glm;

	int max_points_per_refine = 9999999;
	int vistris_start = 0;
	int vistris_end = 99999999;
	bool show_cc = false;
	bool show = false;

	int temp = 0;
	int vertices_per_refine = 1;
	int show_connections = -1;
	int refine_node = -1;
	int sideshow_bob = 0;
	generate::GlobalData gg;
	triangulate::GlobalData tg;

	uint cpu_index_buffer_size;

	TerrainBuffer* tb;
	Quadtree quadtree;

	float log_filter[filter_side * filter_side];

	void setup(TFile& tfile)
	{
		assert(quadtree_levels > 0);

		quadtree.buffer_index_filled = new bool[num_nodes];
		memset(quadtree.buffer_index_filled, 0, num_nodes * sizeof(bool));

		quadtree.num_generate_nodes = 0;
		quadtree.generate_nodes = new GenerateInfo[num_nodes];

		quadtree.num_draw_nodes = 0;
		quadtree.draw_nodes = new uint[num_nodes];

		quadtree.node_memory_size =
			sizeof(VkDrawIndexedIndirectCommand) +
			sizeof(BufferNodeHeader) +
			num_indices * sizeof(uint) + // Indices
			num_vertices * sizeof(vec4) + // Vertices
			(num_indices / 3) * sizeof(Triangle) + // Circumcentre and circumradius
			num_indices * sizeof(uint) + // Triangle connectivity
			num_new_points * sizeof(vec4) + // New points
			num_new_points * sizeof(uint); // New point triangle index

		// Add space for an additional two vec2's to store the quadtree min and max
		cpu_index_buffer_size = (1 << quadtree_levels) * (1 << quadtree_levels) * sizeof(uint) + sizeof(vec2) * 2;
		cpu_index_buffer_size += 64 - (cpu_index_buffer_size % 64);

		tb = (TerrainBuffer*) new char[cpu_index_buffer_size + quadtree.node_memory_size * num_nodes + 1000];

		quadtree.node_index_to_buffer_index = (uint*)tb;

		// Point to the end of cpu index buffer
		quadtree.quadtree_minmax = (vec2*) (((char*)quadtree.node_index_to_buffer_index) + (1 << quadtree_levels) * (1 << quadtree_levels) * sizeof(uint));

		quadtree.total_side_length = TERRAIN_GENERATE_TOTAL_SIDE_LENGTH;
		float half_length = quadtree.total_side_length * 0.5f;
		quadtree.quadtree_minmax[0] = vec2(-half_length, -half_length);
		quadtree.quadtree_minmax[1] = vec2(half_length, half_length);

		// (1 << levels) is number of nodes per axis
		memset(quadtree.node_index_to_buffer_index, INVALID, (1 << quadtree_levels) * (1 << quadtree_levels) * sizeof(uint));

		quadtree.node_size = vec2(quadtree.total_side_length / (1 << quadtree_levels), quadtree.total_side_length / (1 << quadtree_levels));

		// Create filter kernel
		const float gaussian_width = 1.0f;

		float sum = 0.0f;

		for (int64_t x = -filter_radius; x <= filter_radius; x++)
		{
			for (int64_t y = -filter_radius; y <= filter_radius; y++)
			{
				// https://homepages.inf.ed.ac.uk/rbf/HIPR2/log.htm
				float t = -((x * x + y * y) / (2.0f * gaussian_width * gaussian_width));
				float log = -(1.0f / (pi<float>() * powf(gaussian_width, 4.0f))) * (1.0f + t) * exp(t);

				log_filter[(y + filter_radius) * filter_side + (x + filter_radius)] = log;
				sum += log;
			}
		}

		// Normalize filter
		float correction = 1.0f / sum;
		for (uint64_t i = 0; i < filter_side * filter_side; i++)
		{
			log_filter[i] *= correction;
		}
	}

	void destroy()
	{
		delete[] tb;
		delete[] quadtree.draw_nodes;
		delete[] quadtree.generate_nodes;
		delete[] quadtree.buffer_index_filled;
	}

	bool do_triangulation = false;

	void run(DebugDrawer& dd, Camera& main_camera, Camera& current_camera, Window& window, bool show_imgui)
	{
		Frustum fr = main_camera.get_frustum();
		do_triangulation = false;
		cputri::intersect(fr, dd, main_camera.get_pos());

		cputri::draw_terrain(fr, dd, current_camera, window);

		static float threshold = 0.0f;
		static float area_mult = 1.0f;
		static float curv_mult = 1.0f;

		if (show_imgui)
		{
			ImGui::Begin("Lol");
			ImGui::SliderInt("Index", &temp, -1, 15);
			ImGui::SliderInt("Vertices per refine", &vertices_per_refine, 1, 10);
			ImGui::SliderInt("Show Connections", &show_connections, -1, 200);
			ImGui::SliderInt("Refine Node", &refine_node, -1, TERRAIN_GENERATE_NUM_NODES - 1);
			ImGui::SliderInt("Sideshow", &sideshow_bob, -1, 8);

			ImGui::End();

			ImGui::Begin("cputri");
			if (ImGui::Button("Refine"))
			{
				cputri::process_triangles(main_camera, window, threshold, area_mult, curv_mult);
				triangulate();
			}
			else if (do_triangulation)
			{
				triangulate();
			}
			if (ImGui::Button("Clear Terrain"))
			{
				clear_terrain();
			}

			ImGui::DragFloat("Area mult", &area_mult, 0.01f, 0.0f, 50.0f);
			ImGui::DragFloat("Curv mult", &curv_mult, 0.01f, 0.0f, 50.0f);
			ImGui::DragFloat("Threshold", &threshold, 0.01f, 0.0f, 50.0f);

			ImGui::Checkbox("Show", &show);
			ImGui::Checkbox("Show CC", &show_cc);

			ImGui::DragInt("Max Points", &max_points_per_refine);
			ImGui::DragInt("Vistris Start", &vistris_start, 0.1f);
			ImGui::DragInt("Vistris End", &vistris_end, 0.1f);
			ImGui::End();
		}
	}

	uint find_chunk()
	{
		for (uint i = 0; i < num_nodes; i++)
		{
			if (!quadtree.buffer_index_filled[i])
				return i;
		}

		return INVALID;
	}

	uint get_offset(uint node_x, uint node_z)
	{
		assert(node_x >= 0u && node_x < (1u << quadtree_levels));
		assert(node_z >= 0u && node_z < (1u << quadtree_levels));

		return quadtree.node_index_to_buffer_index[node_x + (1u << quadtree_levels) * node_z];
	}

	uint64_t get_index_offset_of_node(uint i)
	{
		return get_offset_of_node(i) + sizeof(VkDrawIndexedIndirectCommand) + sizeof(BufferNodeHeader);
	}

	uint64_t get_vertex_offset_of_node(uint i)
	{
		return get_index_offset_of_node(i) + num_indices * sizeof(uint);

	}

	uint64_t get_offset_of_node(uint i)
	{
		return cpu_index_buffer_size + i * quadtree.node_memory_size;
	}

	void shift_quadtree(glm::vec3 camera_pos)
	{
		size_t nodes_per_side = (1ull << quadtree_levels);

		bool shifted = false;

		do
		{
			shifted = false;

			if (camera_pos.x + quadtree.quadtree_shift_distance >= quadtree.quadtree_minmax[1].x)
			{
				shifted = true;

				quadtree.quadtree_minmax[0].x += quadtree.node_size.x;
				quadtree.quadtree_minmax[1].x += quadtree.node_size.x;

				for (size_t y = 0; y < nodes_per_side; ++y)
				{
					for (size_t x = 0; x < nodes_per_side; ++x)
					{
						size_t index = y * nodes_per_side + x;

						if (x == nodes_per_side - 1)
						{
							quadtree.node_index_to_buffer_index[index] = INVALID;
						}
						else
						{
							if (x == 0 && quadtree.node_index_to_buffer_index[index] != INVALID)
							{
								quadtree.buffer_index_filled[quadtree.node_index_to_buffer_index[index]] = false;
							}

							quadtree.node_index_to_buffer_index[index] = quadtree.node_index_to_buffer_index[index + 1];
						}
					}
				}
			}
			else if (camera_pos.x - quadtree.quadtree_shift_distance <= quadtree.quadtree_minmax[0].x)
			{
				shifted = true;

				quadtree.quadtree_minmax[0].x -= quadtree.node_size.x;
				quadtree.quadtree_minmax[1].x -= quadtree.node_size.x;

				for (size_t y = 0; y < nodes_per_side; ++y)
				{
					for (int64_t x = nodes_per_side - 1; x >= 0; --x)
					{
						size_t index = y * nodes_per_side + x;

						if (x == 0)
						{
							quadtree.node_index_to_buffer_index[index] = INVALID;
						}
						else
						{
							if (x == nodes_per_side - 1 && quadtree.node_index_to_buffer_index[index] != INVALID)
							{
								quadtree.buffer_index_filled[quadtree.node_index_to_buffer_index[index]] = false;
							}

							quadtree.node_index_to_buffer_index[index] = quadtree.node_index_to_buffer_index[index - 1];
						}
					}
				}
			}
			else if (camera_pos.z + quadtree.quadtree_shift_distance >= quadtree.quadtree_minmax[1].y)
			{
				shifted = true;

				quadtree.quadtree_minmax[0].y += quadtree.node_size.y;
				quadtree.quadtree_minmax[1].y += quadtree.node_size.y;

				for (size_t y = 0; y < nodes_per_side; ++y)
				{
					for (size_t x = 0; x < nodes_per_side; ++x)
					{
						size_t index = y * nodes_per_side + x;

						if (y == nodes_per_side - 1)
						{
							quadtree.node_index_to_buffer_index[index] = INVALID;
						}
						else
						{
							if (y == 0 && quadtree.node_index_to_buffer_index[index] != INVALID)
							{
								quadtree.buffer_index_filled[quadtree.node_index_to_buffer_index[index]] = false;
							}

							quadtree.node_index_to_buffer_index[index] = quadtree.node_index_to_buffer_index[index + nodes_per_side];
						}
					}
				}
			}
			else if (camera_pos.z - quadtree.quadtree_shift_distance <= quadtree.quadtree_minmax[0].y)
			{
				shifted = true;

				quadtree.quadtree_minmax[0].y -= quadtree.node_size.y;
				quadtree.quadtree_minmax[1].y -= quadtree.node_size.y;

				for (int64_t y = nodes_per_side - 1; y >= 0; --y)
				{
					for (size_t x = 0; x < nodes_per_side; ++x)
					{
						size_t index = y * nodes_per_side + x;

						if (y == 0)
						{
							quadtree.node_index_to_buffer_index[index] = INVALID;
						}
						else
						{
							if (y == nodes_per_side - 1 && quadtree.node_index_to_buffer_index[index] != INVALID)
							{
								quadtree.buffer_index_filled[quadtree.node_index_to_buffer_index[index]] = false;
							}

							quadtree.node_index_to_buffer_index[index] = quadtree.node_index_to_buffer_index[index - nodes_per_side];
						}
					}
				}
			}
		} while (shifted);
	}

	void clear_terrain()
	{
		memset(quadtree.node_index_to_buffer_index, INVALID, (1 << quadtree_levels) * (1 << quadtree_levels) * sizeof(uint));
		memset(quadtree.buffer_index_filled, 0, num_nodes * sizeof(bool));

		for (uint ii = 0; ii < (1u << quadtree_levels) * (1u << quadtree_levels); ii++)
		{
			tb->data[ii].instance_count = 0;
		}
	}

	void triangulate()
	{
		const int nodes_per_side = 1 << quadtree_levels;
		for (int yy = 0; yy < 3; ++yy)
		{
			for (int xx = 0; xx < 3; ++xx)
			{
				for (int ty = yy; ty < nodes_per_side; ty += 3)
				{
					for (int tx = xx; tx < nodes_per_side; tx += 3)
					{
						bool all_valid = true;
						// Check self and neighbour nodes
						for (int y = -1; y <= 1; ++y)
						{
							for (int x = -1; x <= 1; ++x)
							{
								const int nx = tx + x;
								const int ny = ty + y;
								if (nx >= 0 && nx < nodes_per_side && ny >= 0 && ny < nodes_per_side)
								{
									const uint neighbour_index = quadtree.node_index_to_buffer_index[ny * nodes_per_side + nx];
									if (neighbour_index == INVALID)
									{
										all_valid = false;
									}
								}
								else
								{
									all_valid = false;
								}
							}
						}

						if (all_valid)
						{
							uint index = quadtree.node_index_to_buffer_index[ty * nodes_per_side + tx];
							triangulate::triangulate(tb, tg, index);
						}
					}
				}
			}
		}
	}

	void process_triangles(Camera& camera, Window& window, float em_threshold, float area_multiplier, float curvature_multiplier)
	{
		// Nonupdated terrain
		for (uint i = 0; i < quadtree.num_draw_nodes; i++)
		{
			process::triangle_process(
				tb,
				quadtree,
				log_filter,
				camera.get_vp(),
				vec4(camera.get_pos(), 0),
				window.get_size(),
				em_threshold,
				area_multiplier,
				curvature_multiplier,
				quadtree.draw_nodes[i]);
		}

		// Newly generated terrain
		for (uint i = 0; i < quadtree.num_generate_nodes; i++)
		{
			process::triangle_process(
				tb,
				quadtree,
				log_filter,
				camera.get_vp(),
				vec4(camera.get_pos(), 0),
				window.get_size(),
				em_threshold,
				area_multiplier,
				curvature_multiplier,
				quadtree.generate_nodes[i].index);
		}
	}

	void intersect(Frustum& frustum, DebugDrawer& dd, vec3 camera_pos)
	{
		shift_quadtree(camera_pos);

		quadtree.num_generate_nodes = 0;
		quadtree.num_draw_nodes = 0;

		float half_length = quadtree.total_side_length * 0.5f;
		
		// Gather status of nodes
		intersect(frustum, dd, AabbXZ{ quadtree.quadtree_minmax[0],
			quadtree.quadtree_minmax[1] }, 0, 0, 0);

		for (uint i = 0; i < quadtree.num_generate_nodes; i++)
		{
			tb->data[quadtree.generate_nodes[i].index].instance_count = 0;
			tb->data[quadtree.generate_nodes[i].index].new_points_count = 0;
		}

		for (uint i = 0; i < quadtree.num_generate_nodes; i++)
		{
			generate::generate(tb, gg, log_filter, quadtree.generate_nodes[i].index, quadtree.generate_nodes[i].min, quadtree.generate_nodes[i].max);
			do_triangulation = true;
		}
	}

	int intersect_triangle(glm::vec3 r_o, glm::vec3 r_d, glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, float * t)
	{
		vec3 edge1, edge2, tvec, pvec, qvec;
		float det, inv_det, u, v;

		//find vecotrs fo two edges sharing vert0
		edge1 = p1 - p0;
		edge2 = p2 - p0;

		//begin calculationg determinant
		pvec = cross(r_d, edge2);

		//if determinant is near zero, ray lies in plane of triangle
		det = dot(edge1, pvec);

		const float epsilon = 0.00001f;
		if (det > -epsilon && det < epsilon)
			return 0;
		inv_det = 1.0f / det;

		//calculate distance from vert0 to ray origin
		tvec = r_o - p0;

		u = dot(tvec, pvec) * inv_det;
		if (u < 0.0f || u > 1.0f)
			return 0;

		//prepare to test V parameter
		qvec = cross(tvec, edge1);

		//calculate V parameter and test bounds
		v = dot(r_d, qvec) * inv_det;
		if (v < 0.0f || u + v > 1.0f)
			return 0;

		//calculate t, ray intersection triangle
		*t = dot(edge2, qvec) * inv_det;

		return 1;
	}

	void intersect(Frustum& frustum, DebugDrawer& dd, AabbXZ aabb, uint level, uint x, uint y)
	{
		if (level == quadtree_levels)
		{
			//float minx = aabb.m_min.x + (aabb.m_max.x - aabb.m_min.x) * 0.05f;
			//float maxx = aabb.m_min.x + (aabb.m_max.x - aabb.m_min.x) * 0.95f;

			//float minz = aabb.m_min.y + (aabb.m_max.y - aabb.m_min.y) * 0.05f;
			//float maxz = aabb.m_min.y + (aabb.m_max.y - aabb.m_min.y) * 0.95f;

			//dd.draw_line({ minx, 0, minz }, { minx, 0, maxz }, { 1, 1, 0 });
			//dd.draw_line({ minx, 0, minz }, { maxx, 0, minz }, { 1, 1, 0 });

			//dd.draw_line({ maxx, 0, maxz }, { maxx, 0, minz }, { 1, 1, 0 });
			//dd.draw_line({ maxx, 0, maxz }, { minx, 0, maxz }, { 1, 1, 0 });

			// Index into m_node_index_to_buffer_index
			uint index = (1 << quadtree_levels) * y + x;
			if (quadtree.node_index_to_buffer_index[index] == INVALID)
			{
				// Visible node does not have data

				uint new_index = find_chunk();
				if (new_index != INVALID)
				{
					quadtree.buffer_index_filled[new_index] = true;
					quadtree.node_index_to_buffer_index[index] = new_index;

					// m_buffer[new_index] needs to be filled with data
					quadtree.generate_nodes[quadtree.num_generate_nodes].index = new_index;
					quadtree.generate_nodes[quadtree.num_generate_nodes].min = aabb.m_min;
					quadtree.generate_nodes[quadtree.num_generate_nodes].max = aabb.m_max;
					quadtree.num_generate_nodes++;
				}
				else
				{
					// No space left! Ignore for now...
				}
			}
			else
			{
				// Visible node has data, draw it

				// m_buffer[m_node_index_to_buffer_index[index]] needs to be drawn
				quadtree.draw_nodes[quadtree.num_draw_nodes] = quadtree.node_index_to_buffer_index[index];
				quadtree.num_draw_nodes++;
			}

			return;
		}

		// This node is visible, check children
		if (frustum_aabbxz_intersection(frustum, aabb))
		{
			vec2 mid = (aabb.m_min + aabb.m_max) * 0.5f;
			float mid_x = (aabb.m_min.x + aabb.m_max.x) * 0.5f;
			float mid_z = (aabb.m_min.y + aabb.m_max.y) * 0.5f;

			intersect(frustum, dd, { {aabb.m_min.x, aabb.m_min.y}, {mid.x, mid.y} }, level + 1, (x << 1), (y << 1));
			intersect(frustum, dd, { {aabb.m_min.x, mid_z}, {mid.x, aabb.m_max.y} }, level + 1, (x << 1), (y << 1) + 1);
			intersect(frustum, dd, { {mid_x, aabb.m_min.y}, {aabb.m_max.x, mid_z} }, level + 1, (x << 1) + 1, (y << 1));
			intersect(frustum, dd, { {mid.x, mid.y}, {aabb.m_max.x, aabb.m_max.y} }, level + 1, (x << 1) + 1, (y << 1) + 1);
		}
	}

	void draw_terrain(Frustum& frustum, DebugDrawer& dd, Camera& camera, Window& window)
	{
		static vec3 ori;
		static vec3 dir;
		if (show)
		{
			for (size_t ii = 0; ii < num_nodes; ii++)
			{
				if (quadtree.buffer_index_filled[ii] && (ii == temp || temp == -1))
				{
					int hovered_triangle = -1;
					const float height = -100.0f;


					// If C is pressed, do ray-triangle intersection and show connection of hovered triangle
					if (glfwGetKey(window.get_glfw_window(), GLFW_KEY_C) == GLFW_PRESS
						&& !ImGui::GetIO().WantCaptureMouse)
					{
						vec3 ray_o = camera.get_pos();
						vec3 ray_dir;

						ori = ray_o;
						

						vec2 mouse_pos;
						// Get mouse pos
						const bool focused = glfwGetWindowAttrib(window.get_glfw_window(), GLFW_FOCUSED) != 0;
						if (focused)
						{
							double mouse_x, mouse_y;
							glfwGetCursorPos(window.get_glfw_window(), &mouse_x, &mouse_y);
							mouse_pos = vec2((float)mouse_x, (float)mouse_y);
						}

						int w, h;
						glfwGetWindowSize(window.get_glfw_window(), &w, &h);
						vec2 window_size = vec2(w, h);
						const float deg_to_rad = 3.1415f / 180.0f;
						const float fov = camera.get_fov();	// In degrees
						float px = 2.0f * (mouse_pos.x + 0.5f - window_size.x / 2) / window_size.x * tan(fov / 2.0f * deg_to_rad);
						float py = 2.0f * (mouse_pos.y + 0.5f - window_size.y / 2) / window_size.y * tan(fov / 2.0f * deg_to_rad) * window_size.y / window_size.x;
						ray_dir = vec3(px, py, 1);
						ray_dir = normalize(vec3(inverse(camera.get_view()) * vec4(normalize(ray_dir), 0.0f)));

						dir = ray_dir;

						float d = 9999999999.0f;
						float max_d = 9999999999.0f;

						// Perform ray-triangle intersection
						for (uint ind = 0; ind < tb->data[ii].index_count; ind += 3)
						{
							vec3 p0 = vec3(tb->data[ii].positions[tb->data[ii].indices[ind + 0]]) + vec3(0.0f, height, 0.0f);
							vec3 p1 = vec3(tb->data[ii].positions[tb->data[ii].indices[ind + 1]]) + vec3(0.0f, height, 0.0f);
							vec3 p2 = vec3(tb->data[ii].positions[tb->data[ii].indices[ind + 2]]) + vec3(0.0f, height, 0.0f);

							if (intersect_triangle(ray_o, ray_dir, p0, p1, p2, &d) && d < max_d && d >= 0.0f)
							{
								hovered_triangle = ind / 3;
							}
						}
					}

					for (uint ind = vistris_start * 3; ind < tb->data[ii].index_count && ind < (uint)vistris_end * 3; ind += 3)
					{
						vec3 p0 = vec3(tb->data[ii].positions[tb->data[ii].indices[ind + 0]]) + vec3(0.0f, height, 0.0f);
						vec3 p1 = vec3(tb->data[ii].positions[tb->data[ii].indices[ind + 1]]) + vec3(0.0f, height, 0.0f);
						vec3 p2 = vec3(tb->data[ii].positions[tb->data[ii].indices[ind + 2]]) + vec3(0.0f, height, 0.0f);

						vec3 mid = (p0 + p1 + p2) / 3.0f;

						dd.draw_line(p0, p1, { 1, 0, 0 });
						dd.draw_line(p1, p2, { 1, 0, 0 });
						dd.draw_line(p2, p0, { 1, 0, 0 });

						//dd.draw_line(mid, p0, { 0, 1, 0 });
						//dd.draw_line(mid, p1, { 0, 1, 0 });
						//dd.draw_line(mid, p2, { 0, 1, 0 });

						if (show_cc || hovered_triangle == ind / 3)
						{
							const uint tri_index = ind / 3;
							const uint steps = 20;
							const float angle = 3.14159265f * 2.0f / steps;
							for (uint jj = 0; jj < steps + 1; ++jj)
							{
								float cc_radius = sqrt(tb->data[ii].triangles[tri_index].circumradius2);
								vec3 cc_mid = { tb->data[ii].triangles[tri_index].circumcentre.x, mid.y, tb->data[ii].triangles[tri_index].circumcentre.y };

								dd.draw_line(cc_mid + vec3(sinf(angle * jj) * cc_radius, 0.0f, cosf(angle * jj) * cc_radius),
									cc_mid + vec3(sinf(angle * (jj + 1)) * cc_radius, 0.0f, cosf(angle * (jj + 1)) * cc_radius),
									{ 0, 0, 1 });
							}
						}

						if (show_connections == ind / 3 || hovered_triangle == ind / 3)
						{
							glm::vec3 h = { 0, -20, 0 };

							dd.draw_line(p0 + h, p1 + h, { 1, 0, 0 });
							dd.draw_line(p1 + h, p2 + h, { 0, 1, 0 });
							dd.draw_line(p2 + h, p0 + h, { 0, 0, 1 });

							glm::vec3 n0 = mid + h;
							glm::vec3 n1 = mid + h;
							glm::vec3 n2 = mid + h;

							if (tb->data[ii].triangle_connections[ind + 0] < INVALID - 10)
							{
								uint neighbour_ind = tb->data[ii].triangle_connections[ind + 0];
								n0 = (tb->data[ii].positions[tb->data[ii].indices[neighbour_ind * 3 + 0]] + 
									  tb->data[ii].positions[tb->data[ii].indices[neighbour_ind * 3 + 1]] + 
									  tb->data[ii].positions[tb->data[ii].indices[neighbour_ind * 3 + 2]]) / 3.0f;
								n0 += glm::vec3(0, height, 0) + h;
							}
							if (tb->data[ii].triangle_connections[ind + 1] < INVALID - 10)
							{
								uint neighbour_ind = tb->data[ii].triangle_connections[ind + 1];
								n1 = (tb->data[ii].positions[tb->data[ii].indices[neighbour_ind * 3 + 0]] +
									tb->data[ii].positions[tb->data[ii].indices[neighbour_ind * 3 + 1]] +
									tb->data[ii].positions[tb->data[ii].indices[neighbour_ind * 3 + 2]]) / 3.0f;
								n1 += glm::vec3(0, height, 0) + h;
							}
							if (tb->data[ii].triangle_connections[ind + 2] < INVALID - 10)
							{
								uint neighbour_ind = tb->data[ii].triangle_connections[ind + 2];
								n2 = (tb->data[ii].positions[tb->data[ii].indices[neighbour_ind * 3 + 0]] +
									tb->data[ii].positions[tb->data[ii].indices[neighbour_ind * 3 + 1]] +
									tb->data[ii].positions[tb->data[ii].indices[neighbour_ind * 3 + 2]]) / 3.0f;
								n2 += glm::vec3(0, height, 0) + h;
							}


							dd.draw_line(mid + h, n0, { 1, 0, 0 });
							dd.draw_line(mid + h, n1, { 0, 1, 0 });
							dd.draw_line(mid + h, n2, { 0, 0, 1 });
						}
					}
					//for (int tt = 0; tt < tb->data[ii].vertex_count; ++tt)
					//{
					//	vec3 p = tb->data[ii].positions[tt] + vec4(0, -100, 0, 0);
					//	dd.draw_line(p, p + vec3(0, -50, 0), vec3(1, 0, 1));
					//}

					//vec2 min = tb->data[ii].min;
					//vec2 max = tb->data[ii].max;
					//dd.draw_line({ min.x - tb->data[ii].border_max[3], -150, min.y }, { min.x - tb->data[ii].border_max[3], -150, max.y }, { 1, 1, 1 });
					//dd.draw_line({ max.x + tb->data[ii].border_max[1], -150, min.y }, { max.x + tb->data[ii].border_max[1], -150, max.y }, { 1, 1, 1 });
					//dd.draw_line({ min.x, -150, max.y + tb->data[ii].border_max[0] }, { max.x, -150, max.y + tb->data[ii].border_max[0] }, { 1, 1, 1 });
					//dd.draw_line({ min.x, -150, min.y - tb->data[ii].border_max[2] }, { max.x, -150, min.y - tb->data[ii].border_max[2] }, { 1, 1, 1 });

					for (uint bt = 0; bt < tb->data[ii].border_count; ++bt)
					{
						const float height = -102.0f;
						uint ind = tb->data[ii].border_triangle_indices[bt] * 3;
						vec3 p0 = vec3(tb->data[ii].positions[tb->data[ii].indices[ind + 0]]) + vec3(0.0f, height, 0.0f);
						vec3 p1 = vec3(tb->data[ii].positions[tb->data[ii].indices[ind + 1]]) + vec3(0.0f, height, 0.0f);
						vec3 p2 = vec3(tb->data[ii].positions[tb->data[ii].indices[ind + 2]]) + vec3(0.0f, height, 0.0f);

						if (sideshow_bob != -1 && tb->data[ii].triangle_connections[ind + 0] == INVALID - sideshow_bob)
						{
							dd.draw_line(p0 - vec3{ 0, 2, 0 }, p1 - vec3{ 0, 2, 0 }, { 0.0f, 1.0f, 0.0f });
						}
						if (sideshow_bob != -1 && tb->data[ii].triangle_connections[ind + 1] == INVALID - sideshow_bob)
						{
							dd.draw_line(p1 - vec3{ 0, 2, 0 }, p2 - vec3{ 0, 2, 0 }, { 0.0f, 1.0f, 0.0f });
						}
						if (sideshow_bob != -1 && tb->data[ii].triangle_connections[ind + 2] == INVALID - sideshow_bob)
						{
							dd.draw_line(p2 - vec3{ 0, 2, 0 }, p0 - vec3{ 0, 2, 0 }, { 0.0f, 1.0f, 0.0f });
						}

						dd.draw_line(p0, p1, { 0, 0, 1 });
						dd.draw_line(p1, p2, { 0, 0, 1 });
						dd.draw_line(p2, p0, { 0, 0, 1 });
					}
				}
			}
		}
	}

}
