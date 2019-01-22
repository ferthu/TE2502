#pragma once
#include <GLFW/glfw3.h>
#include <chrono>

#include "camera.hpp"

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
	GLFWwindow* m_window;
	Camera* m_main_camera;
	Camera* m_debug_camera;
	Camera* m_current_camera;
	std::chrono::time_point<std::chrono::steady_clock> m_timer;
};

