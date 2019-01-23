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

private:
	VkSurfaceKHR m_surface;

	VulkanContext& m_vulkan_context;

	GLFWwindow* m_window;
};

