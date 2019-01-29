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

	// Adds a buffer barrier command to the command buffer
	void cmd_buffer_barrier(
		VkImage image,
		VkAccessFlags src_access_mask,
		VkAccessFlags dst_access_mask,
		VkImageLayout old_layout,
		VkImageLayout new_layout,
		VkImageAspectFlags aspect_mask,
		VkPipelineStageFlags src_stage_mask,
		VkPipelineStageFlags dst_stage_mask);

	void cmd_bind_graphics_pipeline(VkPipeline pipeline);

	void cmd_bind_vertex_buffer(VkBuffer buffer, VkDeviceSize offset);

	void cmd_draw_indirect(VkBuffer buffer);

	void cmd_begin_render_pass(RenderPass& render_pass, Framebuffer& framebuffer);

	void cmd_end_render_pass();

	void cmd_copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size, VkDeviceSize src_offset = 0, VkDeviceSize dst_offset = 0);

private:
	// Move other into this
	void move_from(GraphicsQueue&& other);
	
	// Destroys object
	void destroy();
};

