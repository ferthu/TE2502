#include <assert.h>

#include "vulkan_context.hpp"
#include "gpu_image.hpp"
#include "utilities.hpp"

GPUImage::GPUImage(VulkanContext& context, VkExtent3D size, VkFormat format, 
	VkImageTiling tiling, VkImageUsageFlags usage, GPUMemory& memory_heap)
	: m_context(&context), m_size(size), m_usage(usage), m_format(format)
{
	VkImageCreateInfo image_info;
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.pNext = nullptr;
	image_info.flags = 0;	// VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT allows creation of image views with different formats
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.format = format;
	image_info.extent = size;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.tiling = tiling;
	image_info.usage = usage;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.queueFamilyIndexCount = 0;
	image_info.pQueueFamilyIndices = nullptr;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;	// VK_IMAGE_LAYOUT_UNDEFINED and VK_IMAGE_LAYOUT_PREINITIALIZED are allowed

	VkResult result = vkCreateImage(context.get_device(), &image_info, context.get_allocation_callbacks(), &m_image);

	VkMemoryRequirements req = {};
	vkGetImageMemoryRequirements(context.get_device(), m_image, &req);

	// Assert that this object can be backed by memory_heap
	assert(req.memoryTypeBits & (1 << memory_heap.get_memory_type()));

	m_memory = memory_heap.allocate_memory(req.size + req.alignment, m_offset);

	// Get the next aligned address
	m_offset += req.alignment - (m_offset % req.alignment);

	// Bind buffer to memory
	result = vkBindImageMemory(context.get_device(), m_image, m_memory, m_offset);
	assert(result == VK_SUCCESS);
}

GPUImage::~GPUImage()
{
	destroy();
}

GPUImage::GPUImage(GPUImage&& other)
{
	move_from(std::move(other));
}

GPUImage& GPUImage::operator=(GPUImage&& other)
{
	if (this != &other)
	{
		move_from(std::move(other));
	}

	return *this;
}

void GPUImage::move_from(GPUImage&& other)
{
	destroy();

	m_context = other.m_context;
	m_image = other.m_image;
	other.m_image = VK_NULL_HANDLE;
	m_size = other.m_size;
	m_usage = other.m_usage;
	m_format = other.m_format;
	m_memory = other.m_memory;
	other.m_memory = VK_NULL_HANDLE;
	m_offset = other.m_offset;
}

void GPUImage::destroy()
{
	if (m_image != VK_NULL_HANDLE)
	{
		vkDestroyImage(m_context->get_device(), m_image, m_context->get_allocation_callbacks());
		m_image = VK_NULL_HANDLE;
	}
}

ImageView::ImageView(VulkanContext& context, GPUImage& image, VkFormat format, VkImageAspectFlags aspects) : m_context(&context), m_format(format), m_aspects(aspects)
{
	// Check that the image is the same format as the requested image view
	assert(image.get_format() == format);

	VkComponentMapping component_mapping;
	component_mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	component_mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	component_mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	component_mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	VkImageSubresourceRange subrange;
	subrange.aspectMask = aspects;
	subrange.baseMipLevel = 0;
	subrange.levelCount = 1;
	subrange.baseArrayLayer = 0;
	subrange.layerCount = 1;

	VkImageViewCreateInfo image_view_info;
	image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	image_view_info.pNext = nullptr;
	image_view_info.flags = 0;
	image_view_info.image = image.get_image();
	image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	image_view_info.format = format;
	image_view_info.components = component_mapping;
	image_view_info.subresourceRange = subrange;

	VkResult result = vkCreateImageView(m_context->get_device(), &image_view_info, m_context->get_allocation_callbacks(), &m_image_view);
	assert(result == VK_SUCCESS);
}

ImageView::ImageView(VulkanContext& context, VkImage& image, VkFormat format, VkImageAspectFlags aspects) : m_context(&context), m_format(format), m_aspects(aspects)
{
	VkComponentMapping component_mapping;
	component_mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	component_mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	component_mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	component_mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	VkImageSubresourceRange subrange;
	subrange.aspectMask = aspects;
	subrange.baseMipLevel = 0;
	subrange.levelCount = 1;
	subrange.baseArrayLayer = 0;
	subrange.layerCount = 1;

	VkImageViewCreateInfo image_view_info;
	image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	image_view_info.pNext = nullptr;
	image_view_info.flags = 0;
	image_view_info.image = image;
	image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	image_view_info.format = format;
	image_view_info.components = component_mapping;
	image_view_info.subresourceRange = subrange;

	VkResult result = vkCreateImageView(m_context->get_device(), &image_view_info, m_context->get_allocation_callbacks(), &m_image_view);
	assert(result == VK_SUCCESS);
}

ImageView::~ImageView()
{
	destroy();
}

ImageView::ImageView(ImageView&& other)
{
	move_from(std::move(other));
}

ImageView& ImageView::operator=(ImageView&& other)
{
	if (this != &other)
	{
		move_from(std::move(other));
	}

	return *this;
}

void ImageView::move_from(ImageView&& other)
{
	destroy();

	m_context = other.m_context;
	m_image_view = other.m_image_view;
	other.m_image_view = VK_NULL_HANDLE;
	m_format = other.m_format;
	m_aspects = other.m_aspects;
}

void ImageView::destroy()
{
	if (m_image_view != VK_NULL_HANDLE)
	{
		vkDestroyImageView(m_context->get_device(), m_image_view, m_context->get_allocation_callbacks());
		m_image_view = VK_NULL_HANDLE;
	}
}
