#pragma once

#include "gpu_memory.hpp"

class VulkanContext;

// Represents a Vulkan buffer object
class GPUBuffer
{
public:
	GPUBuffer(VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage, GPUMemory& memory_heap);
	~GPUBuffer();

	VkBuffer get_buffer() { return m_buffer; }
	VkDeviceSize get_size() { return m_size; }
	VkBufferUsageFlags get_usage() { return m_usage; }

private:
	VulkanContext& m_context;

	VkBuffer m_buffer;

	// Size of buffer, in bytes
	VkDeviceSize m_size;

	// Usage flags
	VkBufferUsageFlags m_usage;
};

// An object that interprets the data of a buffer with a specific format
class BufferView
{
public:
	BufferView(VulkanContext& context, GPUBuffer& buffer, VkFormat format);
	~BufferView();

	// Returns stored buffer view
	VkBufferView get_view() { return m_buffer_view; }

	VkBuffer get_buffer() { return m_buffer.get_buffer(); }

	VkFormat get_format() { return m_format; }
private:
	VulkanContext& m_context;

	GPUBuffer& m_buffer;

	VkBufferView m_buffer_view;

	VkFormat m_format;
};