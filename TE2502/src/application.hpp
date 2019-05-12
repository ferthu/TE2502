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
#include "algorithm/common.hpp"

#include <GLFW/glfw3.h>

// Main class for the program
class Application
{
public:
	Application(uint32_t window_width, uint32_t window_height, std::vector<std::string> texture_paths);
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

	// Save next rendered frames as png images with specified names
	void snapshot(std::string raster_image_name, std::string raymarch_image_name);

	// Setup terrain textures
	void terrain_texture_setup(std::vector<std::string>& texture_path_names);

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
	DescriptorSetLayout m_raster_set_layout;
	DescriptorSet m_raster_descriptor_set;
	RenderPass m_render_pass;
	PipelineLayout m_draw_pipeline_layout;
	std::unique_ptr<Pipeline> m_draw_pipeline;
	std::unique_ptr<Pipeline> m_draw_wireframe_pipeline;

	// CPU and GPU terrain memory
	GPUMemory m_gpu_buffer_memory;
	GPUBuffer m_gpu_buffer;
	GPUMemory m_cpu_buffer_memory;
	GPUBuffer m_cpu_buffer;

	// CPU image data
	GPUMemory m_cpu_raster_image_memory;
	GPUMemory m_cpu_ray_march_image_memory;
	GPUBuffer m_cpu_raster_image;
	GPUBuffer m_cpu_ray_march_image;
	void* m_raster_data;
	void* m_ray_march_data;
	bool m_save_images = false;
	std::string m_raster_image_name;
	std::string m_ray_march_image_name;

	bool m_update_terrain = true;

	// Textures
	TerrainTexture m_terrain_textures[max_terrain_textures];
	GPUMemory m_terrain_image_memory[max_terrain_textures];
	GPUImage m_terrain_images[max_terrain_textures];
	ImageView m_terrain_image_views[max_terrain_textures];
	uint8_t m_default_texture[4];

	// Width/height of swapchain images
	uint32_t m_window_width;
	uint32_t m_window_height;

	struct DrawData
	{
		glm::mat4 vp;
		glm::vec4 camera_pos;
		glm::vec2 min;			// Min corner
		glm::vec2 max;			// Max corner
		uint32_t node_index;    // Previouly ("buffer_slot")
	};

	DrawData m_draw_data;

	// Debug drawing
	PipelineLayout m_debug_pipeline_layout;
	std::unique_ptr<Pipeline> m_debug_pipeline;
	DebugDrawer m_debug_drawer;
	RenderPass m_debug_render_pass;

	bool m_show_imgui = true;
	bool m_draw_ray_march = false;
	bool m_draw_wireframe = true;
	bool m_draw_triangulated = true;
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

	// Testing 
	struct Fps
	{
		int fps;
		float time;
	};
	struct Sample
	{
		uint gen_nodes = 0;
		uint gen_tris= 0;
		uint draw_nodes = 0;
		uint draw_tris = 0;
		uint new_points = 0;

		float run_time = 0.f;
	};
	bool m_is_testing = false;
	bool m_started_sampling;
	float m_test_run_time = 0.f;
	char m_test_name[40];
	float m_time_to_sample1;
	float m_time_to_sample2;
	Sample m_current_sample;
	std::vector<Fps> m_fps_data;
	std::vector<Sample> m_test_data;
	const float m_sample_rate = 0.25f;
};

