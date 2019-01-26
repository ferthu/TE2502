#include <assert.h>
#include <intrin.h>

#include "gpu_memory.hpp"
#include "vulkan_context.hpp"
#include "utilities.hpp"


GPUMemory::GPUMemory(VulkanContext& context, uint32_t memory_type, VkDeviceSize byte_size) : m_context(&context), m_size(byte_size), m_memory_type(memory_type)
{
	VkMemoryAllocateInfo allocate_info;
	allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocate_info.pNext = nullptr;
	allocate_info.allocationSize = byte_size;
	allocate_info.memoryTypeIndex = memory_type;

	VK_CHECK(vkAllocateMemory(context.get_device(), &allocate_info, context.get_allocation_callbacks(), &m_memory), "GPU memory allocation failed!");
}


GPUMemory::~GPUMemory()
{
	destroy();
}

GPUMemory::GPUMemory(GPUMemory&& other)
{
	move_from(std::move(other));
}

GPUMemory& GPUMemory::operator=(GPUMemory&& other)
{
	if (this != &other)
	{
		move_from(std::move(other));
	}

	return *this;
}

void GPUMemory::reset()
{
	m_next_free = 0;
}

VkDeviceMemory GPUMemory::allocate_memory(VkDeviceSize byte_size, VkDeviceSize& output_offset)
{
	CHECK(m_next_free + byte_size <= m_size, "allocate_memory() failed. Not enough space in memory heap!");

	output_offset = m_next_free;
	m_next_free += byte_size;

	return m_memory;
}

void GPUMemory::move_from(GPUMemory&& other)
{
	destroy();

	m_context = other.m_context;
	m_memory = other.m_memory;
	other.m_memory = VK_NULL_HANDLE;
	m_size = other.m_size;
	m_next_free = other.m_next_free;
	m_memory_type = other.m_memory_type;
}

void GPUMemory::destroy()
{
	if (m_memory != VK_NULL_HANDLE)
	{
		vkFreeMemory(m_context->get_device(), m_memory, m_context->get_allocation_callbacks());
		m_memory = VK_NULL_HANDLE;
	}
}
