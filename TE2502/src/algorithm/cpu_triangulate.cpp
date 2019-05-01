#include "vulkan/vulkan.h"
#include "cpu_triangulate.hpp"
#include "graphics/window.hpp"
#include "utilities.hpp"

#include "algorithm/generate.hpp"
#include "algorithm/process.hpp"
#include "algorithm/triangulate.hpp"
#include "imgui/imgui.h"

#include <mutex>

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

	std::mutex shared_data_lock = std::mutex();

	// Strings of hovered triangles
	std::vector<std::string> hovered_tris;

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

		quadtree.node_memory_size = sizeof(TerrainData);


		// Add space for an additional two vec2's to store the quadtree min and max
		cpu_index_buffer_size = (1 << quadtree_levels) * (1 << quadtree_levels) * sizeof(uint) + sizeof(vec2) * 2;

		filter_setup();
	}

	void destroy(VulkanContext& context, GPUBuffer& cpu_buffer)
	{
		vkUnmapMemory(context.get_device(), cpu_buffer.get_memory());
		delete[] quadtree.draw_nodes;
		delete[] quadtree.generate_nodes;
		delete[] quadtree.buffer_index_filled;
	}

	bool do_triangulation = false;

	void run(TriData* tri_data)
	{
		cputri::draw_terrain(tri_data);

		do_triangulation = false;
		cputri::intersect(tri_data->mc_frustum, *tri_data->dd, tri_data->mc_pos);

		// Clear copy data
		for (uint ii = 0; ii < quadtree.num_draw_nodes; ++ii)
		{
			TerrainData& node = tb->data[quadtree.draw_nodes[ii]];
			node.lowest_indices_index_changed = node.index_count;
			node.old_vertex_count = node.vertex_count;
		}

		if (tri_data->triangulate)
		{
			cputri::process_triangles(tri_data);
			triangulate(tri_data);
		}
		else if (do_triangulation)
		{
			triangulate(tri_data);
		}

	}

	void draw(GraphicsQueue& queue, GPUBuffer& gpu_buffer, GPUBuffer& cpu_buffer)
	{
		std::unique_lock<std::mutex> lock(shared_data_lock);

		// Render nonupdated terrain
		for (uint32_t i = 0; i < quadtree.num_draw_nodes; i++)
		{
			queue.cmd_bind_index_buffer(gpu_buffer.get_buffer(), get_gpu_index_offset_of_node(quadtree.draw_nodes[i]));
			queue.cmd_bind_vertex_buffer(gpu_buffer.get_buffer(), get_gpu_vertex_offset_of_node(quadtree.draw_nodes[i]));
			queue.cmd_draw_indexed(tb->data[quadtree.draw_nodes[i]].draw_index_count);
		}

		// Render newly generated terrain
		for (uint32_t i = 0; i < quadtree.num_generate_nodes; i++)
		{
			if (quadtree.generate_nodes[i].index != INVALID)
			{
				queue.cmd_bind_index_buffer(gpu_buffer.get_buffer(), get_gpu_index_offset_of_node(quadtree.generate_nodes[i].index));
				queue.cmd_bind_vertex_buffer(gpu_buffer.get_buffer(), get_gpu_vertex_offset_of_node(quadtree.generate_nodes[i].index));
				queue.cmd_draw_indexed(tb->data[quadtree.generate_nodes[i].index].draw_index_count);
			}
		}
	}

	void upload(GraphicsQueue& queue, GPUBuffer& gpu_buffer, GPUBuffer& cpu_buffer)
	{
		std::unique_lock<std::mutex> lock(shared_data_lock);

		// Uploading
		for (uint32_t i = 0; i < quadtree.num_draw_nodes; i++)
		{
			TerrainData& node = tb->data[quadtree.draw_nodes[i]];
			if (node.has_data_to_copy)
			{
				node.has_data_to_copy = false;

				// Indices
				uint offset = node.lowest_indices_index_changed * sizeof(uint);
				uint size = (node.index_count - node.lowest_indices_index_changed) * sizeof(uint);
				queue.cmd_copy_buffer(cpu_buffer.get_buffer(), gpu_buffer.get_buffer(),
					size,
					get_cpu_index_offset_of_node(quadtree.draw_nodes[i]) + offset,
					get_gpu_index_offset_of_node(quadtree.draw_nodes[i]) + offset);

				queue.cmd_buffer_barrier(
					gpu_buffer.get_buffer(),
					VK_ACCESS_TRANSFER_WRITE_BIT,
					VK_ACCESS_INDEX_READ_BIT,
					VK_PIPELINE_STAGE_TRANSFER_BIT,
					VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
					get_gpu_index_offset_of_node(quadtree.draw_nodes[i]) + offset,
					size
				);

				// Vertices
				offset = node.old_vertex_count * sizeof(vec4);
				size = (node.vertex_count - node.old_vertex_count) * sizeof(vec4);
				queue.cmd_copy_buffer(cpu_buffer.get_buffer(), gpu_buffer.get_buffer(),
					size,
					get_cpu_vertex_offset_of_node(quadtree.draw_nodes[i]) + offset,
					get_gpu_vertex_offset_of_node(quadtree.draw_nodes[i]) + offset);

				queue.cmd_buffer_barrier(
					gpu_buffer.get_buffer(),
					VK_ACCESS_TRANSFER_WRITE_BIT,
					VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
					VK_PIPELINE_STAGE_TRANSFER_BIT,
					VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
					get_gpu_vertex_offset_of_node(quadtree.draw_nodes[i]),
					num_vertices * sizeof(vec4)
				);
			}
		}
		for (uint32_t i = 0; i < quadtree.num_generate_nodes; i++)
		{
			if (quadtree.generate_nodes[i].index != INVALID)
			{
				// Indices
				queue.cmd_copy_buffer(cpu_buffer.get_buffer(), gpu_buffer.get_buffer(),
					tb->data[quadtree.generate_nodes[i].index].index_count * sizeof(uint),
					get_cpu_index_offset_of_node(quadtree.generate_nodes[i].index),
					get_gpu_index_offset_of_node(quadtree.generate_nodes[i].index));

				queue.cmd_buffer_barrier(
					gpu_buffer.get_buffer(),
					VK_ACCESS_TRANSFER_WRITE_BIT,
					VK_ACCESS_INDEX_READ_BIT,
					VK_PIPELINE_STAGE_TRANSFER_BIT,
					VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
					get_gpu_index_offset_of_node(quadtree.generate_nodes[i].index),
					tb->data[quadtree.generate_nodes[i].index].index_count * sizeof(uint)
				);

				// Vertices
				queue.cmd_copy_buffer(cpu_buffer.get_buffer(), gpu_buffer.get_buffer(), 
					tb->data[quadtree.generate_nodes[i].index].vertex_count * sizeof(vec4),
					get_cpu_vertex_offset_of_node(quadtree.generate_nodes[i].index),
					get_gpu_vertex_offset_of_node(quadtree.generate_nodes[i].index));

				queue.cmd_buffer_barrier(
					gpu_buffer.get_buffer(),
					VK_ACCESS_TRANSFER_WRITE_BIT,
					VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
					VK_PIPELINE_STAGE_TRANSFER_BIT,
					VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
					get_gpu_vertex_offset_of_node(quadtree.generate_nodes[i].index),
					tb->data[quadtree.generate_nodes[i].index].vertex_count * sizeof(vec4)
				);
			}
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

	uint64_t get_gpu_index_offset_of_node(uint i)
	{
		return get_gpu_vertex_offset_of_node(i) + num_vertices * sizeof(vec4);
	}

	uint64_t get_gpu_vertex_offset_of_node(uint i)
	{
		return get_gpu_offset_of_node(i);
	}

	uint64_t get_gpu_offset_of_node(uint i)
	{
		return i * (num_indices * sizeof(uint) + num_vertices * sizeof(vec4));
	}

	uint64_t get_cpu_index_offset_of_node(uint i)
	{
		return get_cpu_offset_of_node(i);
	}

	uint64_t get_cpu_vertex_offset_of_node(uint i)
	{
		return get_cpu_index_offset_of_node(i) + num_indices * sizeof(uint);
	}

	uint64_t get_cpu_offset_of_node(uint i)
	{
		return cpu_index_buffer_size + i * quadtree.node_memory_size;
	}

	void filter_setup()
	{
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

	void gpu_data_setup(VulkanContext& context, GPUMemory& gpu_mem, GPUMemory& cpu_mem, GPUBuffer& gpu_buffer, GPUBuffer& cpu_buffer)
	{
		// Allocate GPU and CPU memory for draw data
		gpu_mem = context.allocate_device_memory((num_indices * sizeof(uint) + num_vertices * sizeof(vec4)) * num_nodes + 1000);
		cpu_mem = context.allocate_host_memory(cpu_index_buffer_size + quadtree.node_memory_size * num_nodes + 1000);

		gpu_buffer = GPUBuffer(context,
			(num_indices * sizeof(uint) + num_vertices * sizeof(vec4)) * num_nodes,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			gpu_mem);

		cpu_buffer = GPUBuffer(context,
			cpu_index_buffer_size + quadtree.node_memory_size * num_nodes,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			cpu_mem);

		// Map CPU buffer to TerrainBuffer pointer
		vkMapMemory(context.get_device(), cpu_buffer.get_memory(), 0, VK_WHOLE_SIZE, 0, (void**) &tb);


		// Setup remaining quadtree variables
		quadtree.node_index_to_buffer_index = (uint*)tb;

		// Point to the end of cpu index buffer
		quadtree.quadtree_minmax = (vec2*)(((char*)quadtree.node_index_to_buffer_index) + (1 << quadtree_levels) * (1 << quadtree_levels) * sizeof(uint));

		quadtree.total_side_length = TERRAIN_GENERATE_TOTAL_SIDE_LENGTH;
		float half_length = quadtree.total_side_length * 0.5f;
		quadtree.quadtree_minmax[0] = vec2(-half_length, -half_length);
		quadtree.quadtree_minmax[1] = vec2(half_length, half_length);

		// (1 << levels) is number of nodes per axis
		memset(quadtree.node_index_to_buffer_index, INVALID, (1 << quadtree_levels) * (1 << quadtree_levels) * sizeof(uint));

		quadtree.node_size = vec2(quadtree.total_side_length / (1 << quadtree_levels), quadtree.total_side_length / (1 << quadtree_levels));
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

		// Update draw index counts
		{
			std::unique_lock<std::mutex> lock(shared_data_lock);

			for (uint ii = 0; ii < (1u << quadtree_levels) * (1u << quadtree_levels); ii++)
			{
				tb->data[ii].is_invalid = true;
			}

			for (uint ii = 0; ii < num_nodes; ii++)
			{
				tb->data[ii].draw_index_count = tb->data[ii].index_count;
			}
		}
	}

	void triangulate(cputri::TriData* tri_data)
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
							triangulate::triangulate(tb, tg, index, tri_data);
						}
					}
				}
			}
		}

		// Update draw index counts
		{
			std::unique_lock<std::mutex> lock(shared_data_lock);

			for (uint ii = 0; ii < num_nodes; ii++)
			{
				tb->data[ii].draw_index_count = tb->data[ii].index_count;
			}
		}
	}

	void process_triangles(TriData* tri_data)
	{
		// Nonupdated terrain
		for (uint i = 0; i < quadtree.num_draw_nodes; i++)
		{
			process::triangle_process(
				tb,
				quadtree,
				log_filter,
				tri_data->mc_vp,
				vec4(tri_data->mc_pos, 0),
				tri_data->threshold,
				tri_data->area_mult,
				tri_data->curv_mult,
				quadtree.draw_nodes[i],
				tri_data);
		}

		// Newly generated terrain
		for (uint i = 0; i < quadtree.num_generate_nodes; i++)
		{
			process::triangle_process(
				tb,
				quadtree,
				log_filter,
				tri_data->mc_vp,
				vec4(tri_data->mc_pos, 0),
				tri_data->threshold,
				tri_data->area_mult,
				tri_data->curv_mult,
				quadtree.generate_nodes[i].index,
				tri_data);
		}
	}

	void intersect(Frustum& frustum, DebugDrawer& dd, vec3 camera_pos)
	{
		float half_length = quadtree.total_side_length * 0.5f;
		
		{
			std::unique_lock<std::mutex> lock(shared_data_lock);

			shift_quadtree(camera_pos);

			quadtree.num_generate_nodes = 0;
			quadtree.num_draw_nodes = 0;

			// Gather status of nodes
			intersect(frustum, dd, AabbXZ{ quadtree.quadtree_minmax[0],
				quadtree.quadtree_minmax[1] }, 0, 0, 0);

			for (uint i = 0; i < quadtree.num_generate_nodes; i++)
			{
				tb->data[quadtree.generate_nodes[i].index].is_invalid = true;
				tb->data[quadtree.generate_nodes[i].index].new_points_count = 0;
				tb->data[quadtree.generate_nodes[i].index].draw_index_count = 0;
			}

			for (uint i = 0; i < quadtree.num_generate_nodes; i++)
			{
				generate::generate(tb, gg, log_filter, quadtree.generate_nodes[i].index, quadtree.generate_nodes[i].min, quadtree.generate_nodes[i].max);
				do_triangulation = true;
			}
		}


		// Update draw index counts
		{
			std::unique_lock<std::mutex> lock(shared_data_lock);

			for (uint ii = 0; ii < num_nodes; ii++)
			{
				tb->data[ii].draw_index_count = tb->data[ii].index_count;
			}
		}
	}

	int intersect_triangle(glm::vec3 r_o, glm::vec3 r_d, glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, float* t)
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

	std::vector<std::string> get_hovered_tris()
	{
		std::unique_lock<std::mutex> lock(shared_data_lock);
		return hovered_tris;
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

	void draw_terrain(TriData* tri_data)
	{
		// Lock debug drawing mutex for the duration of this function
		std::unique_lock<std::mutex> lock(*tri_data->debug_draw_mutex);

		hovered_tris.clear();

		static vec3 ori;
		static vec3 dir;
		if (tri_data->show_debug)
		{
			for (size_t ii = 0; ii < num_nodes; ii++)
			{
				if (quadtree.buffer_index_filled[ii] && (ii == tri_data->show_node || tri_data->show_node == -1))
				{
					int hovered_triangle = -1;
					const float height = -100.0f;


					// If C is pressed, do ray-triangle intersection and show connection of hovered triangle
					if (tri_data->show_hovered)
					{
						vec3 ray_o = tri_data->cc_pos;
						vec3 ray_dir;

						ori = ray_o;
						
						const float deg_to_rad = 3.1415f / 180.0f;
						const float fov = tri_data->cc_fov;	// In degrees
						float px = 2.0f * (tri_data->mouse_pos.x + 0.5f - tri_data->window_size.x / 2) / tri_data->window_size.x * tan(fov / 2.0f * deg_to_rad);
						float py = 2.0f * (tri_data->mouse_pos.y + 0.5f - tri_data->window_size.y / 2) / tri_data->window_size.y * tan(fov / 2.0f * deg_to_rad) * tri_data->window_size.y / tri_data->window_size.x;
						ray_dir = vec3(px, py, 1);
						ray_dir = normalize(vec3((inverse(tri_data->cc_view)) * vec4(normalize(ray_dir), 0.0f)));

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

						// If a triangle from this node is hovered, add a message
						if (hovered_triangle != -1)
						{
							std::string msg = "Node ";
							msg += std::to_string(ii);	// Node index
							msg += ": Triangle ";
							msg += std::to_string(hovered_triangle);
							hovered_tris.push_back(msg);
						}
					}

					for (uint ind = 0; ind < tb->data[ii].index_count; ind += 3)
					{
						vec3 p0 = vec3(tb->data[ii].positions[tb->data[ii].indices[ind + 0]]) + vec3(0.0f, height, 0.0f);
						vec3 p1 = vec3(tb->data[ii].positions[tb->data[ii].indices[ind + 1]]) + vec3(0.0f, height, 0.0f);
						vec3 p2 = vec3(tb->data[ii].positions[tb->data[ii].indices[ind + 2]]) + vec3(0.0f, height, 0.0f);

						vec3 mid = (p0 + p1 + p2) / 3.0f;

						tri_data->dd->draw_line(p0, p1, { 1, 0, 0 });
						tri_data->dd->draw_line(p1, p2, { 1, 0, 0 });
						tri_data->dd->draw_line(p2, p0, { 1, 0, 0 });

						//tri_data->dd->draw_line(mid, p0, { 0, 1, 0 });
						//tri_data->dd->draw_line(mid, p1, { 0, 1, 0 });
						//tri_data->dd->draw_line(mid, p2, { 0, 1, 0 });

						if (hovered_triangle == ind / 3)
						{
							const uint tri_index = ind / 3;
							const uint steps = 20;
							const float angle = 3.14159265f * 2.0f / steps;
							for (uint jj = 0; jj < steps + 1; ++jj)
							{
								float cc_radius = sqrt(tb->data[ii].triangles[tri_index].circumradius2);
								vec3 cc_mid = { tb->data[ii].triangles[tri_index].circumcentre.x, mid.y, tb->data[ii].triangles[tri_index].circumcentre.y };

								tri_data->dd->draw_line(cc_mid + vec3(sinf(angle * jj) * cc_radius, 0.0f, cosf(angle * jj) * cc_radius),
									cc_mid + vec3(sinf(angle * (jj + 1)) * cc_radius, 0.0f, cosf(angle * (jj + 1)) * cc_radius),
									{ 0, 0, 1 });
							}
						}

						if (hovered_triangle == ind / 3)
						{
							glm::vec3 h = { 0, -20, 0 };

							tri_data->dd->draw_line(p0 + h, p1 + h, { 1, 0, 0 });
							tri_data->dd->draw_line(p1 + h, p2 + h, { 0, 1, 0 });
							tri_data->dd->draw_line(p2 + h, p0 + h, { 0, 0, 1 });

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


							tri_data->dd->draw_line(mid + h, n0, { 1, 0, 0 });
							tri_data->dd->draw_line(mid + h, n1, { 0, 1, 0 });
							tri_data->dd->draw_line(mid + h, n2, { 0, 0, 1 });
						}
					}

					for (uint bt = 0; bt < tb->data[ii].border_count; ++bt)
					{
						const float height = -102.0f;
						uint ind = tb->data[ii].border_triangle_indices[bt] * 3;
						vec3 p0 = vec3(tb->data[ii].positions[tb->data[ii].indices[ind + 0]]) + vec3(0.0f, height, 0.0f);
						vec3 p1 = vec3(tb->data[ii].positions[tb->data[ii].indices[ind + 1]]) + vec3(0.0f, height, 0.0f);
						vec3 p2 = vec3(tb->data[ii].positions[tb->data[ii].indices[ind + 2]]) + vec3(0.0f, height, 0.0f);

						if (tri_data->sideshow_bob != -1 && tb->data[ii].triangle_connections[ind + 0] == INVALID - tri_data->sideshow_bob)
						{
							tri_data->dd->draw_line(p0 - vec3{ 0, 2, 0 }, p1 - vec3{ 0, 2, 0 }, { 0.0f, 1.0f, 0.0f });
						}
						if (tri_data->sideshow_bob != -1 && tb->data[ii].triangle_connections[ind + 1] == INVALID - tri_data->sideshow_bob)
						{
							tri_data->dd->draw_line(p1 - vec3{ 0, 2, 0 }, p2 - vec3{ 0, 2, 0 }, { 0.0f, 1.0f, 0.0f });
						}
						if (tri_data->sideshow_bob != -1 && tb->data[ii].triangle_connections[ind + 2] == INVALID - tri_data->sideshow_bob)
						{
							tri_data->dd->draw_line(p2 - vec3{ 0, 2, 0 }, p0 - vec3{ 0, 2, 0 }, { 0.0f, 1.0f, 0.0f });
						}

						tri_data->dd->draw_line(p0, p1, { 0, 0, 1 });
						tri_data->dd->draw_line(p1, p2, { 0, 0, 1 });
						tri_data->dd->draw_line(p2, p0, { 0, 0, 1 });
					}
				}
			}
		}
	}

}
