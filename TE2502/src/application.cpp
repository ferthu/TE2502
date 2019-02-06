#include "application.hpp"
#include "graphics/graphics_queue.hpp"
#include "graphics/compute_queue.hpp"
#include "graphics/transfer_queue.hpp"
#include "graphics/gpu_memory.hpp"
#include "graphics/gpu_image.hpp"
#include "graphics/gpu_buffer.hpp"
#include "graphics/pipeline_layout.hpp"
#include "quadtree.hpp"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

#include <string>
#include <glm/gtc/matrix_transform.hpp>

//#define RAY_MARCH_WINDOW

void error_callback(int error, const char* description)
{
	fprintf(stderr, "Error: %s\n", description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GLFW_TRUE);
}

Application::Application() : m_tfile("shaders/vars.txt", "shaders/")
{
	m_tfile.compile_shaders();

	glfwSetErrorCallback(error_callback);

	int err = glfwInit();
	assert(err == GLFW_TRUE);

#ifdef RAY_MARCH_WINDOW
	m_ray_march_window = new Window(1080, 720, "TE2502 - Ray March", m_vulkan_context, false);
#endif
	m_window = new Window(1080, 720, "TE2502 - Main", m_vulkan_context, true);
	m_main_camera = new Camera(m_window->get_glfw_window());
	m_debug_camera = new Camera(m_window->get_glfw_window());
	m_current_camera = m_main_camera;

	glfwSetWindowPos(m_window->get_glfw_window(), 840, 100);

#ifdef RAY_MARCH_WINDOW
	glfwSetWindowPos(m_ray_march_window->get_glfw_window(), 0, 100);


	// Ray marching
	m_ray_march_set_layout = DescriptorSetLayout(m_vulkan_context);
	m_ray_march_set_layout.add_storage_image(VK_SHADER_STAGE_COMPUTE_BIT);
	m_ray_march_set_layout.create();

	m_ray_march_image_descriptor_set = DescriptorSet(m_vulkan_context, m_ray_march_set_layout);

	m_ray_march_pipeline_layout = PipelineLayout(m_vulkan_context);
	m_ray_march_pipeline_layout.add_descriptor_set_layout(m_ray_march_set_layout);
	// Set up push constant range for frame data
	{
		VkPushConstantRange push_range;
		push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		push_range.offset = 0;
		push_range.size = sizeof(RayMarchFrameData);

		m_ray_march_pipeline_layout.create(&push_range);
	}

	m_ray_march_compute_queue = m_vulkan_context.create_compute_queue();
	// !Ray marching
#endif

	// Point generation
	// Compute
	m_point_gen_buffer_set_layout_compute = DescriptorSetLayout(m_vulkan_context);
	m_point_gen_buffer_set_layout_compute.add_storage_buffer(VK_SHADER_STAGE_COMPUTE_BIT);
	m_point_gen_buffer_set_layout_compute.add_storage_buffer(VK_SHADER_STAGE_COMPUTE_BIT);
	m_point_gen_buffer_set_layout_compute.add_storage_buffer(VK_SHADER_STAGE_COMPUTE_BIT);
	m_point_gen_buffer_set_layout_compute.create();

	m_point_gen_buffer_set_compute = DescriptorSet(m_vulkan_context, m_point_gen_buffer_set_layout_compute);

	m_point_gen_pipeline_layout_compute = PipelineLayout(m_vulkan_context);
	m_point_gen_pipeline_layout_compute.add_descriptor_set_layout(m_point_gen_buffer_set_layout_compute);
	{
		// Set up push constant range for frame data
		VkPushConstantRange push_range;
		push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT;
		push_range.offset = 0;
		push_range.size = sizeof(PointGenerationFrameData);

		m_point_gen_pipeline_layout_compute.create(&push_range);
	}

	// Graphics
	m_point_gen_buffer_set_layout_graphics = DescriptorSetLayout(m_vulkan_context);
	m_point_gen_buffer_set_layout_graphics.add_uniform_buffer(VK_SHADER_STAGE_VERTEX_BIT);
	m_point_gen_buffer_set_layout_graphics.create();

	m_point_gen_buffer_set_graphics = DescriptorSet(m_vulkan_context, m_point_gen_buffer_set_layout_graphics);

	m_point_gen_pipeline_layout_graphics = PipelineLayout(m_vulkan_context);
	m_point_gen_pipeline_layout_graphics.add_descriptor_set_layout(m_point_gen_buffer_set_layout_graphics);
	{
		// Set up push constant range for frame data
		VkPushConstantRange push_range;
		push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT;
		push_range.offset = 0;
		push_range.size = sizeof(PointGenerationFrameData);

		m_point_gen_pipeline_layout_graphics.create(&push_range);
	}



	m_point_gen_render_pass = RenderPass(m_vulkan_context, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true, 
		true, true, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);



	const unsigned int max_rays_per_frame = 10000;
	const unsigned int max_points_per_ray = 5;
	VkDeviceSize dirs = sizeof(glm::vec4) * max_rays_per_frame;
	VkDeviceSize point_counts = sizeof(uint32_t) * (max_rays_per_frame + 1);
	VkDeviceSize points_found = sizeof(glm::vec4) * max_rays_per_frame * max_points_per_ray;
	m_point_gen_cpu_memory = m_vulkan_context.allocate_host_memory(dirs + 1000);
	m_point_gen_gpu_memory = m_vulkan_context.allocate_device_memory(dirs + point_counts + points_found + 2000);

	m_point_gen_cpu_buffer = GPUBuffer(m_vulkan_context, dirs, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, m_point_gen_cpu_memory);
	m_point_gen_input_buffer = GPUBuffer(m_vulkan_context, dirs, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, m_point_gen_gpu_memory);
	m_point_gen_point_counts_buffer = GPUBuffer(m_vulkan_context, point_counts, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_point_gen_gpu_memory);
	m_point_gen_output_buffer = GPUBuffer(m_vulkan_context, points_found + sizeof(VkDrawIndirectCommand), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, m_point_gen_gpu_memory);

	vkMapMemory(m_vulkan_context.get_device(), m_point_gen_cpu_buffer.get_memory(), m_point_gen_cpu_buffer.get_offset(), m_point_gen_cpu_buffer.get_size(), 0, (void**)&m_point_gen_dirs);

	m_point_gen_queue = m_vulkan_context.create_graphics_queue();

	// Dirs
	const int t = 30;
	for (int y = 0; y < t; ++y)
	{
		for (int x = 0; x < t; ++x)
		{
			m_point_gen_dirs[m_point_gen_dirs_sent++] = glm::normalize(glm::vec4(x - t / 2.f, y - t / 2.f, t / 2.f, 0));
		}
	}
	//m_point_gen_dirs_sent = 98;
	int p = 0;
	for (; powf(2, p) < m_point_gen_dirs_sent; ++p)
	{ }
	m_point_gen_power2_dirs_sent = (unsigned int)(powf(2, p));
	// !Point generation

	//glfwSetKeyCallback(m_ray_march_window->get_glfw_window(), key_callback);
	glfwSetKeyCallback(m_window->get_glfw_window(), key_callback);

	imgui_setup();

#ifdef RAY_MARCH_WINDOW
	m_ray_march_window_states.swapchain_framebuffers.resize(m_ray_march_window->get_swapchain_size());
	for (uint32_t i = 0; i < m_ray_march_window->get_swapchain_size(); i++)
	{
		m_ray_march_window_states.swapchain_framebuffers[i] = Framebuffer(m_vulkan_context);
		m_ray_march_window_states.swapchain_framebuffers[i].add_attachment(m_ray_march_window->get_swapchain_image_view(i));
		m_ray_march_window_states.swapchain_framebuffers[i].create(m_imgui_vulkan_state.render_pass, m_ray_march_window->get_size().x, m_ray_march_window->get_size().y);
	}
#endif

	m_window_states.depth_memory = m_vulkan_context.allocate_device_memory(m_window->get_size().x * m_window->get_size().y * 2 * m_window->get_swapchain_size() * 4 + 1024);
	m_window_states.swapchain_framebuffers.resize(m_window->get_swapchain_size());
	m_imgui_vulkan_state.swapchain_framebuffers.resize(m_window->get_swapchain_size());
	for (uint32_t i = 0; i < m_window->get_swapchain_size(); i++)
	{
		VkExtent3D depth_size;
		depth_size.width = m_window->get_size().x;
		depth_size.height = m_window->get_size().y;
		depth_size.depth = 1;
		m_window_states.depth_images.push_back(GPUImage(m_vulkan_context, depth_size, 
			VK_FORMAT_D32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, 
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
			m_window_states.depth_memory));

		m_window_states.depth_image_views.push_back(ImageView(m_vulkan_context, m_window_states.depth_images[i], VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT));

		m_window_states.swapchain_framebuffers[i] = Framebuffer(m_vulkan_context);
		m_window_states.swapchain_framebuffers[i].add_attachment(m_window->get_swapchain_image_view(i));
		m_window_states.swapchain_framebuffers[i].add_attachment(m_window_states.depth_image_views[i]);
		m_window_states.swapchain_framebuffers[i].create(m_point_gen_render_pass.get_render_pass(), m_window->get_size().x, m_window->get_size().y);

		m_imgui_vulkan_state.swapchain_framebuffers[i] = Framebuffer(m_vulkan_context);
		m_imgui_vulkan_state.swapchain_framebuffers[i].add_attachment(m_window->get_swapchain_image_view(i));
		m_imgui_vulkan_state.swapchain_framebuffers[i].create(m_imgui_vulkan_state.render_pass, m_window->get_size().x, m_window->get_size().y);
	}

	m_imgui_vulkan_state.done_drawing_semaphores.resize(m_window->get_swapchain_size());
	for (uint32_t i = 0; i < m_window->get_swapchain_size(); i++)
	{
		VkSemaphoreCreateInfo create_info;
		create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		create_info.pNext = nullptr;
		create_info.flags = 0;
		VK_CHECK(vkCreateSemaphore(m_vulkan_context.get_device(), &create_info, m_vulkan_context.get_allocation_callbacks(), 
			&m_imgui_vulkan_state.done_drawing_semaphores[i]), "Semaphore creation failed!")
	}

	// Set up terrain generation/drawing
	uint32_t num_indices = m_tfile.get_u32("TERRAIN_GENERATE_NUM_INDICES");
	uint32_t num_vertices = m_tfile.get_u32("TERRAIN_GENERATE_NUM_VERTICES");
	uint32_t num_nodes = m_tfile.get_u32("TERRAIN_GENERATE_NUM_NODES");
	m_quadtree = Quadtree(m_vulkan_context, 5000.0f, 3, num_nodes, num_indices, num_vertices, *m_window);

	// Set up debug drawing
	m_debug_pipeline_layout = PipelineLayout(m_vulkan_context);
	{
		// Set up push constant range for frame data
		VkPushConstantRange push_range;
		push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		push_range.offset = 0;
		push_range.size = sizeof(DebugDrawingFrameData);

		m_debug_pipeline_layout.create(&push_range);
	}

	m_debug_render_pass = RenderPass(m_vulkan_context, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, false, true, false, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	m_debug_queue = m_vulkan_context.create_graphics_queue();
	m_debug_drawer = DebugDrawer(m_vulkan_context, 11000);

	create_pipelines();

#ifdef RAY_MARCH_WINDOW
	// Start ray march thread
	m_ray_march_thread = std::thread(&Application::draw_ray_march, this);
#endif
}

Application::~Application()
{
	imgui_shutdown();

	for (uint32_t i = 0; i < m_window->get_swapchain_size(); i++)
	{
		vkDestroySemaphore(m_vulkan_context.get_device(), m_imgui_vulkan_state.done_drawing_semaphores[i], 
			m_vulkan_context.get_allocation_callbacks());
	}

	vkUnmapMemory(m_vulkan_context.get_device(), m_point_gen_cpu_buffer.get_memory());
	m_point_gen_dirs = nullptr;

	delete m_debug_camera;
	delete m_main_camera;

#ifdef RAY_MARCH_WINDOW
	delete m_ray_march_window;
#endif

	delete m_window;

	glfwTerminate();
}

void Application::run()
{
	bool right_mouse_clicked = false;
	bool f_pressed = false;
	bool demo_window = true;
	bool camera_switch_pressed = false;
	bool f5_pressed = false;

	while (!glfwWindowShouldClose(m_window->get_glfw_window()))
	{
		auto stop_time = m_timer;
		m_timer = std::chrono::high_resolution_clock::now();
		std::chrono::duration<float> delta_time = m_timer - stop_time;

		glfwPollEvents();

		// Toggle camera controls
		if (!right_mouse_clicked && glfwGetMouseButton(m_window->get_glfw_window(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS
			&& !ImGui::GetIO().WantCaptureMouse)
		{
			m_window->set_mouse_locked(!m_window->get_mouse_locked());
			right_mouse_clicked = true;
		}
		else if (m_window && glfwGetMouseButton(m_window->get_glfw_window(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_RELEASE
			&& !ImGui::GetIO().WantCaptureMouse)
			right_mouse_clicked = false;

		// Switch camera
		if (!camera_switch_pressed && glfwGetKey(m_window->get_glfw_window(), GLFW_KEY_F1) == GLFW_PRESS
			&& !ImGui::GetIO().WantCaptureMouse)
		{
			if (m_current_camera == m_main_camera)
				m_current_camera = m_debug_camera;
			else
				m_current_camera = m_main_camera;
			camera_switch_pressed = true;
		}
		else if (m_window && glfwGetKey(m_window->get_glfw_window(), GLFW_KEY_F1) == GLFW_RELEASE
			&& !ImGui::GetIO().WantCaptureMouse)
			camera_switch_pressed = false;

		// Toggle imgui
		if (!f_pressed && glfwGetKey(m_window->get_glfw_window(), GLFW_KEY_F) == GLFW_PRESS)
		{
			f_pressed = true;
			m_show_imgui = !m_show_imgui;
		}
		else if (f_pressed && glfwGetKey(m_window->get_glfw_window(), GLFW_KEY_F) == GLFW_RELEASE)
			f_pressed = false;

		// Reload shaders
		if (!f5_pressed && glfwGetKey(m_window->get_glfw_window(), GLFW_KEY_F5) == GLFW_PRESS)
		{
			f5_pressed = true;
			m_tfile.compile_shaders();
			create_pipelines();
		}
		else if (f5_pressed && glfwGetKey(m_window->get_glfw_window(), GLFW_KEY_F5) == GLFW_RELEASE)
			f5_pressed = false;

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame(!m_window->get_mouse_locked() && m_show_imgui);
		ImGui_ImplGlfw_SetHandleCallbacks(!m_window->get_mouse_locked() && m_show_imgui);
		ImGui::NewFrame();

		// Reset debug drawer
		m_debug_drawer.new_frame();

		update(delta_time.count());

		draw();
	}

	m_quit = true; 
#ifdef RAY_MARCH_WINDOW
	m_cv.notify_all();
	m_ray_march_thread.join();
#endif
}


void Application::update(const float dt)
{
	m_current_camera->update(dt, m_window->get_mouse_locked(), m_debug_drawer);

	m_point_gen_frame_data.vp = m_current_camera->get_vp();
	m_point_gen_frame_data.ray_march_view = m_main_camera->get_ray_march_view();
	m_point_gen_frame_data.position = glm::vec4(m_main_camera->get_pos(), 0);
	m_point_gen_frame_data.dir_count = m_point_gen_dirs_sent; 
	m_point_gen_frame_data.power2_dir_count = m_point_gen_power2_dirs_sent;

#ifdef RAY_MARCH_WINDOW
	m_ray_march_frame_data.view = m_current_camera->get_ray_march_view();
	m_ray_march_frame_data.screen_size = m_ray_march_window->get_size();
	m_ray_march_frame_data.position = glm::vec4(m_current_camera->get_pos(), 0);
#endif

	m_debug_draw_frame_data.vp = m_current_camera->get_vp();

	if (m_show_imgui)
	{
		ImGui::Begin("Info");

		static const uint32_t num_frames = 200;
		static float values[num_frames] = { 0 };
		static int values_offset = 0;
		values[values_offset] = dt;
		values_offset = (values_offset + 1) % num_frames;
		ImGui::PlotLines("Frame Time", values, num_frames, values_offset, nullptr, 0.0f, 0.02f, ImVec2(150, 30));
		

		std::string text = "Frame info: " + std::to_string(int(1.f / dt)) + "fps  "
			+ std::to_string(dt) + "s  " + std::to_string(int(100.f * dt / 0.016f)) + "%%";
		ImGui::Text(text.c_str());
		text = "Position: " + std::to_string(m_main_camera->get_pos().x) + ", " + std::to_string(m_main_camera->get_pos().y) + ", " + std::to_string(m_main_camera->get_pos().z);
		ImGui::Text(text.c_str());
		ImGui::Checkbox("Draw Ray Marched View", &m_draw_ray_march);
		if (ImGui::Button("Clear Terrain"))
		{
			m_quadtree.clear_terrain();
		}
		ImGui::End();
	}
}


void Application::draw()
{
#ifdef RAY_MARCH_WINDOW
	// Start ray march thread
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_ray_march_new_frame = true;
	}
	m_cv.notify_all();
#endif

	draw_main();

#ifdef RAY_MARCH_WINDOW
	// Wait for ray march thread
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_cv.wait(lock, [this] { return m_ray_march_done; });
		m_ray_march_done = false;
	}
#endif
}

void Application::draw_main()
{
	const uint32_t index = m_window->get_next_image();
	VkImage image = m_window->get_swapchain_image(index);

	// RENDER-------------------

	// Fritjof stuff
	{
		m_point_gen_buffer_set_compute.clear();
		m_point_gen_buffer_set_compute.add_storage_buffer(m_point_gen_input_buffer);
		m_point_gen_buffer_set_compute.add_storage_buffer(m_point_gen_point_counts_buffer);
		m_point_gen_buffer_set_compute.add_storage_buffer(m_point_gen_output_buffer);
		m_point_gen_buffer_set_compute.bind();

		m_point_gen_queue.start_recording();

		// Bind pipeline
		m_point_gen_queue.cmd_bind_compute_pipeline(m_point_gen_compute_pipeline->m_pipeline);

		// Bind descriptor set
		m_point_gen_queue.cmd_bind_descriptor_set_compute(m_point_gen_compute_pipeline->m_pipeline_layout.get_pipeline_layout(), 0, m_point_gen_buffer_set_compute.get_descriptor_set());

		// Push frame data
		m_point_gen_queue.cmd_push_constants(m_point_gen_pipeline_layout_compute.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT, sizeof(PointGenerationFrameData), &m_point_gen_frame_data);

		m_point_gen_queue.cmd_copy_buffer(m_point_gen_cpu_buffer.get_buffer(), m_point_gen_input_buffer.get_buffer(), m_point_gen_dirs_sent * sizeof(glm::vec4));

		// Memory barrier for GPU buffer
		m_point_gen_queue.cmd_buffer_barrier(m_point_gen_input_buffer.get_buffer(),
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		// Dispatch point generation/gathering
		const uint32_t group_size = 32;
		m_point_gen_queue.cmd_dispatch(m_point_gen_power2_dirs_sent / group_size + 1, 1, 1);

		m_point_gen_queue.cmd_buffer_barrier(m_point_gen_point_counts_buffer.get_buffer(),
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		m_point_gen_queue.cmd_buffer_barrier(m_point_gen_output_buffer.get_buffer(),
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		m_point_gen_queue.cmd_bind_compute_pipeline(m_point_gen_prefix_sum_pipeline->m_pipeline);

		// Dispatch prefix sum
		m_point_gen_queue.cmd_dispatch(1, 1, 1);

		m_point_gen_queue.cmd_image_barrier(
			image,
			VK_ACCESS_MEMORY_READ_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		m_point_gen_queue.cmd_image_barrier(
			m_window_states.depth_images[index].get_image(),
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			VK_IMAGE_ASPECT_DEPTH_BIT,
			VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

		m_point_gen_queue.cmd_buffer_barrier(m_point_gen_output_buffer.get_buffer(),
			VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT |	VK_ACCESS_INDEX_READ_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT |	VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);

		m_point_gen_queue.cmd_bind_graphics_pipeline(m_point_gen_graphics_pipeline->m_pipeline);
		m_point_gen_queue.cmd_bind_vertex_buffer(m_point_gen_output_buffer.get_buffer(), sizeof(VkDrawIndirectCommand));
		m_point_gen_queue.cmd_begin_render_pass(m_point_gen_render_pass, m_window_states.swapchain_framebuffers[index]);
		m_point_gen_queue.cmd_draw_indirect(m_point_gen_output_buffer.get_buffer());
		m_point_gen_queue.cmd_end_render_pass();

		m_point_gen_queue.end_recording();
		m_point_gen_queue.submit();
		m_point_gen_queue.wait();

		// end of RENDER------------------
	}

	// Perform terrain generation/drawing
	Frustum fr = m_main_camera->get_frustum();
	m_quadtree.draw_terrain(fr, m_debug_drawer, m_window_states.swapchain_framebuffers[index], *m_current_camera);

	// Do debug drawing
	{
		m_debug_drawer.draw_line({ 0,0,0 }, { 1, 0, 0 }, { 1, 0, 0 });
		m_debug_drawer.draw_line({ 0,0,0 }, { 0, 1, 0 }, { 0, 1, 0 });
		m_debug_drawer.draw_line({ 0,0,0 }, { 0, 0, 1 }, { 0, 0, 1 });

		if (m_current_camera != m_main_camera)
		{
			// Draw frustum
			m_debug_drawer.draw_frustum(m_main_camera->get_vp(), {1, 0, 1});

			//glm::mat4 inv_vp = glm::inverse(m_main_camera->get_vp());
			//glm::vec4 left_pos = inv_vp * glm::vec4(-1, 0, 0.99f, 1); left_pos /= left_pos.w;
			//glm::vec4 right_pos = inv_vp * glm::vec4(1, 0, 0.99f, 1); right_pos /= right_pos.w;
			//glm::vec4 top_pos = inv_vp * glm::vec4(0, -1, 0.99f, 1); top_pos /= top_pos.w;
			//glm::vec4 bottom_pos = inv_vp * glm::vec4(0, 1, 0.99f, 1); bottom_pos /= bottom_pos.w;
			//glm::vec4 near_pos = inv_vp * glm::vec4(0, 0, 0, 1); near_pos /= near_pos.w;
			//glm::vec4 far_pos = inv_vp * glm::vec4(0, 0, 1, 1); far_pos /= far_pos.w;

			//Frustum frustum = m_main_camera->get_frustum();
			//m_debug_drawer.draw_plane(frustum.m_left, left_pos, 1.0f, { 1,1,1 }, { 1,0,0 });
			//m_debug_drawer.draw_plane(frustum.m_right, right_pos, 1.0f, { 1,1,1 }, { 1,0,0 });
			//m_debug_drawer.draw_plane(frustum.m_top, top_pos, 1.0f, { 1,1,1 }, { 1,0,0 });
			//m_debug_drawer.draw_plane(frustum.m_bottom, bottom_pos, 1.0f, { 1,1,1 }, { 1,0,0 });
			//m_debug_drawer.draw_plane(frustum.m_near, near_pos, 1.0f, { 1,1,1 }, { 1,0,0 });
			//m_debug_drawer.draw_plane(frustum.m_far, far_pos, 1.0f, { 1,1,1 }, { 1,0,0 });
		}

		m_debug_queue.start_recording();

		// Copy lines specified on CPU to GPU buffer
		m_debug_queue.cmd_copy_buffer(m_debug_drawer.get_cpu_buffer().get_buffer(),
			m_debug_drawer.get_gpu_buffer().get_buffer(),
			m_debug_drawer.get_active_buffer_size());
	
		// Memory barrier for GPU buffer
		m_debug_queue.cmd_buffer_barrier(m_debug_drawer.get_gpu_buffer().get_buffer(),
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);

		m_debug_queue.cmd_begin_render_pass(m_debug_render_pass, m_window_states.swapchain_framebuffers[index]);

		m_debug_queue.cmd_bind_graphics_pipeline(m_debug_pipeline->m_pipeline);
		m_debug_queue.cmd_bind_vertex_buffer(m_debug_drawer.get_gpu_buffer().get_buffer(), 0);
		m_debug_queue.cmd_push_constants(m_debug_pipeline_layout.get_pipeline_layout(), VK_SHADER_STAGE_VERTEX_BIT, sizeof(DebugDrawingFrameData), &m_debug_draw_frame_data);
		m_debug_queue.cmd_draw(m_debug_drawer.get_num_lines() * 2);

		m_debug_queue.cmd_end_render_pass();

		m_debug_queue.end_recording();
		m_debug_queue.submit();
		m_debug_queue.wait();
	}

	imgui_draw(m_imgui_vulkan_state.swapchain_framebuffers[index], m_imgui_vulkan_state.done_drawing_semaphores[index]);

	present(m_window, m_point_gen_queue.get_queue(), index, m_imgui_vulkan_state.done_drawing_semaphores[index]);
}

void Application::draw_ray_march()
{
	while (!m_quit)
	{
		// Wait until main thread signals new frame
		std::unique_lock<std::mutex> lock(m_mutex);
		m_cv.wait(lock, [this] { return m_ray_march_new_frame || m_quit; });
		m_ray_march_new_frame = false;

		if (m_draw_ray_march)
		{
			const uint32_t index = m_ray_march_window->get_next_image();
			VkImage image = m_ray_march_window->get_swapchain_image(index);

			m_ray_march_image_descriptor_set.clear();
			m_ray_march_image_descriptor_set.add_storage_image(m_ray_march_window->get_swapchain_image_view(index), VK_IMAGE_LAYOUT_GENERAL);
			m_ray_march_image_descriptor_set.bind();

			m_ray_march_compute_queue.start_recording();

			// RENDER-------------------
			// Bind pipeline
			m_ray_march_compute_queue.cmd_bind_compute_pipeline(m_ray_march_compute_pipeline->m_pipeline);

			// Bind descriptor set
			m_ray_march_compute_queue.cmd_bind_descriptor_set_compute(m_ray_march_compute_pipeline->m_pipeline_layout.get_pipeline_layout(), 0, m_ray_march_image_descriptor_set.get_descriptor_set());

			// Transfer image to shader write layout
			m_ray_march_compute_queue.cmd_image_barrier(image,
				VK_ACCESS_MEMORY_READ_BIT,
				VK_ACCESS_SHADER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_GENERAL,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

			// Push frame data
			m_ray_march_compute_queue.cmd_push_constants(m_ray_march_pipeline_layout.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, sizeof(RayMarchFrameData), &m_ray_march_frame_data);

			// Dispatch
			const uint32_t group_size = 32;
			m_ray_march_compute_queue.cmd_dispatch(m_ray_march_window->get_size().x / group_size + 1, m_ray_march_window->get_size().y / group_size + 1, 1);

			// end of RENDER------------------

			m_ray_march_compute_queue.cmd_image_barrier(
				image,
				VK_ACCESS_SHADER_WRITE_BIT,
				VK_ACCESS_MEMORY_READ_BIT,
				VK_IMAGE_LAYOUT_GENERAL,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

			m_ray_march_compute_queue.end_recording();
			m_ray_march_compute_queue.submit();
			m_ray_march_compute_queue.wait();

			present(m_ray_march_window, m_ray_march_compute_queue.get_queue(), index, VK_NULL_HANDLE);
		}

		m_ray_march_done = true;
		lock.unlock();
		m_cv.notify_all();
	}
}

void Application::present(Window* window, VkQueue queue, const uint32_t index, VkSemaphore wait_for) const
{
	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = nullptr;
	if (wait_for == VK_NULL_HANDLE)
	{
		present_info.waitSemaphoreCount = 0;
		present_info.pWaitSemaphores = nullptr;
	}
	else
	{
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = &wait_for;
	}
	present_info.swapchainCount = 1;
	present_info.pSwapchains = window->get_swapchain();
	present_info.pImageIndices = &index;
	VkResult result;
	present_info.pResults = &result;

	if ((result = vkQueuePresentKHR(queue, &present_info)) != VK_SUCCESS)
	{
#ifdef _DEBUG
		__debugbreak();
#else
		println("Failed to present image");
		exit(1);
#endif
	}
}

void Application::imgui_setup()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(m_window->get_glfw_window(), true);

	m_imgui_vulkan_state.queue = m_vulkan_context.create_graphics_queue();

	// Create the Render Pass
    {
        VkAttachmentDescription attachment = {};
        attachment.format = m_window->get_format();
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkAttachmentReference color_attachment = {};
        color_attachment.attachment = 0;
        color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment;
        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 1;
        info.pAttachments = &attachment;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dependency;
        VK_CHECK(vkCreateRenderPass(m_vulkan_context.get_device(), &info, 
				m_vulkan_context.get_allocation_callbacks(), 
				&m_imgui_vulkan_state.render_pass), 
			"imgui setup failed to create render pass!");
    }

	ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_vulkan_context.get_instance();
    init_info.PhysicalDevice = m_vulkan_context.get_physical_device();
    init_info.Device = m_vulkan_context.get_device();
    init_info.QueueFamily = m_vulkan_context.get_graphics_queue_index();
    init_info.Queue = m_imgui_vulkan_state.queue.get_queue();
    init_info.DescriptorPool = m_vulkan_context.get_descriptor_pool();
    init_info.Allocator = m_vulkan_context.get_allocation_callbacks();
    ImGui_ImplVulkan_Init(&init_info, m_imgui_vulkan_state.render_pass);

	 // Upload Fonts
    {
		// Create command pool
		VkCommandPoolCreateInfo command_pool_info;
		command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		command_pool_info.pNext = nullptr;
		command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		command_pool_info.queueFamilyIndex = m_vulkan_context.get_graphics_queue_index();

		VkResult result = vkCreateCommandPool(m_vulkan_context.get_device(), 
			&command_pool_info, m_vulkan_context.get_allocation_callbacks(), 
			&m_imgui_vulkan_state.command_pool);
		assert(result == VK_SUCCESS);

		// Create command buffer
		VkCommandBufferAllocateInfo alloc_info;
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.pNext = nullptr;
		alloc_info.commandPool = m_imgui_vulkan_state.command_pool;
		alloc_info.level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;

		result = vkAllocateCommandBuffers(m_vulkan_context.get_device(), &alloc_info, &m_imgui_vulkan_state.command_buffer);
		assert(result == VK_SUCCESS);

        VK_CHECK(vkResetCommandPool(init_info.Device, m_imgui_vulkan_state.command_pool, 0), "imgui setup failed to reset command pool!");

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(m_imgui_vulkan_state.command_buffer, &begin_info), "imgui setup failed to reset command pool!");

        ImGui_ImplVulkan_CreateFontsTexture(m_imgui_vulkan_state.command_buffer);

        VkSubmitInfo end_info = {};
        end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        end_info.commandBufferCount = 1;
        end_info.pCommandBuffers = &m_imgui_vulkan_state.command_buffer;
        VK_CHECK(vkEndCommandBuffer(m_imgui_vulkan_state.command_buffer), "imgui setup failed to end command buffer!");
        VK_CHECK(vkQueueSubmit(init_info.Queue, 1, &end_info, VK_NULL_HANDLE), "imgui setup failed to submit command buffer!");

        VK_CHECK(vkDeviceWaitIdle(init_info.Device), "imgui setup failed when waiting for device!");
        
        ImGui_ImplVulkan_InvalidateFontUploadObjects();
    }

	VkFenceCreateInfo fence_info;
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.pNext = nullptr;
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	VK_CHECK(vkCreateFence(m_vulkan_context.get_device(), &fence_info, m_vulkan_context.get_allocation_callbacks(), 
		&m_imgui_vulkan_state.command_buffer_idle), "Fence creation failed!");
}

void Application::imgui_shutdown()
{
	vkDeviceWaitIdle(m_vulkan_context.get_device());
	vkDestroyFence(m_vulkan_context.get_device(), m_imgui_vulkan_state.command_buffer_idle,
		m_vulkan_context.get_allocation_callbacks());

	vkDestroyRenderPass(m_vulkan_context.get_device(), m_imgui_vulkan_state.render_pass, 
				m_vulkan_context.get_allocation_callbacks());

	vkFreeCommandBuffers(m_vulkan_context.get_device(), m_imgui_vulkan_state.command_pool, 1, &m_imgui_vulkan_state.command_buffer);

	vkDestroyCommandPool(m_vulkan_context.get_device(), m_imgui_vulkan_state.command_pool, 
				m_vulkan_context.get_allocation_callbacks());


	ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void Application::imgui_draw(Framebuffer& framebuffer, VkSemaphore imgui_draw_complete_semaphore)
{
	ImGui::Render();

	{
		vkWaitForFences(m_vulkan_context.get_device(), 1, &m_imgui_vulkan_state.command_buffer_idle, VK_FALSE, ~0ull);
		vkResetFences(m_vulkan_context.get_device(), 1, &m_imgui_vulkan_state.command_buffer_idle);
		VK_CHECK(vkResetCommandPool(m_vulkan_context.get_device(), m_imgui_vulkan_state.command_pool, 0), "imgui failed to reset command pool!");
		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VK_CHECK(vkBeginCommandBuffer(m_imgui_vulkan_state.command_buffer, &info), "imgui failed to begin command buffer!");
	}
	{
		VkRenderPassBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass = m_imgui_vulkan_state.render_pass;
		info.framebuffer = framebuffer.get_framebuffer();
		info.renderArea.extent.width = m_window->get_size().x;
		info.renderArea.extent.height = m_window->get_size().y;
		info.clearValueCount = 1;
		VkClearValue clear_value;
		clear_value.color.float32[0] = 0.0f;
		clear_value.color.float32[1] = 0.0f;
		clear_value.color.float32[2] = 0.0f;
		clear_value.color.float32[3] = 0.0f;
		clear_value.depthStencil.depth = 0.0f;
		clear_value.depthStencil.stencil = 0;
		info.pClearValues = &clear_value;
		vkCmdBeginRenderPass(m_imgui_vulkan_state.command_buffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	}

	// Record Imgui Draw Data and draw funcs into command buffer
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_imgui_vulkan_state.command_buffer);

	// Submit command buffer
	vkCmdEndRenderPass(m_imgui_vulkan_state.command_buffer);
	{
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.waitSemaphoreCount = 0;
		info.pWaitSemaphores = nullptr;
		info.pWaitDstStageMask = &wait_stage;
		info.commandBufferCount = 1;
		info.pCommandBuffers = &m_imgui_vulkan_state.command_buffer;
		info.signalSemaphoreCount = 1;
		info.pSignalSemaphores = &imgui_draw_complete_semaphore;

		VK_CHECK(vkEndCommandBuffer(m_imgui_vulkan_state.command_buffer), "imgui ending command buffer failed!");
		VK_CHECK(vkQueueSubmit(m_imgui_vulkan_state.queue.get_queue(), 1, &info, m_imgui_vulkan_state.command_buffer_idle), "imgui submitting queue failed!");
	}
}

void Application::create_pipelines()
{
#ifdef RAY_MARCH_WINDOW
	m_ray_march_compute_pipeline = m_vulkan_context.create_compute_pipeline("terrain", m_ray_march_pipeline_layout, nullptr);
#endif
	VertexAttributes vertex_attributes;
	vertex_attributes.add_buffer();
	vertex_attributes.add_attribute(4);
	m_point_gen_graphics_pipeline = m_vulkan_context.create_graphics_pipeline(
		"point_generation",
		m_window->get_size(),
		m_point_gen_pipeline_layout_graphics,
		vertex_attributes,
		m_point_gen_render_pass,
		true,
		false,
		nullptr,
		nullptr,
		VK_PRIMITIVE_TOPOLOGY_POINT_LIST);

	VertexAttributes debug_attributes;
	debug_attributes.add_buffer();
	debug_attributes.add_attribute(3);
	debug_attributes.add_attribute(3);
	m_debug_pipeline = m_vulkan_context.create_graphics_pipeline("debug", m_window->get_size(), m_debug_pipeline_layout, debug_attributes, m_debug_render_pass, true, false, nullptr, nullptr, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);


	m_point_gen_compute_pipeline = m_vulkan_context.create_compute_pipeline("point_generation", m_point_gen_pipeline_layout_compute, nullptr);

	// Prefix sum
	m_point_gen_prefix_sum_pipeline = m_vulkan_context.create_compute_pipeline("prefix_sum", m_point_gen_pipeline_layout_compute, nullptr);

	m_quadtree.create_pipelines(*m_window);
}
