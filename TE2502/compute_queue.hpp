#pragma once

#include "queue.hpp"

class VulkanContext;

// Represents a hardware queue used for compute commands
class ComputeQueue : public Queue
{
public:
	ComputeQueue(VulkanContext& context, VkCommandPool command_pool, VkQueue queue);
	virtual ~ComputeQueue();

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
};

