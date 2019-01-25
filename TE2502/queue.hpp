#pragma once

#include <vulkan/vulkan.h>

class VulkanContext;

class Queue
{
public:
	Queue(VulkanContext& context, VkCommandPool command_pool, VkQueue queue);
	virtual ~Queue();

	// Resets the Vulkan queue to allow for recording and submitting again
	void reset();

	// Submit recorded work to GPU
	void submit();

	// Begins recording commands
	void start_recording();

	// End command recording, allowing submission to GPU
	void end_recording();

	// Waits until submitted work is complete
	void wait();

	VkQueue get_queue() const;

protected:
	// Current Vulkan context
	VulkanContext& m_context;

	// Handle to the command pool that allocates the command buffer
	VkCommandPool m_command_pool;

	// The queue to submit commands to
	VkQueue m_queue;

	// Vulkan Command buffer object
	VkCommandBuffer m_command_buffer;

	// Fence that is signaled when queue submission is complete
	VkFence m_fence;

	// True if recording is currently enabled
	bool m_recording;

	// True if recording is complete
	bool m_has_recorded;
};

