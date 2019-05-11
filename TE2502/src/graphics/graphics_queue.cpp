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
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange = image_subresource_range;

	vkCmdPipelineBarrier(m_command_buffer, src_stage_mask, dst_stage_mask, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void GraphicsQueue::cmd_clear_color(VkImage image, VkImageLayout current_layout, float r, float g, float b)
{
	VkImageSubresourceRange range;
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.baseArrayLayer = 0;
	range.layerCount = 1;
	range.baseMipLevel = 0;
	range.levelCount = 1;

	VkClearColorValue col;
	col.float32[0] = r;
	col.float32[1] = g;
	col.float32[2] = b;
	col.float32[3] = 1.0f;

	vkCmdClearColorImage(m_command_buffer, image, current_layout, &col, 1, &range);
}

void GraphicsQueue::cmd_clear_depth(VkImage image, VkImageLayout current_layout, float val)
{
	VkImageSubresourceRange range;
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.baseArrayLayer = 0;
	range.layerCount = 1;
	range.baseMipLevel = 0;
	range.levelCount = 1;

	VkClearDepthStencilValue dep;
	dep.depth = val;
	dep.stencil = 0;

	vkCmdClearDepthStencilImage(m_command_buffer, image, current_layout, &dep, 1, &range);
}

void GraphicsQueue::cmd_bind_graphics_pipeline(VkPipeline pipeline)
{
	vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

void GraphicsQueue::cmd_bind_vertex_buffer(VkBuffer buffer, VkDeviceSize offset)
{
	vkCmdBindVertexBuffers(m_command_buffer, 0, 1, &buffer, &offset);
}

void GraphicsQueue::cmd_bind_index_buffer(VkBuffer buffer, VkDeviceSize offset)
{
	vkCmdBindIndexBuffer(m_command_buffer, buffer, offset, VkIndexType::VK_INDEX_TYPE_UINT32);
}

void GraphicsQueue::cmd_draw_indirect(VkBuffer buffer, VkDeviceSize offset)
{
	vkCmdDrawIndirect(m_command_buffer, buffer, offset, 1, 0);
}

void GraphicsQueue::cmd_draw_indexed_indirect(VkBuffer buffer, VkDeviceSize offset, uint32_t draw_count, uint32_t stride)
{
	vkCmdDrawIndexedIndirect(m_command_buffer, buffer, offset, draw_count, stride);
}

void GraphicsQueue::cmd_draw(uint32_t num_vertices, uint32_t num_instances, uint32_t vertex_offset, uint32_t instance_offset)
{
	vkCmdDraw(m_command_buffer, num_vertices, num_instances, vertex_offset, instance_offset);
}

void GraphicsQueue::cmd_draw_indexed(uint32_t num_indices, uint32_t num_instances, uint32_t index_offset, uint32_t vertex_offset, uint32_t instance_offset)
{
	vkCmdDrawIndexed(m_command_buffer, num_indices, num_instances, index_offset, vertex_offset, instance_offset);
}

float post_process(float color)
{
	return (1.0f - expf(-color * 6.0f)) * 1.0024f;
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
	begin_info.clearValueCount = 2;
	VkClearValue clear_value[2];
	clear_value[0].color.float32[0] = post_process(0.1f);
	clear_value[0].color.float32[1] = post_process(0.15f);
	clear_value[0].color.float32[2] = post_process(0.3f);
	clear_value[0].color.float32[3] = 1.0f;
	clear_value[1].color.float32[0] = 0.0f;
	clear_value[1].color.float32[1] = 0.0f;
	clear_value[1].color.float32[2] = 0.0f;
	clear_value[1].color.float32[3] = 0.0f;
	clear_value[1].depthStencil.depth = 1.0f;
	clear_value[1].depthStencil.stencil = 0;
	begin_info.pClearValues = clear_value;
	vkCmdBeginRenderPass(m_command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void GraphicsQueue::cmd_end_render_pass()
{
	vkCmdEndRenderPass(m_command_buffer);
}

void GraphicsQueue::cmd_bind_descriptor_set(VkPipelineLayout layout, uint32_t index, VkDescriptorSet descriptor_set)
{
	vkCmdBindDescriptorSets(m_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, index, 1, &descriptor_set, 0, nullptr);
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
