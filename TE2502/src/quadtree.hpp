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
	Quadtree(
		VulkanContext& context, 
		float total_side_length, 
		uint32_t levels, 
		VkDeviceSize max_nodes, 
		VkDeviceSize max_node_indices, 
		VkDeviceSize max_node_vertices, 
		VkDeviceSize max_node_new_points, 
		Window& window);

	// Performs frustum culling and draws/generates visible terrain
	void draw_terrain(Frustum& frustum, DebugDrawer& dd, Framebuffer& framebuffer, Camera& camera);

	// Resets all terrain data
	void clear_terrain();

	// Create Vulkan pipelines
	void create_pipelines(Window& window);

	// Re-triangulate the terrain using the new points that have been previously added
	void triangulate();

private:
	struct GenerationData
	{
		glm::mat4 vp;
		glm::vec4 camera_pos;
		glm::vec2 min;			// Min corner
		glm::vec2 max;			// Max corner
		uint32_t buffer_slot;	// Buffer slot
	};

	struct ErrorMetricData
	{
		glm::mat4 vp;
		glm::vec4 camera_pos;
		glm::vec2 screen_size;
	};

	struct TriangulationData
	{
		uint32_t node_index;
	};
	struct BufferNodeHeader
	{
		uint32_t vertex_count;
		uint32_t new_points_count;
		uint32_t pad2[2];
	};
	struct Triangle
	{
		glm::vec2 circumcircle;
		float circumradius;
		uint32_t pad;
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

	// Set up error metric objects
	void error_metric_setup(Window& window);

	GenerationData m_push_data;
	ErrorMetricData m_em_push_data;
	TriangulationData m_triangulation_push_data;

	VulkanContext* m_context;

	// GPU memory used for backing buffer
	GPUMemory m_memory;

	// Contains terrain indices + vertices for quadtree nodes
	GPUBuffer m_buffer;

	GraphicsQueue m_terrain_queue;
	DescriptorSetLayout m_generation_set_layout;
	DescriptorSet m_descriptor_set;
	PipelineLayout m_generation_pipeline_layout;
	PipelineLayout m_triangulation_pipeline_layout;
	std::unique_ptr<Pipeline> m_generation_pipeline;
	std::unique_ptr<Pipeline> m_triangulation_pipeline;

	RenderPass m_render_pass;
	PipelineLayout m_draw_pipeline_layout;
	std::unique_ptr<Pipeline> m_draw_pipeline;

	GPUMemory m_em_memory;
	GPUImage m_em_image;
	GPUImage m_em_depth_image;
	ImageView m_em_image_view;
	ImageView m_em_depth_image_view;
	Framebuffer m_em_framebuffer;
	GraphicsQueue m_em_queue;
	PipelineLayout m_em_pipeline_layout;
	RenderPass m_em_render_pass;
	std::unique_ptr<Pipeline> m_em_pipeline;

	// Max number of indices and vertices per node
	VkDeviceSize m_max_indices;
	VkDeviceSize m_max_vertices;
	VkDeviceSize m_max_node_new_points;

	// Max number of active nodes
	VkDeviceSize m_max_nodes;

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
	VkDeviceSize m_node_memory_size;

	const uint32_t INVALID = ~0u;
};

