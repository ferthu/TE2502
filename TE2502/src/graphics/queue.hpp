#pragma once

#include <vulkan/vulkan.h>

class VulkanContext;

class Queue
{
public:
	Queue() {};
	Queue(VulkanContext& context, VkCommandPool command_pool, VkQueue queue);
	virtual ~Queue();

	Queue(Queue&& other);
	Queue& operator=(Queue&& other);

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

	// Check if submitted work is complete
	bool is_done();

	// Returns true if currently recording
	bool is_recording();

	VkQueue get_queue() const;

	VkFence get_fence() const;

protected:
	// Move other into this
	void move_from(Queue&& other);
	
	// Destroys object
	void destroy();

	// Current Vulkan context
	VulkanContext* m_context;

	// Handle to the command pool that allocates the command buffer
	VkCommandPool m_command_pool = VK_NULL_HANDLE;

	// The queue to submit commands to
	VkQueue m_queue = VK_NULL_HANDLE;

	// Vulkan Command buffer object
	VkCommandBuffer m_command_buffer = VK_NULL_HANDLE;

	// Fence that is signaled when queue submission is complete
	VkFence m_fence = VK_NULL_HANDLE;

	// True if recording is currently enabled
	bool m_recording;

	// True if recording is complete
	bool m_has_recorded;
};

