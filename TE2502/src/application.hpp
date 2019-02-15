#pragma once

#include <chrono>
#include <mutex>
#include <condition_variable>

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
#include "tfile.hpp"

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

	// Create Vulkan pipelines
	void create_pipelines();

	struct RayMarchFrameData
	{
		glm::mat4 view;
		glm::vec4 position;
		glm::vec2 screen_size;
	};
	struct PointGenerationFrameData
	{
		glm::mat4 ray_march_view;
		glm::vec4 position;
		glm::vec2 screen_size;
		glm::uvec2 sample_counts;
		glm::vec2 sample_offset;
		unsigned int dir_count;
		unsigned int power2_dir_count;
		float em_threshold;
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

	GraphicsQueue m_main_queue;
	
	// Ray marching
	DescriptorSetLayout m_ray_march_set_layout;
	DescriptorSet m_ray_march_image_descriptor_set;
	PipelineLayout m_ray_march_pipeline_layout;
	std::unique_ptr<Pipeline> m_ray_march_compute_pipeline;
	ComputeQueue m_ray_march_compute_queue;

	// Point generation
	DescriptorSetLayout m_point_gen_buffer_set_layout_compute;
	DescriptorSet m_point_gen_buffer_set_compute;
	PipelineLayout m_point_gen_pipeline_layout_compute;
	std::unique_ptr<Pipeline> m_point_gen_compute_pipeline;
	std::unique_ptr<Pipeline> m_point_gen_prefix_sum_pipeline;
	DescriptorSetLayout m_point_gen_buffer_set_layout_graphics;
	PipelineLayout m_point_gen_pipeline_layout_graphics;
	DescriptorSet m_point_gen_buffer_set_graphics;
	std::unique_ptr<Pipeline> m_point_gen_graphics_pipeline;
	RenderPass m_point_gen_render_pass;
	GPUMemory m_point_gen_gpu_memory;
	GPUBuffer m_point_gen_input_buffer;
	GPUBuffer m_point_gen_point_counts_buffer;
	GPUBuffer m_point_gen_output_buffer;
	unsigned int m_point_gen_dirs_sent;
	unsigned int m_point_gen_power2_dirs_sent;

	// Number of samples taken from the error metric image for the x and y directions
	glm::uvec2 m_em_num_samples{ 3, 3 };

	// Sample offset [0, 1] of samples taken from error metric image
	float m_em_offset_x = 0.0f;
	float m_em_offset_y = 0.0f;

	// Error metric parameters
	float m_em_area_multiplier = 0.01f;
	float m_em_curvature_multiplier = 0.01f;
	float m_em_threshold = 0.5f;

	// Group size of error metric dispatch
	uint32_t m_em_group_size = 0;

	// Terrain generation/drawing
	Quadtree m_quadtree;

	// Debug drawing
	PipelineLayout m_debug_pipeline_layout;
	std::unique_ptr<Pipeline> m_debug_pipeline;
	DebugDrawer m_debug_drawer;
	RenderPass m_debug_render_pass;

	bool m_show_imgui = true;
	bool m_draw_ray_march = true;
	bool m_draw_wireframe = false;

	std::mutex m_mutex;
	std::condition_variable m_cv;
	std::thread m_ray_march_thread;
	bool m_ray_march_done = false;
	bool m_ray_march_new_frame = true;

	bool m_quit = false;

	TFile m_tfile;

	std::mutex m_present_lock;
};

