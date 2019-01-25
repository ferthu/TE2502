#pragma once

#include <chrono>

#include "window.hpp"
#include "vulkan_context.hpp"
#include "camera.hpp"
#include "compute_queue.hpp"
#include "graphics_queue.hpp"
#include "transfer_queue.hpp"
#include "descriptor_set.hpp"

#include <GLFW/glfw3.h>

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

	// Present queue on screen
	void present(VkQueue queue, const uint32_t index) const;


private:
	Window* m_window;
	Camera* m_main_camera;
	Camera* m_debug_camera;
	Camera* m_current_camera;
	VulkanContext m_vulkan_context;
	std::chrono::time_point<std::chrono::steady_clock> m_timer;

	std::unique_ptr<Pipeline> m_compute_pipeline;
	std::unique_ptr<Pipeline> m_graphics_pipeline;

	PipelineLayout m_pipeline_layout;

	ComputeQueue m_compute_queue;

	DescriptorSet m_image_descriptor_set;
	DescriptorSetLayout m_image_descriptor_set_layout;
};

