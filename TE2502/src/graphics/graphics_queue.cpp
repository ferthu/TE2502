#include <utility>

#include "graphics_queue.hpp"

GraphicsQueue::GraphicsQueue(VulkanContext& context, VkCommandPool command_pool, VkQueue queue) : ComputeQueue(context, command_pool, queue)
{
}


GraphicsQueue::~GraphicsQueue()
{
	destroy();
}

void GraphicsQueue::cmd_image_barrier(
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
	barrier.subresourceRange = image_subresource_range;

	vkCmdPipelineBarrier(m_command_buffer, src_stage_mask, dst_stage_mask, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void GraphicsQueue::cmd_buffer_barrier(VkBuffer buffer, VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask, VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask)
{
	VkBufferMemoryBarrier barrier;
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.pNext = nullptr;
	barrier.srcAccessMask = src_access_mask;
	barrier.dstAccessMask = dst_access_mask;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer = buffer;
	barrier.offset = 0;
	barrier.size = VK_WHOLE_SIZE;

	vkCmdPipelineBarrier(m_command_buffer, src_stage_mask, dst_stage_mask, 0, 0, nullptr, 1, &barrier, 0, nullptr);
}

void GraphicsQueue::cmd_bind_graphics_pipeline(VkPipeline pipeline)
{
	vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

void GraphicsQueue::cmd_copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size, VkDeviceSize src_offset, VkDeviceSize dst_offset)
{
	VkBufferCopy buffer_copy;
	buffer_copy.size = size;
	buffer_copy.srcOffset = src_offset;
	buffer_copy.dstOffset = dst_offset;

	vkCmdCopyBuffer(m_command_buffer, src, dst, 1, &buffer_copy);
}

GraphicsQueue::GraphicsQueue(GraphicsQueue&& other)
{
	move_from(std::move(other));
}

GraphicsQueue& GraphicsQueue::operator=(GraphicsQueue&& other)
{
	if (this != &other)
		move_from(std::move(other));

	return *this;
}

void GraphicsQueue::move_from(GraphicsQueue&& other)
{
	destroy();
	ComputeQueue::move_from(std::move(other));
}

void GraphicsQueue::destroy()
{
	ComputeQueue::destroy();
}
