#pragma once
#include <GLFW/glfw3.h>
#include <chrono>

#include "camera.hpp"
#include "window.hpp"
#include "vulkan_context.hpp"

// Main class for the program
class Application
{
public:
	Application();
	virtual ~Application();
	// Start the "game"-loop
	void run();
	// Update
	void update(const float dt);
	// Draw
	void draw();

private:
	Window* m_window;
	Camera* m_main_camera;
	Camera* m_debug_camera;
	Camera* m_current_camera;
	VulkanContext m_vulkan_context;
	std::chrono::time_point<std::chrono::steady_clock> m_timer;
};

