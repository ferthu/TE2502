#pragma once

#include <chrono>

#include "graphics/window.hpp"
#include "graphics/vulkan_context.hpp"
#include "camera.hpp"
#include "graphics/compute_queue.hpp"
#include "graphics/graphics_queue.hpp"
#include "graphics/transfer_queue.hpp"
#include "graphics/descriptor_set.hpp"
#include "graphics/framebuffer.hpp"
#include "graphics/render_pass.hpp"
#include "graphics/debug_drawer.hpp"
#include "quadtree.hpp"

#include <GLFW/glfw3.h>

// Main class for the program
class Application
{
public:
	Application();
	virtual ~Application();
	
	// Start the "game"-loop
	void run();
	
private:
	// Update
	void update(const float dt);
	
	// Draw
	void draw();

	void draw_main();

	void draw_ray_march();

	// Present queue on screen
	void present(Window* window, VkQueue queue, const uint32_t index, VkSemaphore wait_for) const;

	// Set up imgui
	void imgui_setup();

	// Shut down imgui
	void imgui_shutdown();

	// Draw imgui
	void imgui_draw(Framebuffer& framebuffer, VkSemaphore imgui_draw_complete_semaphore);

	struct RayMarchFrameData
	{
		glm::mat4 view;
		glm::vec4 position;
		glm::vec2 screen_size;
	};
	struct PointGenerationFrameData
	{
		glm::mat4 vp;
		glm::vec4 position;
	};
	struct DebugDrawingFrameData
	{
		glm::mat4 vp;
	};

	RayMarchFrameData m_ray_march_frame_data;
	PointGenerationFrameData m_point_gen_frame_data;
	DebugDrawingFrameData m_debug_draw_frame_data;

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
		std::vector<Framebuffer> swapchain_framebuffers;
	};

	ImGuiVulkanState m_imgui_vulkan_state;

	Window* m_ray_march_window;
	Window* m_window;
	Camera* m_main_camera;
	Camera* m_debug_camera;
	Camera* m_current_camera;
	std::chrono::time_point<std::chrono::steady_clock> m_timer;
	struct VulkanWindowStates
	{
		GPUMemory depth_memory;
		std::vector<GPUImage> depth_images;
		std::vector<ImageView> depth_image_views;
		std::vector<Framebuffer> swapchain_framebuffers;
	};
	VulkanWindowStates m_ray_march_window_states;
	VulkanWindowStates m_window_states;

	// Ray marching
	DescriptorSetLayout m_ray_march_set_layout;
	DescriptorSet m_ray_march_image_descriptor_set;
	PipelineLayout m_ray_march_pipeline_layout;
	std::unique_ptr<Pipeline> m_ray_march_compute_pipeline;
	ComputeQueue m_ray_march_compute_queue;

	// Point generation
	DescriptorSetLayout m_point_gen_buffer_set_layout_compute;
	DescriptorSetLayout m_point_gen_buffer_set_layout_graphics;
	DescriptorSet m_point_gen_buffer_set_compute;
	DescriptorSet m_point_gen_buffer_set_graphics;
	PipelineLayout m_point_gen_pipeline_layout_compute;
	PipelineLayout m_point_gen_pipeline_layout_graphics;
	std::unique_ptr<Pipeline> m_point_gen_compute_pipeline;
	std::unique_ptr<Pipeline> m_point_gen_graphics_pipeline;
	GraphicsQueue m_point_gen_queue;
	GPUMemory m_point_gen_memory;
	GPUBuffer m_point_gen_input_buffer;
	GPUBuffer m_point_gen_output_buffer;
	RenderPass m_point_gen_render_pass;

	// Terrain generation/drawing
	Quadtree m_quadtree;

	// Debug drawing
	PipelineLayout m_debug_pipeline_layout;
	std::unique_ptr<Pipeline> m_debug_pipeline;
	GraphicsQueue m_debug_queue;
	DebugDrawer m_debug_drawer;
	RenderPass m_debug_render_pass;

	bool m_show_imgui = true;
	bool m_draw_ray_march = true;
};

