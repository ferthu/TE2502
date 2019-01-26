#pragma once

#include "gpu_memory.hpp"

class VulkanContext;

// Represents a Vulkan buffer object
class GPUBuffer
{
public:
	GPUBuffer() {};
	GPUBuffer(VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage, GPUMemory& memory_heap);
	~GPUBuffer();

	GPUBuffer(GPUBuffer&& other);
	GPUBuffer& operator=(GPUBuffer&& other);

	VkBuffer get_buffer() { return m_buffer; }
	VkDeviceSize get_size() { return m_size; }
	VkBufferUsageFlags get_usage() { return m_usage; }

private:
	// Move other into this
	void move_from(GPUBuffer&& other);
	
	// Destroys object
	void destroy();

	VulkanContext* m_context;

	VkBuffer m_buffer = VK_NULL_HANDLE;

	// Size of buffer, in bytes
	VkDeviceSize m_size;

	// Usage flags
	VkBufferUsageFlags m_usage;
};

// An object that interprets the data of a buffer with a specific format
class BufferView
{
public:
	BufferView() {};
	BufferView(VulkanContext& context, GPUBuffer& buffer, VkFormat format);
	~BufferView();

	BufferView(BufferView&& other);
	BufferView& operator=(BufferView&& other);

	// Returns stored buffer view
	VkBufferView get_view() { return m_buffer_view; }

	VkBuffer get_buffer() { return m_buffer->get_buffer(); }

	VkFormat get_format() { return m_format; }
private:
	// Move other into this
	void move_from(BufferView&& other);

	// Destroys object
	void destroy();

	VulkanContext* m_context;

	GPUBuffer* m_buffer;

	VkBufferView m_buffer_view = VK_NULL_HANDLE;

	VkFormat m_format;
};