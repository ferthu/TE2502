#pragma once

#include "queue.hpp"

class VulkanContext;

// Represents a hardware queue used for compute commands
class ComputeQueue : public Queue
{
public:
	ComputeQueue() {};
	ComputeQueue(VulkanContext& context, VkCommandPool command_pool, VkQueue queue);
	virtual ~ComputeQueue();

	ComputeQueue(ComputeQueue&& other);
	ComputeQueue& operator=(ComputeQueue&& other);

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

	// Binds a compute pipeline to the command buffer
	void cmd_bind_compute_pipeline(VkPipeline pipeline);

	// Bind a descriptor set to the specified slot in the compute pipeline
	void cmd_bind_descriptor_set_compute(VkPipelineLayout layout, uint32_t index, VkDescriptorSet descriptor_set);

	// Run the compute pipeline with the specified global group sizes
	void cmd_dispatch(uint32_t x, uint32_t y, uint32_t z);

private:
	// Move other into this
	void move_from(ComputeQueue&& other);
	
	// Destroys object
	void destroy();
};

