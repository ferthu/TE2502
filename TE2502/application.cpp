#include "application.hpp"

#include <string>


Application::Application()
{
	m_main_camera = new Camera(m_window);
	m_debug_camera = new Camera(m_window);
	m_current_camera = m_main_camera;
}


Application::~Application()
{
	delete m_main_camera;
	delete m_debug_camera;
	glfwDestroyWindow(m_window);
}


void Application::run()
{
	while (!glfwWindowShouldClose(m_window))
	{
		auto stop_time = m_timer;
		m_timer = std::chrono::high_resolution_clock::now();
		std::chrono::duration<float> delta_time = stop_time - m_timer;

		glfwPollEvents();
		update(delta_time.count());

		std::string title = "Frame time: " + std::to_string(delta_time.count()); 
		glfwSetWindowTitle(m_window, title.c_str());

		draw();
	}
}


void Application::update(const float dt)
{
	m_current_camera->update(dt);


}


void Application::draw()
{



	glfwSwapBuffers(m_window);
}
