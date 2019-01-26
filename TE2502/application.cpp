#include "application.hpp"
#include "graphics_queue.hpp"
#include "compute_queue.hpp"
#include "transfer_queue.hpp"
#include "gpu_memory.hpp"
#include "gpu_image.hpp"
#include "gpu_buffer.hpp"
#include "pipeline_layout.hpp"

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

	m_image_descriptor_set_layout = DescriptorSetLayout(m_vulkan_context);
	m_image_descriptor_set_layout.add_storage_image(VK_SHADER_STAGE_COMPUTE_BIT);
	m_image_descriptor_set_layout.create();

	m_image_descriptor_set = DescriptorSet(m_vulkan_context, m_image_descriptor_set_layout);

	m_pipeline_layout = PipelineLayout(m_vulkan_context);
	m_pipeline_layout.add_descriptor_set_layout(m_image_descriptor_set_layout);
	m_pipeline_layout.create(nullptr);

	m_vulkan_context.create_render_pass(m_window);
	m_compute_pipeline = m_vulkan_context.create_compute_pipeline("test", m_pipeline_layout);
	m_graphics_pipeline = m_vulkan_context.create_graphics_pipeline("test", m_window->get_size(), m_pipeline_layout);

	m_compute_queue = m_vulkan_context.create_compute_queue();


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
	bool right_mouse_clicked = false;

	while (!glfwWindowShouldClose(m_window->get_glfw_window()))
	{
		auto stop_time = m_timer;
		m_timer = std::chrono::high_resolution_clock::now();
		std::chrono::duration<float> delta_time = m_timer - stop_time;

		glfwPollEvents();

		if (!right_mouse_clicked && glfwGetMouseButton(m_window->get_glfw_window(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
		{
			m_window->set_mouse_locked(!m_window->get_mouse_locked());
			right_mouse_clicked = true;
		}
		else if (right_mouse_clicked && glfwGetMouseButton(m_window->get_glfw_window(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_RELEASE)
			right_mouse_clicked = false;

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
	const uint32_t index = m_window->get_next_image();
	VkImage image = m_window->get_swapchain_image(index);

	m_image_descriptor_set.clear();
	m_image_descriptor_set.add_storage_image(m_window->get_swapchain_image_view(index), VK_IMAGE_LAYOUT_GENERAL);
	m_image_descriptor_set.bind();

	m_compute_queue.start_recording();

	// RENDER-------------------
	// Bind pipeline
	m_compute_queue.cmd_bind_compute_pipeline(m_compute_pipeline->m_pipeline);

	// Bind descriptor set
	m_compute_queue.cmd_bind_descriptor_set_compute(m_compute_pipeline->m_pipeline_layout.get_pipeline_layout(), 0, m_image_descriptor_set.get_descriptor_set());

	// Transfer image to shader write layout
	m_compute_queue.cmd_image_barrier(image, 
		VK_ACCESS_MEMORY_READ_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

	// Dispatch
	m_compute_queue.cmd_dispatch(m_window->get_size().x, m_window->get_size().y, 1);

	// end of RENDER------------------

	m_compute_queue.cmd_image_barrier(
		image,
		VK_ACCESS_SHADER_READ_BIT,
		VK_ACCESS_MEMORY_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_IMAGE_ASPECT_COLOR_BIT, 
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

	m_compute_queue.end_recording();
	m_compute_queue.submit();
	m_compute_queue.wait();
	present(m_compute_queue.get_queue(), index);
}

void Application::present(VkQueue queue, const uint32_t index) const
{
	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = nullptr;
	present_info.waitSemaphoreCount = 0;
	present_info.pWaitSemaphores = nullptr;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = m_window->get_swapchain();
	present_info.pImageIndices = &index;
	VkResult result;
	present_info.pResults = &result;

	if (vkQueuePresentKHR(queue, &present_info) != VK_SUCCESS)
	{
#ifdef _DEBUG
		__debugbreak();
#else
		println("Failed to present image");
		exit(1);
#endif
	}

	int a = 0;
}
