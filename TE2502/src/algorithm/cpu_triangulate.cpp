#include "vulkan/vulkan.h"
#include "cpu_triangulate.hpp"
#include "graphics/window.hpp"
#include "utilities.hpp"

#include "algorithm/generate.hpp"
#include "algorithm/process.hpp"
#include "algorithm/triangulate.hpp"
#include "algorithm/terrain_texture.hpp"
#include "imgui/imgui.h"

#include <mutex>
#include <atomic>
#include <thread>
#include <fstream>
#include <iostream>

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

// Terrain textures
TerrainTexture* storage_tex0;
TerrainTexture* storage_tex1;
TerrainTexture* storage_tex2;
TerrainTexture* storage_tex3;

namespace cputri
{
	using namespace glm;

	std::mutex shared_data_lock = std::mutex();

	static const uint num_threads = 8;

	// Atomic variables used for multithreading
	std::atomic<int> atomic_started;
	std::atomic<int> atomic_finished;
	std::atomic<int> atomic_total;
	bool quit = false;
	TriData* triangulation_data;
	std::thread threads[num_threads];

	// DEBUG
	std::vector<std::vector<glm::vec3>> debug_lines;
	std::vector<std::vector<std::vector<glm::vec3>>> old_debug_lines;

	std::vector<TerrainBuffer*> autosaves;
	std::vector<Quadtree*> quadtree_autosaves;

	// Strings of hovered triangles
	std::vector<std::string> hovered_tris;

	generate::GlobalData gg;
	triangulate::GlobalData tg;

	uint cpu_index_buffer_size;

	TerrainBuffer* tb;
	TerrainBuffer* backup_tb;
	Quadtree quadtree;
	Quadtree backup_quadtree;

	float log_filter[filter_side * filter_side];

	void setup(TFile& tfile, TerrainTexture* terrain_textures)
	{
		assert(quadtree_levels > 0);

		autosaves.reserve(50);
		quadtree_autosaves.reserve(50);

		memset(quadtree.buffer_index_filled, 0, num_nodes * sizeof(bool));

		quadtree.num_generate_nodes = 0;
		quadtree.num_generate_nodes_draw = 0;

		quadtree.num_draw_nodes = 0;
		quadtree.num_draw_nodes_draw = 0;

		quadtree.node_memory_size = sizeof(TerrainData);

		quadtree.total_side_length = TERRAIN_GENERATE_TOTAL_SIDE_LENGTH;
		float half_length = quadtree.total_side_length * 0.5f;
		quadtree.quadtree_min = vec2(-half_length, -half_length);
		quadtree.quadtree_max = vec2(half_length, half_length);

		backup_tb = new TerrainBuffer();

		// Add space for an additional two vec2's to store the quadtree min and max
		cpu_index_buffer_size = sizeof(uint*) + sizeof(vec2) * 2;

		filter_setup();

		// Set up threads
		atomic_started.store(0);
		atomic_finished.store(0);
		atomic_total.store(0);

		for (uint ii = 0; ii < num_threads; ii++)
		{
			threads[ii] = std::thread(worker_thread);
		}

		storage_tex0 = &terrain_textures[0];
		storage_tex1 = &terrain_textures[1];
		storage_tex2 = &terrain_textures[2];
		storage_tex3 = &terrain_textures[3];
	}

	void destroy(VulkanContext& context, GPUBuffer& cpu_buffer)
	{
		delete backup_tb;

		quit = true;

		for (TerrainBuffer* tb : autosaves)
		{
			delete tb;
		}
		for (Quadtree* q : quadtree_autosaves)
		{
			delete q;
		}

		for (uint ii = 0; ii < num_threads; ii++)
		{
			threads[ii].join();
		}

		vkUnmapMemory(context.get_device(), cpu_buffer.get_memory());
	}

	bool do_triangulation = false;

	void run(TriData* tri_data, const std::chrono::time_point<std::chrono::system_clock>& start_time, Timings& timings)
	{
		// Erase old autosave data
		if (autosaves.size() > 500)
		{
			for (uint i = 0; i < 300; i++)
			{
				delete autosaves[i];
				delete quadtree_autosaves[i];
			}

			autosaves.erase(autosaves.begin(), autosaves.begin() + 300);
			quadtree_autosaves.erase(quadtree_autosaves.begin(), quadtree_autosaves.begin() + 300);
		}

		cputri::draw_terrain(tri_data);

		do_triangulation = false;
		cputri::intersect(tri_data->mc_frustum, *tri_data->dd, tri_data->mc_pos, tri_data);
		std::chrono::duration<float> elapsed_time = std::chrono::system_clock::now() - start_time;
		timings.generate = elapsed_time.count();

		//if (quadtree.num_generate_nodes > 0)
		//	backup_auto();

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
			std::chrono::duration<float> elapsed_time = std::chrono::system_clock::now() - start_time;
			timings.process = elapsed_time.count();
			triangulate(tri_data);
			//backup_auto();
		}
		else if (do_triangulation)
		{
			triangulate(tri_data);
			//backup_auto();
		}

		if (debug_lines.size() > 0)
		{
			old_debug_lines.push_back(std::move(debug_lines));
		}
	}

	void draw(GraphicsQueue& queue, GPUBuffer& gpu_buffer, GPUBuffer& cpu_buffer)
	{
		std::unique_lock<std::mutex> lock(shared_data_lock);

		// Render nonupdated terrain
		for (uint32_t i = 0; i < quadtree.num_draw_nodes_draw; i++)
		{
			queue.cmd_bind_index_buffer(gpu_buffer.get_buffer(), get_gpu_index_offset_of_node(quadtree.draw_nodes_draw[i]));
			queue.cmd_bind_vertex_buffer(gpu_buffer.get_buffer(), get_gpu_vertex_offset_of_node(quadtree.draw_nodes_draw[i]));
			queue.cmd_draw_indexed(tb->data[quadtree.draw_nodes_draw[i]].draw_index_count);
		}

		// Render newly generated terrain
		for (uint32_t i = 0; i < quadtree.num_generate_nodes_draw; i++)
		{
			if (quadtree.generate_nodes_draw[i].index != INVALID)
			{
				queue.cmd_bind_index_buffer(gpu_buffer.get_buffer(), get_gpu_index_offset_of_node(quadtree.generate_nodes_draw[i].index));
				queue.cmd_bind_vertex_buffer(gpu_buffer.get_buffer(), get_gpu_vertex_offset_of_node(quadtree.generate_nodes_draw[i].index));
				queue.cmd_draw_indexed(tb->data[quadtree.generate_nodes_draw[i].index].draw_index_count);
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
			node.draw_index_count = node.index_count;

			if (node.has_data_to_copy)
			{
				node.has_data_to_copy = false;

				// Indices
				uint offset = 0;// node.lowest_indices_index_changed * sizeof(uint);
				uint size = node.index_count * sizeof(uint);// (node.index_count - node.lowest_indices_index_changed) * sizeof(uint);

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
				offset = 0; // node.old_vertex_count * sizeof(vec4);
				size = node.vertex_count * sizeof(vec4);// (node.vertex_count - node.old_vertex_count) * sizeof(vec4);

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
			TerrainData& node = tb->data[quadtree.generate_nodes[i].index];
			node.draw_index_count = node.index_count;

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



		quadtree.num_generate_nodes_draw = quadtree.num_generate_nodes;
		for (uint ii = 0; ii < quadtree.num_generate_nodes; ++ii)
		{
			quadtree.generate_nodes_draw[ii] = quadtree.generate_nodes[ii];
		}

		quadtree.num_draw_nodes_draw = quadtree.num_draw_nodes;
		quadtree.drawn_triangle_count = 0;
		for (uint ii = 0; ii < quadtree.num_draw_nodes; ++ii)
		{
			quadtree.draw_nodes_draw[ii] = quadtree.draw_nodes[ii];
			quadtree.drawn_triangle_count += tb->data[quadtree.draw_nodes[ii]].index_count / 3;
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

		tb->quadtree_min = &quadtree.quadtree_min;
		tb->quadtree_max = &quadtree.quadtree_max; 

		// (1 << levels) is number of nodes per axis
		memset(quadtree.node_index_to_buffer_index, INVALID, (1 << quadtree_levels) * (1 << quadtree_levels) * sizeof(uint));

		quadtree.node_size = vec2(quadtree.total_side_length / (1 << quadtree_levels), quadtree.total_side_length / (1 << quadtree_levels));

		tb->quadtree_index_map = quadtree.node_index_to_buffer_index;
	}

	void shift_quadtree(glm::vec3 camera_pos)
	{
		size_t nodes_per_side = (1ull << quadtree_levels);

		bool shifted = false;

		do
		{
			shifted = false;

			if (camera_pos.x + quadtree.quadtree_shift_distance >= tb->quadtree_max->x)
			{
				shifted = true;

				tb->quadtree_min->x += quadtree.node_size.x;
				tb->quadtree_max->x += quadtree.node_size.x;

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
			else if (camera_pos.x - quadtree.quadtree_shift_distance <= tb->quadtree_min->x)
			{
				shifted = true;

				tb->quadtree_min->x -= quadtree.node_size.x;
				tb->quadtree_max->x -= quadtree.node_size.x;

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
			else if (camera_pos.z + quadtree.quadtree_shift_distance >= tb->quadtree_max->y)
			{
				shifted = true;

				tb->quadtree_min->y += quadtree.node_size.y;
				tb->quadtree_max->y += quadtree.node_size.y;

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
			else if (camera_pos.z - quadtree.quadtree_shift_distance <= tb->quadtree_min->y)
			{
				shifted = true;

				tb->quadtree_min->y -= quadtree.node_size.y;
				tb->quadtree_max->y -= quadtree.node_size.y;

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
							triangulate::triangulate(quadtree, tb, tg, index, tri_data);
						}
					}
				}
			}
		}
	}

	void process_triangles(TriData* tri_data)
	{
		triangulation_data = tri_data;

		// Disable child threads
		atomic_total.store(0);

		atomic_started.store(0);
		atomic_finished.store(0);

		atomic_total.store(quadtree.num_draw_nodes + quadtree.num_generate_nodes);

		// Wait until child threads are done processing
		while (atomic_finished.load() < atomic_total.load())
		{
			std::this_thread::sleep_for(std::chrono::nanoseconds(200));
		}
	}


	void worker_thread()
	{
		while (!quit)
		{
			if (atomic_started.load() < atomic_total.load())
			{
				int index = atomic_started.fetch_add(1);

				if (index < atomic_total.load())
				{
					// Process a generated node
					if (index >= int(quadtree.num_draw_nodes))
					{
						index -= quadtree.num_draw_nodes;

						process::triangle_process(
							tb,
							quadtree,
							log_filter,
							triangulation_data->mc_vp,
							vec4(triangulation_data->mc_pos, 0),
							triangulation_data->threshold,
							triangulation_data->area_mult,
							triangulation_data->curv_mult,
							quadtree.generate_nodes[index].index,
							triangulation_data);
					}
					// Process a draw node
					else
					{
						process::triangle_process(
							tb,
							quadtree,
							log_filter,
							triangulation_data->mc_vp,
							vec4(triangulation_data->mc_pos, 0),
							triangulation_data->threshold,
							triangulation_data->area_mult,
							triangulation_data->curv_mult,
							quadtree.draw_nodes[index],
							triangulation_data);
					}

					atomic_finished.fetch_add(1);
				}
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::nanoseconds(200));
			}
		}
	}

	void intersect(Frustum& frustum, DebugDrawer& dd, vec3 camera_pos, TriData* tri_data)
	{
		float half_length = quadtree.total_side_length * 0.5f;
		
		{
			std::unique_lock<std::mutex> lock(shared_data_lock);

			shift_quadtree(camera_pos);

			quadtree.num_generate_nodes = 0;
			quadtree.num_draw_nodes = 0;

			// Gather status of nodes
			intersect(frustum, dd, AabbXZ{ *tb->quadtree_min,
				*tb->quadtree_max }, 0, 0, 0);

			for (uint i = 0; i < quadtree.num_generate_nodes; i++)
			{
				tb->data[quadtree.generate_nodes[i].index].is_invalid = true;
				tb->data[quadtree.generate_nodes[i].index].new_points_count = 0;
			}

		}

		if (quadtree.num_generate_nodes > 0)
		{
			generate::generate(tb, gg, log_filter, tri_data, quadtree.generate_nodes, quadtree.num_generate_nodes);
			do_triangulation = true;
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

	void backup()
	{
		memcpy(backup_tb, tb, sizeof(TerrainBuffer));
		memcpy(&backup_quadtree, &quadtree, sizeof(Quadtree));
	}

	void restore()
	{
		memcpy(tb, backup_tb, sizeof(TerrainBuffer));
		memcpy(&quadtree, &backup_quadtree, sizeof(Quadtree));
	}

	void backup_auto()
	{
		autosaves.push_back(new TerrainBuffer());
		quadtree_autosaves.push_back(new Quadtree());

		memcpy(autosaves[autosaves.size() - 1], tb, sizeof(TerrainBuffer));
		memcpy(quadtree_autosaves[quadtree_autosaves.size() - 1], &quadtree, sizeof(Quadtree));
	}

	void restore_auto(int version)
	{
		if (version < autosaves.size())
		{
			memcpy(tb, autosaves[version], sizeof(TerrainBuffer));
			memcpy(&quadtree, quadtree_autosaves[version], sizeof(Quadtree));
		}
	}

	void file_backup()
	{
		std::ofstream f("terrain_backup", std::ofstream::out | std::ofstream::binary);
		f.write((char*)tb, sizeof(TerrainBuffer));
		f.write((char*)&quadtree, sizeof(Quadtree));
	}

	void file_restore()
	{
		std::ifstream f("terrain_backup", std::ifstream::in | std::ifstream::binary);
		f.read((char*)tb, sizeof(TerrainBuffer));
		f.read((char*)&quadtree, sizeof(Quadtree));

		tb->quadtree_index_map = quadtree.node_index_to_buffer_index;
		tb->quadtree_min = &quadtree.quadtree_min;
		tb->quadtree_max = &quadtree.quadtree_max;
	}

	int get_num_autosaves()
	{
		return (int)autosaves.size();
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
					quadtree.generate_nodes[quadtree.num_generate_nodes].x = x;
					quadtree.generate_nodes[quadtree.num_generate_nodes].y = y;
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

		// DEBUG
		if (tri_data->debug_stage >= 0 && tri_data->debug_version >= 0 && 
			tri_data->debug_version < old_debug_lines.size() && 
			tri_data->debug_stage < old_debug_lines[tri_data->debug_version].size())
		{
			for (uint v = 0; v < old_debug_lines[tri_data->debug_version][tri_data->debug_stage].size(); v += 3)
			{
				tri_data->dd->draw_line(old_debug_lines[tri_data->debug_version][tri_data->debug_stage][v], 
					old_debug_lines[tri_data->debug_version][tri_data->debug_stage][v + 1],
					old_debug_lines[tri_data->debug_version][tri_data->debug_stage][v + 2]);
			}
		}

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
					const float height = 100.0f;


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
						ray_dir = vec3(px, -py, 1);
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

							// Add point if x is pressed
							if (tri_data->x_pressed)
							{
								glm::vec4 v0 = tb->data[ii].positions[tb->data[ii].indices[hovered_triangle * 3 + 0]];
								glm::vec4 v1 = tb->data[ii].positions[tb->data[ii].indices[hovered_triangle * 3 + 1]];
								glm::vec4 v2 = tb->data[ii].positions[tb->data[ii].indices[hovered_triangle * 3 + 2]];

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
								float terrain_y = terrain(glm::vec2(new_pos.x, new_pos.z)) - 0.5f;

								const glm::vec4 point = glm::vec4(new_pos.x, terrain_y, new_pos.z, curvature(vec3(new_pos.x, terrain_y, new_pos.z), log_filter));

								bool add_point = true;

								for (size_t p = 0; p < tb->data[ii].new_points_count; ++p)
								{
									if (tb->data[ii].new_points[p] == point)
									{
										add_point = false;
										break;
									}
								}

								if (add_point)
								{
									tb->data[ii].new_points[tb->data[ii].new_points_count] = point;
									tb->data[ii].new_points_triangles[tb->data[ii].new_points_count] = hovered_triangle;
									tb->data[ii].new_points_count++;
								}
							}
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
							glm::vec3 h = { 0, -1, 0 };

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
						const float height = 102.0f;
						uint ind = tb->data[ii].border_triangle_indices[bt] * 3;
						vec3 p0 = vec3(tb->data[ii].positions[tb->data[ii].indices[ind + 0]]) + vec3(0.0f, height, 0.0f);
						vec3 p1 = vec3(tb->data[ii].positions[tb->data[ii].indices[ind + 1]]) + vec3(0.0f, height, 0.0f);
						vec3 p2 = vec3(tb->data[ii].positions[tb->data[ii].indices[ind + 2]]) + vec3(0.0f, height, 0.0f);

						if (tri_data->sideshow_bob != -1 && tb->data[ii].triangle_connections[ind + 0] == INVALID - tri_data->sideshow_bob)
						{
							tri_data->dd->draw_line(p0 + vec3{ 0, 2, 0 }, p1 + vec3{ 0, 2, 0 }, { 0.0f, 1.0f, 0.0f });
						}
						if (tri_data->sideshow_bob != -1 && tb->data[ii].triangle_connections[ind + 1] == INVALID - tri_data->sideshow_bob)
						{
							tri_data->dd->draw_line(p1 + vec3{ 0, 2, 0 }, p2 + vec3{ 0, 2, 0 }, { 0.0f, 1.0f, 0.0f });
						}
						if (tri_data->sideshow_bob != -1 && tb->data[ii].triangle_connections[ind + 2] == INVALID - tri_data->sideshow_bob)
						{
							tri_data->dd->draw_line(p2 + vec3{ 0, 2, 0 }, p0 + vec3{ 0, 2, 0 }, { 0.0f, 1.0f, 0.0f });
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
