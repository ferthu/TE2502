#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include "vulkan_context.hpp"

// Class for an OS window
class Window
{
public:
	Window(int width, int height, const char* title, VulkanContext& vulkan_context);
	virtual ~Window();

	// Return pointer to GLFW window
	GLFWwindow* get_glfw_window();

	// Returns next image from swapchain
	uint32_t get_next_image();

	// Returns swapchain image by index
	VkImage get_swapchain_image(uint32_t index);

private:
	// Create the swapchain object
	void create_swapchain();

	// Gets and stores the capabilities of m_surface
	void get_surface_capabilities();

	// Gets and stores surface formats of m_surface
	void get_surface_formats();

	// Gets and stores surface present modes of m_surface
	void get_surface_present_modes();

	// Returns true if format is in m_surface_formats
	bool surface_format_supported(VkSurfaceFormatKHR format);

	// Returns true if present_mode is in m_surface_present_modes
	bool surface_present_mode_supported(VkPresentModeKHR present_mode);

	VkSurfaceKHR m_surface;
	VkSurfaceCapabilitiesKHR m_surface_capabilities;
	VkSwapchainKHR m_swapchain;
	std::vector<VkImage> m_swapchain_images;

	std::vector<VkSurfaceFormatKHR> m_surface_formats;
	std::vector<VkPresentModeKHR> m_surface_present_modes;

	VulkanContext& m_vulkan_context;

	GLFWwindow* m_window;

	uint32_t m_width;
	uint32_t m_height;
};

