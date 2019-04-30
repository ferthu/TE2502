#include <assert.h>

#include "vulkan_context.hpp"
#include "queue.hpp"
#include "utilities.hpp"


Queue::Queue(VulkanContext& context, VkCommandPool command_pool, VkQueue queue) 
	: m_context(&context), m_command_pool(command_pool), m_queue(queue), m_recording(false), m_has_recorded(false)
{
	// Create the command buffer
	VkCommandBufferAllocateInfo alloc_info;
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.pNext = nullptr;
	alloc_info.commandPool = command_pool;
	alloc_info.level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;

	VkResult result = vkAllocateCommandBuffers(m_context->get_device(), &alloc_info, &m_command_buffer);
	assert(result == VK_SUCCESS);

	// Create fence
	VkFenceCreateInfo fence_info;
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.pNext = nullptr;
	fence_info.flags = VkFenceCreateFlagBits::VK_FENCE_CREATE_SIGNALED_BIT;
	
	VK_CHECK(vkCreateFence(m_context->get_device(), &fence_info, m_context->get_allocation_callbacks(), &m_fence), "Failed to create queue fence");
}


Queue::~Queue()
{
	destroy();
}

Queue::Queue(Queue&& other)
{
	move_from(std::move(other));
}

Queue& Queue::operator=(Queue&& other)
{
	if (this != &other)
		move_from(std::move(other));
	
	return *this;
}

bool Queue::is_done()
{
	assert(!m_recording);

	VkResult result = vkGetFenceStatus(m_context->get_device(), m_fence);

	return result == VK_SUCCESS;
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

	VK_CHECK(vkResetFences(m_context->get_device(), 1, &m_fence), "Failed to free fence");

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
	reset();
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
	vkWaitForFences(m_context->get_device(), 1, &m_fence, VK_FALSE, ~0ull);
}

bool Queue::is_recording()
{
	return m_recording;
}

VkQueue Queue::get_queue() const
{
	return m_queue;
}

VkFence Queue::get_fence() const
{
	return m_fence;
}

void Queue::move_from(Queue&& other)
{
	destroy();

	m_context = other.m_context;

	m_command_pool = other.m_command_pool;
	other.m_command_pool = VK_NULL_HANDLE;

	m_queue = other.m_queue;
	other.m_queue = VK_NULL_HANDLE;

	m_command_buffer = other.m_command_buffer;
	other.m_command_buffer = VK_NULL_HANDLE;

	m_fence = other.m_fence;
	other.m_fence = VK_NULL_HANDLE;

	m_recording = other.m_recording;
	m_has_recorded = other.m_has_recorded;
}

void Queue::destroy()
{
	if (m_fence != VK_NULL_HANDLE)
	{
		vkDestroyFence(m_context->get_device(), m_fence, m_context->get_allocation_callbacks());
		m_fence = VK_NULL_HANDLE;
	}

	if (m_command_buffer != VK_NULL_HANDLE)
	{
		vkFreeCommandBuffers(m_context->get_device(), m_command_pool, 1, &m_command_buffer);
		m_command_buffer = VK_NULL_HANDLE;
	}
}
