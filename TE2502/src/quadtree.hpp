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
		Window& window,
		GraphicsQueue& queue);

	// Recursive intersection that gathers data on what needs to be generated or drawn
	void intersect(GraphicsQueue& queue, Frustum& frustum, DebugDrawer& dd);

	// Performs frustum culling and draws/generates visible terrain
	void draw_terrain(GraphicsQueue& queue, Frustum& frustum, DebugDrawer& dd, Framebuffer& framebuffer, Camera& camera, bool wireframe);

	// Adds new vertices to terrain buffer when needed
	void process_triangles(GraphicsQueue& queue, Camera& camera, Window& window, float em_threshold, float area_multiplier, float curvature_multiplier);

	// Performs frustum culling and draws/generates visible terrain to error metric image
	void draw_error_metric(GraphicsQueue& queue, Frustum& frustum, DebugDrawer& dd, Framebuffer& framebuffer, Camera& camera, bool draw_to_screen, float area_multiplier, float curvature_multiplier, bool wireframe);

	// Resets all terrain data
	void clear_terrain();

	// Create Vulkan pipelines
	void create_pipelines(Window& window);

	// Re-triangulate the terrain using the new points that have been previously added
	void triangulate(GraphicsQueue& queue);

	// Handle borders
	void handle_borders(GraphicsQueue& queue);

	PipelineLayout& get_triangle_processing_layout();

	// Return the image view of the error metric image
	ImageView& get_em_image_view();

	GPUBuffer& get_buffer();

	GPUImage& get_em_image();

	GPUImage& get_em_depth_image();

	RenderPass& get_render_pass();

private:
	struct GenerationData
	{
		glm::mat4 vp;
		glm::vec4 camera_pos;
		glm::vec2 min;			// Min corner
		glm::vec2 max;			// Max corner
		uint32_t node_index;    // Previouly ("buffer_slot")
	};

	struct ErrorMetricData
	{
		glm::mat4 vp;
		glm::vec4 camera_pos;
		glm::vec2 screen_size;
		float area_multiplier;
		float curvature_multiplier;
	};

	struct TriangleProcessingFrameData
	{
		glm::mat4 vp;
		glm::vec4 camera_position;
		glm::vec2 screen_size;
		float em_threshold;
		float area_multiplier;
		float curvature_multiplier;
		uint32_t node_index;
	};

	struct TriangulationData
	{
		uint32_t node_index;
	}; 
#define MAX_BORDER_TRIANGLE_COUNT 500
#define TRIANGULATE_MAX_NEW_BORDER_POINTS 500
	struct BufferNodeHeader
	{
		uint32_t vertex_count;
		uint32_t new_points_count;
		uint32_t pad;

		glm::vec2 min;
		glm::vec2 max;

		uint32_t new_border_point_count[4];
		glm::vec4 new_border_points[4 * TRIANGULATE_MAX_NEW_BORDER_POINTS];
		float border_max[4];
		uint32_t border_count[4];
		uint32_t border_triangle_indices[4 * MAX_BORDER_TRIANGLE_COUNT];
		float border_diffs[4 * MAX_BORDER_TRIANGLE_COUNT];
	};
	struct Triangle
	{
		glm::vec2 circumcircle;
		float circumradius;
		float circumradius2;
	};

	// Move other into this
	void move_from(Quadtree&& other);

	// Destroys object
	void destroy();

	void intersect(Frustum& frustum, DebugDrawer& dd, AabbXZ aabb, uint32_t level, uint32_t x, uint32_t y);

	// Finds a free chunk in m_buffer and returns it index, or INVALID if none was found
	uint32_t find_chunk();

	// For a node at the given position, return its index into m_buffer
	uint32_t get_offset(uint32_t node_x, uint32_t node_z);

	// Set up error metric objects
	void error_metric_setup(Window& window, GraphicsQueue& queue);

	// Get offset for indices for index i in m_buffer
	VkDeviceSize get_index_offset_of_node(uint32_t i);

	// Get offset for indices for index i in m_buffer
	VkDeviceSize get_vertex_offset_of_node(uint32_t i);

	// Get offset for indices for index i in m_buffer
	VkDeviceSize get_offset_of_node(uint32_t i);

	GenerationData m_push_data;
	ErrorMetricData m_em_push_data;
	TriangulationData m_triangulation_push_data;

	VulkanContext* m_context;

	// GPU memory used for backing buffer
	GPUMemory m_memory;

	// Contains terrain indices + vertices for quadtree nodes
	GPUBuffer m_buffer;

	TriangleProcessingFrameData m_triangle_processing_frame_data;

	DescriptorSetLayout m_generation_set_layout;
	DescriptorSet m_descriptor_set;
	PipelineLayout m_generation_pipeline_layout;
	PipelineLayout m_triangulation_pipeline_layout;
	std::unique_ptr<Pipeline> m_generation_pipeline;
	std::unique_ptr<Pipeline> m_triangulation_pipeline;
	std::unique_ptr<Pipeline> m_border_handling_pipeline;

	RenderPass m_render_pass;
	PipelineLayout m_draw_pipeline_layout;
	std::unique_ptr<Pipeline> m_draw_pipeline;
	std::unique_ptr<Pipeline> m_draw_wireframe_pipeline;

	GPUMemory m_em_memory;
	GPUImage m_em_image;
	GPUImage m_em_depth_image;
	ImageView m_em_image_view;
	ImageView m_em_depth_image_view;
	Framebuffer m_em_framebuffer;
	PipelineLayout m_em_pipeline_layout;
	RenderPass m_em_render_pass;
	std::unique_ptr<Pipeline> m_em_pipeline;
	std::unique_ptr<Pipeline> m_em_wireframe_pipeline;

	// Triangle processing
	DescriptorSetLayout m_triangle_processing_layout;
	DescriptorSet m_triangle_processing_set;
	PipelineLayout m_triangle_processing_pipeline_layout;
	std::unique_ptr<Pipeline> m_triangle_processing_compute_pipeline;

	// Max number of indices and vertices per node
	VkDeviceSize m_max_indices;
	VkDeviceSize m_max_vertices;
	VkDeviceSize m_max_node_new_points;

	// Max number of active nodes
	VkDeviceSize m_max_nodes;

	// Size and number of levels in quadtree
	float m_total_side_length;
	uint32_t m_levels;


	// CPU buffer for m_node_index_to_buffer_index
	GPUBuffer m_cpu_index_buffer;

	// For every possible node, store an index into m_buffer. The chunk of m_buffer pointed to contains mesh data for that node
	uint32_t* m_node_index_to_buffer_index;
	GPUMemory m_cpu_index_buffer_memory;
	VkDeviceSize m_cpu_index_buffer_size;
	glm::vec2* m_quadtree_minmax;

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

