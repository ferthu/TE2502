#pragma once

#include <chrono>

#include "window.hpp"
#include "vulkan_context.hpp"
#include "camera.hpp"
#include "compute_queue.hpp"
#include "graphics_queue.hpp"
#include "transfer_queue.hpp"
#include "descriptor_set.hpp"
#include "framebuffer.hpp"
#include "render_pass.hpp"

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
	void present(VkQueue queue, const uint32_t index, VkSemaphore wait_for) const;

	// Set up imgui
	void imgui_setup();

	// Shut down imgui
	void imgui_shutdown();

	// Draw imgui
	void imgui_draw(Framebuffer& framebuffer, VkSemaphore imgui_draw_complete_semaphore);

private:
	struct FrameData
	{
		glm::mat4 view;
		glm::vec4 position;
		glm::vec2 screen_size;
	};

	FrameData m_frame_data;

	VulkanContext m_vulkan_context;

	// Holds Vulkan object required by imgui
	struct ImGuiVulkanState
	{
		~ImGuiVulkanState() {}
		GraphicsQueue queue;
		std::vector<VkSemaphore> done_drawing_semaphores;
		VkRenderPass render_pass;
		VkCommandPool command_pool;
		VkCommandBuffer command_buffer;
		VkFence command_buffer_idle;
	};

	ImGuiVulkanState m_imgui_vulkan_state;

	Window* m_window;
	Camera* m_main_camera;
	Camera* m_debug_camera;
	Camera* m_current_camera;
	std::chrono::time_point<std::chrono::steady_clock> m_timer;

	std::unique_ptr<Pipeline> m_compute_pipeline;
	std::unique_ptr<Pipeline> m_graphics_pipeline;

	PipelineLayout m_pipeline_layout;

	ComputeQueue m_compute_queue;

	DescriptorSet m_image_descriptor_set;
	DescriptorSetLayout m_image_descriptor_set_layout;

	std::vector<Framebuffer> m_swapchain_framebuffers;

	bool m_show_imgui = true;
};

