#pragma once

#include <vulkan/vulkan.h>

#include "graphics/debug_drawer.hpp"
#include "math/geometry.hpp"
#include "graphics/gpu_memory.hpp"
#include "graphics/gpu_buffer.hpp"
#include "graphics/graphics_queue.hpp"
#include "graphics/vulkan_context.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/pipeline_layout.hpp"
#include "graphics/descriptor_set_layout.hpp"
#include "graphics/descriptor_set.hpp"
#include "graphics/render_pass.hpp"
#include "graphics/framebuffer.hpp"
#include "graphics/window.hpp"
#include "camera.hpp"

class Quadtree
{
public:
	Quadtree() {}
	~Quadtree();

	Quadtree(Quadtree&& other);
	Quadtree& operator=(Quadtree&& other);

	Quadtree(VulkanContext& context, float total_side_length, uint32_t levels, uint32_t max_nodes, uint32_t max_node_indices, uint32_t max_node_vertices, Window& window);

	// Performs frustum culling and draws/generates visible terrain
	void draw_terrain(Frustum& frustum, DebugDrawer& dd, Framebuffer& framebuffer, Camera& camera);

	// Resets all terrain data
	void clear_terrain();

private:
	struct GenerationData
	{
		glm::mat4 vp;
		glm::vec4 camera_pos;
		glm::vec2 min;			// Min corner
		glm::vec2 max;			// Max corner
		uint32_t buffer_slot;	// Buffer slot
	};

	// Move other into this
	void move_from(Quadtree&& other);

	// Destroys object
	void destroy();

	// Recursive intersection
	void intersect(Frustum& frustum, DebugDrawer& dd, AabbXZ aabb, uint32_t level, uint32_t x, uint32_t y);

	// Finds a free chunk in m_buffer and returns it index, or INVALID if none was found
	uint32_t find_chunk();

	// For a node at the given position, return its index into m_buffer
	uint32_t get_offset(uint32_t node_x, uint32_t node_z);

	GenerationData m_push_data;

	VulkanContext* m_context;

	// GPU memory used for backing buffer
	GPUMemory m_memory;

	// Contains terrain indices + vertices for quadtree nodes
	GPUBuffer m_buffer;

	GraphicsQueue m_terrain_queue;
	DescriptorSetLayout m_generation_set_layout;
	DescriptorSet m_descriptor_set;
	PipelineLayout m_generation_pipeline_layout;
	std::unique_ptr<Pipeline> m_generation_pipeline;

	RenderPass m_render_pass;
	PipelineLayout m_draw_pipeline_layout;
	std::unique_ptr<Pipeline> m_draw_pipeline;

	// Max number of indices and vertices per node
	uint32_t m_max_indices;
	uint32_t m_max_vertices;

	// Max number of active nodes
	uint32_t m_max_nodes;

	// Size and number of levels in quadtree
	float m_total_side_length;
	uint32_t m_levels;

	// For every possible node, store an index into m_buffer. The chunk of m_buffer pointed to contains mesh data for that node
	uint32_t* m_node_index_to_buffer_index;

	// For chunk i of m_buffer, m_buffer_index_filled[i] is true if that chunk is used by a node
	bool* m_buffer_index_filled;

	struct GenerateInfo
	{
		glm::vec2 min;
		glm::vec2 max;
		uint32_t index;
	};

	// Number and array of indices to nodes that needs to generate terrain
	uint32_t m_num_generate_nodes;
	GenerateInfo* m_generate_nodes;

	// Number and array of indices to nodes that needs to draw terrain
	uint32_t m_num_draw_nodes;
	uint32_t* m_draw_nodes;

	// Number of bytes in buffer per node
	uint32_t m_node_memory_size;

	const uint32_t INVALID = ~0u;
};

