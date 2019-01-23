#include <assert.h>

#include "window.hpp"

Window::Window(int width, int height, const char* title, VulkanContext& vulkan_context) : m_vulkan_context(vulkan_context)
{
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
}

Window::~Window()
{
	vkDestroySurfaceKHR(m_vulkan_context.get_instance(), m_surface, m_vulkan_context.get_allocation_callbacks());

	glfwDestroyWindow(m_window);
}

GLFWwindow* Window::get_glfw_window()
{
	return m_window;
}