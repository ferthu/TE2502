#pragma once

#include "queue.hpp"

class VulkanContext;

// Represents a hardware queue used for graphics commands
class GraphicsQueue : public Queue
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

private:
	// Move other into this
	void move_from(GraphicsQueue&& other);
};

