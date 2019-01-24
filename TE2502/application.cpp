#include "application.hpp"

#include <string>


void error_callback(int error, const char* description)
{
	fprintf(stderr, "Error: %s\n", description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GLFW_TRUE);
}

Application::Application()
{
	glfwSetErrorCallback(error_callback);

	int err = glfwInit();
	assert(err == GLFW_TRUE);


	m_window = new Window(1080, 720, "TE2502", m_vulkan_context);
	m_main_camera = new Camera(m_window->get_glfw_window());
	m_debug_camera = new Camera(m_window->get_glfw_window());
	m_current_camera = m_main_camera;

	m_vulkan_context.create_render_pass(m_window);
	m_compute_pipeline = m_vulkan_context.create_compute_pipeline();
	m_graphics_pipeline = m_vulkan_context.create_graphics_pipeline(m_window->get_size());

	glfwSetKeyCallback(m_window->get_glfw_window(), key_callback);
}


Application::~Application()
{
	delete m_debug_camera;
	delete m_main_camera;
	delete m_window;

	glfwTerminate();
}

void Application::run()
{
	while (!glfwWindowShouldClose(m_window->get_glfw_window()))
	{
		auto stop_time = m_timer;
		m_timer = std::chrono::high_resolution_clock::now();
		std::chrono::duration<float> delta_time = stop_time - m_timer;

		glfwPollEvents();
		update(delta_time.count());

		std::string title = "Frame time: " + std::to_string(delta_time.count()); 
		glfwSetWindowTitle(m_window->get_glfw_window(), title.c_str());

		draw();
	}
}


void Application::update(const float dt)
{
	m_current_camera->update(dt);


}


void Application::draw()
{
	
}
