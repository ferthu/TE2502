#include <assert.h>

#include "queue.hpp"

Queue::Queue(VulkanContext& context, VkCommandPool command_pool, VkQueue queue) : m_context(context), m_command_pool(command_pool), m_queue(queue), m_recording(false), m_has_recorded(false)
{
	// Create the command buffer
	VkCommandBufferAllocateInfo alloc_info;
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.pNext = nullptr;
	alloc_info.commandPool = command_pool;
	alloc_info.level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;

	VkResult result = vkAllocateCommandBuffers(m_context.get_device(), &alloc_info, &m_command_buffer);
	assert(result == VK_SUCCESS);

	// Create fence
	VkFenceCreateInfo fence_info;
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.pNext = nullptr;
	fence_info.flags = VkFenceCreateFlagBits::VK_FENCE_CREATE_SIGNALED_BIT;

	vkCreateFence(m_context.get_device(), &fence_info, m_context.get_allocation_callbacks(), &m_fence);
}


Queue::~Queue()
{
	vkDestroyFence(m_context.get_device(), m_fence, m_context.get_allocation_callbacks());

	vkFreeCommandBuffers(m_context.get_device(), m_command_pool, 1, &m_command_buffer);
}

void Queue::reset()
{
	assert(!m_recording);
	vkResetCommandBuffer(m_command_buffer, 0);
	m_has_recorded = false;
}

void Queue::submit() 
{
	assert(!m_recording && m_has_recorded);

	vkResetFences(m_context.get_device(), 1, &m_fence);

	VkSubmitInfo submit_info;
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = nullptr;
	submit_info.waitSemaphoreCount = 0;
	submit_info.pWaitSemaphores = nullptr;
	submit_info.pWaitDstStageMask = nullptr;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &m_command_buffer;
	submit_info.signalSemaphoreCount = 0;
	submit_info.pSignalSemaphores = nullptr;

	vkQueueSubmit(m_queue, 1, &submit_info, m_fence);
}

void Queue::start_recording()
{
	assert(!m_recording && !m_has_recorded);
	m_recording = true;

	VkCommandBufferBeginInfo begin_info;
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = nullptr;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = nullptr;

	vkBeginCommandBuffer(m_command_buffer, &begin_info);
}

void Queue::end_recording()
{
	assert(m_recording && !m_has_recorded);
	m_recording = false;
	m_has_recorded = true;

	vkEndCommandBuffer(m_command_buffer);
}

void Queue::wait()
{
	vkWaitForFences(m_context.get_device(), 1, &m_fence, VK_FALSE, ~0ull);
}
