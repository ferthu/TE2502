#pragma once

#include "compute_queue.hpp"

class VulkanContext;

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

	void cmd_bind_graphics_pipeline(VkPipeline pipeline);

private:
	// Move other into this
	void move_from(GraphicsQueue&& other);
	
	// Destroys object
	void destroy();
};

