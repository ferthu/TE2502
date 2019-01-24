#include <assert.h>

#include "window.hpp"

Window::Window(int width, int height, const char* title, VulkanContext& vulkan_context) : m_vulkan_context(vulkan_context)
{
	m_width = static_cast<uint32_t>(width);
	m_height = static_cast<uint32_t>(height);

	assert(glfwVulkanSupported() == GLFW_TRUE);

	uint32_t count;
	const char** extensions = glfwGetRequiredInstanceExtensions(&count);

	assert(extensions);

	for (uint32_t i = 0; i < count; i++)
	{
		assert(vulkan_context.instance_extension_available(extensions[i]));
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	m_window = glfwCreateWindow(width, height, title, NULL, NULL);

	VkResult result = glfwCreateWindowSurface(vulkan_context.get_instance(), m_window, vulkan_context.get_allocation_callbacks(), &m_surface);
	assert(result == VK_SUCCESS);

	get_surface_capabilities();
	get_surface_formats();
	get_surface_present_modes();

	create_swapchain();
}

Window::~Window()
{
	vkDeviceWaitIdle(m_vulkan_context.get_device());
	vkDestroySwapchainKHR(m_vulkan_context.get_device(), m_swapchain, m_vulkan_context.get_allocation_callbacks());

	vkDestroySurfaceKHR(m_vulkan_context.get_instance(), m_surface, m_vulkan_context.get_allocation_callbacks());

	glfwDestroyWindow(m_window);
}

GLFWwindow* Window::get_glfw_window()
{
	return m_window;
}

uint32_t Window::get_next_image()
{
	uint32_t index = 0;

	VkResult result = vkAcquireNextImageKHR(m_vulkan_context.get_device(), m_swapchain, 999999999, VK_NULL_HANDLE, VK_NULL_HANDLE, &index);
	assert(result == VK_SUCCESS);

	return index;
}

VkImage Window::get_swapchain_image(uint32_t index)
{
	return m_swapchain_images[index];
}

void Window::create_swapchain()
{
	VkSwapchainCreateInfoKHR swapchain_create_info;
	swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchain_create_info.pNext = nullptr;
	swapchain_create_info.flags = 0;
	swapchain_create_info.surface = m_surface;
	assert(m_surface_capabilities.minImageCount >= 2);
	swapchain_create_info.minImageCount = 2;

	VkSurfaceFormatKHR format;
	format.colorSpace = VkColorSpaceKHR::VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	format.format = VkFormat::VK_FORMAT_B8G8R8A8_SRGB;
	if (surface_format_supported(format))
	{
		swapchain_create_info.imageFormat = format.format;
		swapchain_create_info.imageColorSpace = format.colorSpace;
	}
	else
	{
		format.format = VkFormat::VK_FORMAT_B8G8R8A8_UNORM;
		if (surface_format_supported(format))
		{
			swapchain_create_info.imageFormat = format.format;
			swapchain_create_info.imageColorSpace = format.colorSpace;
		}
		else
		{
			assert(false);
		}
	}

	swapchain_create_info.imageExtent.width = m_width;
	swapchain_create_info.imageExtent.height = m_height;
	swapchain_create_info.imageArrayLayers = 1;
	assert(VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT & m_surface_capabilities.supportedUsageFlags);
	assert(VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT & m_surface_capabilities.supportedUsageFlags);
	swapchain_create_info.imageUsage = VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT;
	swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchain_create_info.pQueueFamilyIndices = nullptr;
	swapchain_create_info.queueFamilyIndexCount = 0;
	swapchain_create_info.preTransform = VkSurfaceTransformFlagBitsKHR::VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchain_create_info.compositeAlpha = VkCompositeAlphaFlagBitsKHR::VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchain_create_info.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
	swapchain_create_info.clipped = VK_TRUE;
	swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

	VkResult result = vkCreateSwapchainKHR(m_vulkan_context.get_device(), &swapchain_create_info, m_vulkan_context.get_allocation_callbacks(), &m_swapchain);
	assert(result == VK_SUCCESS);

	uint32_t swapchain_image_count = 0;
	result = vkGetSwapchainImagesKHR(m_vulkan_context.get_device(), m_swapchain, &swapchain_image_count, nullptr);
	assert(result == VK_SUCCESS);

	m_swapchain_images.resize(swapchain_image_count);
	result = vkGetSwapchainImagesKHR(m_vulkan_context.get_device(), m_swapchain, &swapchain_image_count, m_swapchain_images.data());
	assert(result == VK_SUCCESS);
}

void Window::get_surface_capabilities()
{
	VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_vulkan_context.get_physical_device(), m_surface, &m_surface_capabilities);
	assert(result == VK_SUCCESS);
}

void Window::get_surface_formats()
{
	uint32_t format_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_vulkan_context.get_physical_device(), m_surface, &format_count, nullptr);

	assert(format_count > 0);
	m_surface_formats.resize(format_count);

	vkGetPhysicalDeviceSurfaceFormatsKHR(m_vulkan_context.get_physical_device(), m_surface, &format_count, m_surface_formats.data());

	assert(format_count == m_surface_formats.size());
}

void Window::get_surface_present_modes()
{
	uint32_t present_mode_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_vulkan_context.get_physical_device(), m_surface, &present_mode_count, nullptr);

	assert(present_mode_count > 0);
	m_surface_present_modes.resize(present_mode_count);

	vkGetPhysicalDeviceSurfacePresentModesKHR(m_vulkan_context.get_physical_device(), m_surface, &present_mode_count, m_surface_present_modes.data());

	assert(present_mode_count == m_surface_present_modes.size());
}

bool Window::surface_format_supported(VkSurfaceFormatKHR format)
{
	for (auto& fmt : m_surface_formats)
	{
		if (fmt.colorSpace == format.colorSpace && fmt.format == format.format)
			return true;
	}
	return false;
}

bool Window::surface_present_mode_supported(VkPresentModeKHR present_mode)
{
	for (auto& mode : m_surface_present_modes)
	{
		if (mode == present_mode)
			return true;
	}
	return false;
}