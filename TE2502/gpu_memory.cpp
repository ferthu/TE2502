#include <assert.h>

#include "gpu_memory.hpp"
#include "vulkan_context.hpp"
#include "utilities.hpp"


GPUMemory::GPUMemory(VulkanContext& context, uint32_t memory_type, VkDeviceSize byte_size) : m_context(context), m_size(byte_size)
{
	VkMemoryAllocateInfo allocate_info;
	allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocate_info.pNext = nullptr;
	allocate_info.allocationSize = byte_size;
	allocate_info.memoryTypeIndex = memory_type;

	VkResult result = vkAllocateMemory(context.get_device(), &allocate_info, context.get_allocation_callbacks(), &m_memory);
	if (result != VK_SUCCESS)
	{
#ifdef _DEBUG
		__debugbreak();
#endif

		print("GPU memory allocation failed!\n");
		exit(1);
	}
}


GPUMemory::~GPUMemory()
{
	vkFreeMemory(m_context.get_device(), m_memory, m_context.get_allocation_callbacks());
}

void GPUMemory::reset()
{
	m_next_free = 0;
}

VkDeviceMemory GPUMemory::allocate_memory(VkDeviceSize byte_size, VkDeviceSize& output_offset)
{
	if (m_next_free + byte_size > m_size)
	{
#ifdef _DEBUG
		__debugbreak();
#endif

		print("allocate_memory() failed. Not enough space in memory heap!\n");
		exit(1);
	}

	output_offset = m_next_free;
	m_next_free += byte_size;

	return m_memory;
}
