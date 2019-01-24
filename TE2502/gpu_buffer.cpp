#include <assert.h>
#include <intrin.h>

#include "vulkan_context.hpp"
#include "gpu_buffer.hpp"
#include "utilities.hpp"

GPUBuffer::GPUBuffer(VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage, GPUMemory& memory_heap) : m_context(context), m_size(size), m_usage(usage)
{
	VkBufferCreateInfo buffer_info;
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.pNext = nullptr;
	buffer_info.flags = 0;
	buffer_info.size = size;
	buffer_info.usage = usage;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buffer_info.queueFamilyIndexCount = 0;
	buffer_info.pQueueFamilyIndices = nullptr;

	VkResult result = vkCreateBuffer(context.get_device(), &buffer_info, context.get_allocation_callbacks(), &m_buffer);
	assert(result == VK_SUCCESS);

	VkMemoryRequirements req = {};
	vkGetBufferMemoryRequirements(context.get_device(), m_buffer, &req);

	// Assert that this object can be backed by memory_heap
	assert(req.memoryTypeBits & (1 << memory_heap.get_memory_type()));

	VkDeviceSize offset = 0;
	VkDeviceMemory memory = memory_heap.allocate_memory(req.size + req.alignment, offset);

	// If offset is not aligned, get the next aligned address
	if (offset % req.alignment != 0)
	{
		offset += req.alignment - (offset % req.alignment);
	}

	// Bind buffer to memory
	result = vkBindBufferMemory(context.get_device(), m_buffer, memory, offset);
	assert(result == VK_SUCCESS);
}

GPUBuffer::~GPUBuffer()
{
	vkDestroyBuffer(m_context.get_device(), m_buffer, m_context.get_allocation_callbacks());
}

BufferView::BufferView(VulkanContext& context, GPUBuffer& buffer, VkFormat format) : m_context(context), m_format(format)
{
#ifdef _DEBUG
	// Check that buffer has specified texel buffer usage
	if (!(buffer.get_usage() & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT || buffer.get_usage() & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT))
	{
#ifdef _DEBUG
		__debugbreak();
#endif

		print("The buffer that a buffer view is created from needs to specify VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT or VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT usage!\n");
		exit(1);
	}

	// Check that the format specified is available for buffer views
	VkFormatProperties fprops;

	vkGetPhysicalDeviceFormatProperties(context.get_physical_device(), format, &fprops);

	if (!(fprops.bufferFeatures & VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT ||
		  fprops.bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT ||
		  fprops.bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_ATOMIC_BIT))
	{
#ifdef _DEBUG
		__debugbreak();
#endif

		print("The format specified for buffer view creation does not support any texel buffer feature!\n");
		exit(1);
	}
#endif

	VkBufferViewCreateInfo buffer_view_info;
	buffer_view_info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
	buffer_view_info.pNext = nullptr;
	buffer_view_info.flags = 0;
	buffer_view_info.buffer = buffer.get_buffer();
	buffer_view_info.format = format;
	buffer_view_info.offset = 0;
	buffer_view_info.range = 0;

	VkResult result = vkCreateBufferView(context.get_device(), &buffer_view_info, context.get_allocation_callbacks(), &m_buffer_view);
	assert(result == VK_SUCCESS);
}

BufferView::~BufferView()
{
	vkDestroyBufferView(m_context.get_device(), m_buffer_view, m_context.get_allocation_callbacks());
}
