#pragma once

#include <vulkan/vulkan.h>

class VulkanContext;

// Represents a chunk of GPU memory that can be used to store buffers/images
class GPUMemory
{
public:
	// memory_type is the index of memory type in Vulkan's physical device
	GPUMemory(VulkanContext& context, uint32_t memory_type, VkDeviceSize byte_size);
	~GPUMemory();

	// Resets allocations. All resources using the memory will be invalidated
	void reset();

	// Allocates memory of the given size. Offset into VkDeviceMemory returned is written to output_offset
	VkDeviceMemory allocate_memory(VkDeviceSize byte_size, VkDeviceSize& output_offset);

	// Returns memory type this object was created from
	uint32_t get_memory_type() { return m_memory_type; }

private:
	// Reference to Vulkan context
	VulkanContext& m_context;

	// The Vulkan allocated memory object
	VkDeviceMemory m_memory;

	// Size of allocated memory
	VkDeviceSize m_size;

	// Offset of the next free memeory
	VkDeviceSize m_next_free;

	// The memory type this memory object was created from
	uint32_t m_memory_type;
};

