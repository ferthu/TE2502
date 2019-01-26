#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "vulkan_context.hpp"
#include "gpu_image.hpp"

// Class for an OS window
class Window
{
public:
	Window() {};
	Window(int width, int height, const char* title, VulkanContext& vulkan_context);
	virtual ~Window();

	Window(Window&& other);
	Window& operator=(Window&& other);

	// Return pointer to GLFW window
	GLFWwindow* get_glfw_window();

	// Returns next image from swapchain
	uint32_t get_next_image();

	// Returns swapchain image by index
	VkImage get_swapchain_image(uint32_t index);

	// Returns image view of swapchain image by index
	ImageView& get_swapchain_image_view(uint32_t index);

	// Returns the window size
	glm::uvec2 get_size() const;

	// Returns the swapchain format
	VkFormat get_format() const;

	const VkSwapchainKHR* get_swapchain() const;

	// Returns true if the mouse is locked and should be controlling the camera
	bool get_mouse_locked() const;

	// Set whether the mouse is locked and should be controlling camera
	void set_mouse_locked(bool is_locked);

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

	// Move other into this
	void move_from(Window&& other);

	// Destroys object
	void destroy();

	VkSurfaceKHR m_surface = VK_NULL_HANDLE;
	VkSurfaceCapabilitiesKHR m_surface_capabilities;
	VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
	std::vector<VkImage> m_swapchain_images;
	std::vector<ImageView> m_swapchain_image_views;

	std::vector<VkSurfaceFormatKHR> m_surface_formats;
	std::vector<VkPresentModeKHR> m_surface_present_modes;

	VulkanContext* m_vulkan_context;

	VkFormat m_format;

	VkFence m_swapchain_fence = VK_NULL_HANDLE;

	GLFWwindow* m_window;

	uint32_t m_width;
	uint32_t m_height;

	// True if mouse is locked to window
	bool m_mouse_locked;
};

