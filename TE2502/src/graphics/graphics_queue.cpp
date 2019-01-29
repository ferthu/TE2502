#include <utility>

#include "graphics_queue.hpp"

GraphicsQueue::GraphicsQueue(VulkanContext& context, VkCommandPool command_pool, VkQueue queue) : Queue(context, command_pool, queue)
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
	barrier.srcQueueFamilyIndex = 0;
	barrier.dstQueueFamilyIndex = 0;
	barrier.subresourceRange = image_subresource_range;

	vkCmdPipelineBarrier(m_command_buffer, src_stage_mask, dst_stage_mask, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void GraphicsQueue::cmd_bind_graphics_pipeline(VkPipeline pipeline)
{
	vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
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
