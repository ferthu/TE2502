#pragma once

#include "compute_queue.hpp"

class VulkanContext;
class Framebuffer;
class RenderPass;

// Represents a hardware queue used for graphics commands
class GraphicsQueue : public ComputeQueue
{
public:
	GraphicsQueue() {};
	GraphicsQueue(VulkanContext& context, VkCommandPool command_pool, VkQueue queue);
	virtual ~GraphicsQueue();

	GraphicsQueue(GraphicsQueue&& other);
	GraphicsQueue& operator=(GraphicsQueue&& other);

	// Adds an image barrier command to the command buffer
	void cmd_image_barrier(
		VkImage image,
		VkAccessFlags src_access_mask,
		VkAccessFlags dst_access_mask,
		VkImageLayout old_layout,
		VkImageLayout new_layout,
		VkImageAspectFlags aspect_mask,
		VkPipelineStageFlags src_stage_mask,
		VkPipelineStageFlags dst_stage_mask);

	void cmd_clear_color(VkImage image, VkImageLayout current_layout, float r, float g, float b);

	void cmd_clear_depth(VkImage image, VkImageLayout current_layout, float val);

	void cmd_bind_graphics_pipeline(VkPipeline pipeline);

	void cmd_bind_vertex_buffer(VkBuffer buffer, VkDeviceSize offset);

	void cmd_bind_index_buffer(VkBuffer buffer, VkDeviceSize offset);

	void cmd_draw_indirect(VkBuffer buffer, VkDeviceSize offset = 0);

	void cmd_draw_indexed_indirect(VkBuffer buffer, VkDeviceSize offset = 0, uint32_t draw_count = 1, uint32_t stride = 0);

	void cmd_draw(uint32_t num_vertices, uint32_t num_instances = 1, uint32_t vertex_offset = 0, uint32_t instance_offset = 0);

	void cmd_draw_indexed(uint32_t num_indices, uint32_t num_instances = 1, uint32_t index_offset = 0, uint32_t vertex_offset = 0, uint32_t instance_offset = 0);

	void cmd_begin_render_pass(RenderPass& render_pass, Framebuffer& framebuffer);

	void cmd_end_render_pass();


private:
	// Move other into this
	void move_from(GraphicsQueue&& other);
	
	// Destroys object
	void destroy();
};

