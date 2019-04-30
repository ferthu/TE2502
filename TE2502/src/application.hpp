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
#include "tfile.hpp"
#include "path_handler.hpp"
#include "algorithm/cpu_triangulate.hpp"

#include <GLFW/glfw3.h>

// Main class for the program
class Application
{
public:
	Application();
	virtual ~Application();
	
	// Start the "game"-loop
	void run(bool auto_triangulate = false);
	
private:
	// Update
	void update(const float dt, bool auto_triangulate = false);
	
	// Draw
	void draw();

	void draw_main();

	void draw_ray_march();

	// Triangulation loop
	void triangulate_thread();

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

	struct DebugDrawingFrameData
	{
		glm::mat4 vp;
	};

	RayMarchFrameData m_ray_march_frame_data;
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

	// Drawing
	RenderPass m_render_pass;
	PipelineLayout m_draw_pipeline_layout;
	std::unique_ptr<Pipeline> m_draw_pipeline;
	std::unique_ptr<Pipeline> m_draw_wireframe_pipeline;

	// CPU and GPU terrain memory
	GPUMemory m_gpu_buffer_memory;
	GPUBuffer m_gpu_buffer;
	GPUMemory m_cpu_buffer_memory;
	GPUBuffer m_cpu_buffer;

	struct DrawData
	{
		glm::mat4 vp;
		glm::vec4 camera_pos;
		glm::vec2 min;			// Min corner
		glm::vec2 max;			// Max corner
		uint32_t node_index;    // Previouly ("buffer_slot")
	};

	DrawData m_draw_data;

	// Error metric parameters
	float m_em_area_multiplier = 0.f;
	float m_em_curvature_multiplier = 0.f;
	float m_em_threshold = 0.1f;

	// Debug drawing
	PipelineLayout m_debug_pipeline_layout;
	std::unique_ptr<Pipeline> m_debug_pipeline;
	DebugDrawer m_debug_drawer;
	RenderPass m_debug_render_pass;

	bool m_show_imgui = true;
	bool m_draw_ray_march = true;
	bool m_draw_wireframe = true;
	bool m_triangulate = false;
	bool m_triangulate_button_held = false;

	PathHandler m_path_handler;

	std::mutex m_mutex;
	std::condition_variable m_cv;
	std::thread m_ray_march_thread;
	bool m_ray_march_done = false;
	bool m_ray_march_new_frame = true;

	bool m_quit = false;

	TFile m_tfile;

	std::mutex m_present_lock;

	// Triangulation threading
	cputri::TriData m_tri_data;
	std::mutex m_tri_mutex;
	std::thread m_tri_thread;
	bool m_tri_done = true;
	DebugDrawer m_tri_debug_drawer;
	std::mutex m_debug_draw_mutex;
};

