#include <assert.h>

#include "window.hpp"
#include "utilities.hpp"

Window::Window(int width, int height, const char* title, VulkanContext& vulkan_context) : m_vulkan_context(&vulkan_context)
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

	// Create fence for use when getting swapchain image
	VkFenceCreateInfo fence_info;
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.pNext = nullptr;
	fence_info.flags = 0;

	VK_CHECK(vkCreateFence(m_vulkan_context->get_device(), &fence_info, m_vulkan_context->get_allocation_callbacks(), &m_swapchain_fence), "Failed to create fence!");
}

Window::~Window()
{
	if (m_swapchain_fence != VK_NULL_HANDLE)
		vkDestroyFence(m_vulkan_context->get_device(), m_swapchain_fence, m_vulkan_context->get_allocation_callbacks());

	if (m_vulkan_context)
		vkDeviceWaitIdle(m_vulkan_context->get_device());

	if (m_swapchain != VK_NULL_HANDLE)
		vkDestroySwapchainKHR(m_vulkan_context->get_device(), m_swapchain, m_vulkan_context->get_allocation_callbacks());

	if (m_surface != VK_NULL_HANDLE)
		vkDestroySurfaceKHR(m_vulkan_context->get_instance(), m_surface, m_vulkan_context->get_allocation_callbacks());

	if (m_window)
		glfwDestroyWindow(m_window);
}

Window::Window(Window&& other)
{
	move_from(std::move(other));
}

Window& Window::operator=(Window&& other)
{
	if (this != &other)
		move_from(std::move(other));

	return *this;
}

GLFWwindow* Window::get_glfw_window()
{
	return m_window;
}

uint32_t Window::get_next_image()
{
	uint32_t index = 0;

	VkResult result = vkAcquireNextImageKHR(m_vulkan_context->get_device(), m_swapchain, 999999999, VK_NULL_HANDLE, m_swapchain_fence, &index);
	vkWaitForFences(m_vulkan_context->get_device(), 1, &m_swapchain_fence, VK_FALSE, ~0ull);
	vkResetFences(m_vulkan_context->get_device(), 1, &m_swapchain_fence);

	assert(result == VK_SUCCESS);

	return index;
}

VkImage Window::get_swapchain_image(uint32_t index)
{
	return m_swapchain_images[index];
}

ImageView& Window::get_swapchain_image_view(uint32_t index)
{
	return m_swapchain_image_views[index];
}

glm::vec2 Window::get_size() const
{
	return glm::vec2(m_width, m_height);
}

VkFormat Window::get_format() const
{
	return m_format;
}

const VkSwapchainKHR* Window::get_swapchain() const
{
	return &m_swapchain;
}

void Window::create_swapchain()
{
	// Check that queues support presenting surface
	VkBool32 supported = VK_FALSE;
	VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(m_vulkan_context->get_physical_device(), m_vulkan_context->get_graphics_queue_index(), m_surface, &supported), "Failed to get physical device surface support!");
	assert(supported);

	VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(m_vulkan_context->get_physical_device(), m_vulkan_context->get_compute_queue_index(), m_surface, &supported), "Failed to get physical device surface support!");
	assert(supported);

	VkSwapchainCreateInfoKHR swapchain_create_info;
	swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchain_create_info.pNext = nullptr;
	swapchain_create_info.flags = 0;
	swapchain_create_info.surface = m_surface;
	assert(m_surface_capabilities.minImageCount >= 2);
	swapchain_create_info.minImageCount = 2;

	VkSurfaceFormatKHR surface_format;
	surface_format.colorSpace = VkColorSpaceKHR::VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	//surface_format.format = VkFormat::VK_FORMAT_B8G8R8A8_SRGB;
	//if (surface_format_supported(surface_format))
	//{
	//	swapchain_create_info.imageFormat = surface_format.format;
	//	swapchain_create_info.imageColorSpace = surface_format.colorSpace;
	//}
	//else
	//{
		surface_format.format = VkFormat::VK_FORMAT_B8G8R8A8_UNORM;
		if (surface_format_supported(surface_format))
		{
			swapchain_create_info.imageFormat = surface_format.format;
			swapchain_create_info.imageColorSpace = surface_format.colorSpace;
		}
		else
		{
			CHECK(false, "Requested surface formats not supported");
		}
	//}

	m_format = surface_format.format;

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
	swapchain_create_info.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
	swapchain_create_info.clipped = VK_TRUE;
	swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

	VkResult result = vkCreateSwapchainKHR(m_vulkan_context->get_device(), &swapchain_create_info, m_vulkan_context->get_allocation_callbacks(), &m_swapchain);
	assert(result == VK_SUCCESS);

	uint32_t swapchain_image_count = 0;
	result = vkGetSwapchainImagesKHR(m_vulkan_context->get_device(), m_swapchain, &swapchain_image_count, nullptr);
	assert(result == VK_SUCCESS);

	m_swapchain_images.resize(swapchain_image_count);
	result = vkGetSwapchainImagesKHR(m_vulkan_context->get_device(), m_swapchain, &swapchain_image_count, m_swapchain_images.data());
	assert(result == VK_SUCCESS);

	// Create image views for swapchain images
	m_swapchain_image_views.resize(swapchain_image_count);
	for (size_t i = 0; i < swapchain_image_count; i++)
	{
		m_swapchain_image_views[i] = ImageView(*m_vulkan_context, m_swapchain_images[i], m_format, VK_IMAGE_ASPECT_COLOR_BIT);
	}
}

void Window::get_surface_capabilities()
{
	VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_vulkan_context->get_physical_device(), m_surface, &m_surface_capabilities);
	assert(result == VK_SUCCESS);
}

void Window::get_surface_formats()
{
	uint32_t format_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_vulkan_context->get_physical_device(), m_surface, &format_count, nullptr);

	assert(format_count > 0);
	m_surface_formats.resize(format_count);

	vkGetPhysicalDeviceSurfaceFormatsKHR(m_vulkan_context->get_physical_device(), m_surface, &format_count, m_surface_formats.data());

	assert(format_count == m_surface_formats.size());
}

void Window::get_surface_present_modes()
{
	uint32_t present_mode_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_vulkan_context->get_physical_device(), m_surface, &present_mode_count, nullptr);

	assert(present_mode_count > 0);
	m_surface_present_modes.resize(present_mode_count);

	vkGetPhysicalDeviceSurfacePresentModesKHR(m_vulkan_context->get_physical_device(), m_surface, &present_mode_count, m_surface_present_modes.data());

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

void Window::move_from(Window&& other)
{
	m_surface = other.m_surface;
	other.m_surface = VK_NULL_HANDLE;

	m_surface_capabilities = other.m_surface_capabilities;

	m_swapchain = other.m_swapchain;
	other.m_swapchain = VK_NULL_HANDLE;

	m_swapchain_images = std::move(other.m_swapchain_images);
	m_swapchain_image_views = std::move(other.m_swapchain_image_views);

	m_surface_formats = std::move(other.m_surface_formats);
	m_surface_present_modes = std::move(other.m_surface_present_modes);

	m_vulkan_context = other.m_vulkan_context;
	other.m_vulkan_context = nullptr;

	m_format = other.m_format;

	m_swapchain_fence = other.m_swapchain_fence;
	other.m_swapchain_fence = VK_NULL_HANDLE;

	m_window = other.m_window;
	other.m_window = nullptr;

	m_width = other.m_width;
	m_height = other.m_height;
}
