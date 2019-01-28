#include <utility>

#include "compute_queue.hpp"

ComputeQueue::ComputeQueue(VulkanContext& context, VkCommandPool command_pool, VkQueue queue) : Queue(context, command_pool, queue)
{
}


ComputeQueue::~ComputeQueue()
{
	destroy();
}

void ComputeQueue::cmd_image_barrier(
	VkImage image, 
	VkAccessFlags src_access_mask, 
	VkAccessFlags dst_access_mask, 
	VkImageLayout old_layout, 
	VkImageLayout new_layout, 
	VkImageAspectFlags aspect_mask, 
	VkPipelineStageFlags src_stage_mask,
	VkPipelineStageFlags dst_stage_mask)
{
	VkImageSubresourceRange image_subresource_range;
	image_subresource_range.aspectMask = aspect_mask;
	image_subresource_range.baseMipLevel = 0;
	image_subresource_range.levelCount = 1;
	image_subresource_range.baseArrayLayer = 0;
	image_subresource_range.layerCount = 1;


	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext = nullptr;
	barrier.srcAccessMask = src_access_mask;
	barrier.dstAccessMask = dst_access_mask;
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange = image_subresource_range;
	

	vkCmdPipelineBarrier(m_command_buffer, src_stage_mask, dst_stage_mask, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void ComputeQueue::cmd_bind_compute_pipeline(VkPipeline pipeline)
{
	vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
}

void ComputeQueue::cmd_bind_descriptor_set_compute(VkPipelineLayout layout, uint32_t index, VkDescriptorSet descriptor_set)
{
	vkCmdBindDescriptorSets(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, layout, index, 1, &descriptor_set, 0, nullptr);
}

void ComputeQueue::cmd_dispatch(uint32_t x, uint32_t y, uint32_t z)
{
	vkCmdDispatch(m_command_buffer, x, y, z);
}

void ComputeQueue::cmd_push_constants(VkPipelineLayout layout, VkShaderStageFlags stage_flags, uint32_t size, const void* data_pointer)
{
	vkCmdPushConstants(m_command_buffer, layout, stage_flags, 0, size, data_pointer);
}

ComputeQueue::ComputeQueue(ComputeQueue&& other)
{
	move_from(std::move(other));
}

ComputeQueue& ComputeQueue::operator=(ComputeQueue&& other)
{
	if (this != &other)
		move_from(std::move(other));

	return *this;
}

void ComputeQueue::move_from(ComputeQueue&& other)
{
	destroy();
	Queue::move_from(std::move(other));
}

void ComputeQueue::destroy()
{
	Queue::destroy();
}
