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

	// Adds a buffer barrier command to the command buffer
	void cmd_buffer_barrier(
		VkBuffer buffer,
		VkAccessFlags src_access_mask,
		VkAccessFlags dst_access_mask,
		VkPipelineStageFlags src_stage_mask,
		VkPipelineStageFlags dst_stage_mask,
		VkDeviceSize offset = 0,
		VkDeviceSize size = VK_WHOLE_SIZE);

	// Binds a compute pipeline to the command buffer
	void cmd_bind_compute_pipeline(VkPipeline pipeline);

	// Bind a descriptor set to the specified slot in the compute pipeline
	void cmd_bind_descriptor_set_compute(VkPipelineLayout layout, uint32_t index, VkDescriptorSet descriptor_set);

	// Run the compute pipeline with the specified global group sizes
	void cmd_dispatch(uint32_t x, uint32_t y, uint32_t z);

	// Upload push constant range to pipeline
	void cmd_push_constants(VkPipelineLayout layout, VkShaderStageFlags stage_flags, uint32_t size, const void* data_pointer);

	void cmd_copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size, VkDeviceSize src_offset = 0, VkDeviceSize dst_offset = 0);

	void cmd_copy_image(VkImage src, VkImage dst, VkImageLayout src_layout, VkImageLayout dst_layout, VkExtent3D size);

	void cmd_pipeline_barrier();

protected:
	// Move other into this
	void move_from(ComputeQueue&& other);
	
	// Destroys object
	void destroy();
};

