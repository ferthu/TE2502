#include <utility>

#include "graphics_queue.hpp"
#include "framebuffer.hpp"
#include "render_pass.hpp"

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
	barrier.srcQueueFamilyIndex = 0;
	barrier.dstQueueFamilyIndex = 0;
	barrier.image = image;
	barrier.subresourceRange = image_subresource_range;

	vkCmdPipelineBarrier(m_command_buffer, src_stage_mask, dst_stage_mask, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void GraphicsQueue::cmd_bind_graphics_pipeline(VkPipeline pipeline)
{
	vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

void GraphicsQueue::cmd_bind_vertex_buffer(VkBuffer buffer, VkDeviceSize offset)
{
	vkCmdBindVertexBuffers(m_command_buffer, 0, 1, &buffer, &offset);
}

void GraphicsQueue::cmd_draw_indirect(VkBuffer buffer)
{
	vkCmdDrawIndirect(m_command_buffer, buffer, 0, 1, 0);
}

void GraphicsQueue::cmd_begin_render_pass(RenderPass& render_pass, Framebuffer& framebuffer)
{
	VkRenderPassBeginInfo begin_info;
	begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	begin_info.pNext = nullptr;
	begin_info.renderPass = render_pass.get_render_pass();
	begin_info.framebuffer = framebuffer.get_framebuffer();
	begin_info.renderArea.offset = { 0, 0 };
	begin_info.renderArea.extent = { framebuffer.get_width(), framebuffer.get_height() };
	begin_info.clearValueCount = 1;
	VkClearValue clear_value;
	clear_value.color.float32[0] = 0.0f;
	clear_value.color.float32[1] = 0.0f;
	clear_value.color.float32[2] = 0.0f;
	clear_value.color.float32[3] = 0.0f;
	clear_value.depthStencil.depth = 0.0f;
	clear_value.depthStencil.stencil = 0;
	begin_info.pClearValues = &clear_value;
	vkCmdBeginRenderPass(m_command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void GraphicsQueue::cmd_end_render_pass()
{
	vkCmdEndRenderPass(m_command_buffer);
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
