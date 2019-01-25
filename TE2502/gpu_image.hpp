#pragma once

#include "gpu_memory.hpp"

class VulkanContext;

// Represents a Vulkan image object
class GPUImage
{
public:
	GPUImage() {};
	GPUImage(VulkanContext& context, 
		VkExtent3D size, 
		VkFormat format, 
		VkImageTiling tiling, 
		VkImageUsageFlags usage, 
		GPUMemory& memory_heap);
	~GPUImage();

	GPUImage(GPUImage&& other);
	GPUImage& operator=(GPUImage&& other);

	VkImage get_image() { return m_image; }
	VkExtent3D get_size() { return m_size; }
	VkImageUsageFlags get_usage() { return m_usage; }
	VkFormat get_format() { return m_format; }

private:
	// Moves other into this
	void move_from(GPUImage&& other);

	VulkanContext* m_context;

	VkImage m_image = VK_NULL_HANDLE;

	VkExtent3D m_size;

	VkImageUsageFlags m_usage;

	VkFormat m_format;
};

// An object that references an image
class ImageView
{
public:
	ImageView() {};
	ImageView(VulkanContext& context, GPUImage& image, VkFormat format, VkImageAspectFlags aspects);
	ImageView(VulkanContext& context, VkImage& image, VkFormat format, VkImageAspectFlags aspects);
	~ImageView();

	ImageView(ImageView&& other);
	ImageView& operator=(ImageView&& other);

	// Returns stored image view
	VkImageView get_view() { return m_image_view; }

	VkFormat get_format() { return m_format; }
	VkImageAspectFlags get_aspects() { return m_aspects; }

private:
	// Move other into this
	void move_from(ImageView&& other);

	VulkanContext* m_context;

	VkImageView m_image_view = VK_NULL_HANDLE;

	VkFormat m_format;

	VkImageAspectFlags m_aspects;
};