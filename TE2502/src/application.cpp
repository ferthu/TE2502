#include "application.hpp"
#include "graphics/graphics_queue.hpp"
#include "graphics/compute_queue.hpp"
#include "graphics/transfer_queue.hpp"
#include "graphics/gpu_memory.hpp"
#include "graphics/gpu_image.hpp"
#include "graphics/gpu_buffer.hpp"
#include "graphics/pipeline_layout.hpp"

#include "algorithm/common.hpp"
#include "algorithm/cpu_triangulate.hpp"
#include "algorithm/terrain_texture.hpp"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT
#include "stb_image_write.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <array>
#include <fstream>
#include <iostream>

#define RAY_MARCH_WINDOW

void error_callback(int error, const char* description)
{
	fprintf(stderr, "Error: %s\n", description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GLFW_TRUE);
}

Application::Application(uint32_t window_width, uint32_t window_height, std::vector<std::string> texture_paths) :
	m_tfile("shaders/vars.txt", "shaders/"),
	m_path_handler("camera_paths.txt"),
	m_window_width(window_width),
	m_window_height(window_height)
{
	m_tfile.compile_shaders();

	glfwSetErrorCallback(error_callback);

	int err = glfwInit();
	assert(err == GLFW_TRUE);

	m_main_queue = m_vulkan_context.create_graphics_queue();

	size_t cpu_image_data_size = window_width * window_height * sizeof(uint8_t) * 4;

#ifdef RAY_MARCH_WINDOW
	m_ray_march_window = new Window(window_width, window_height, "TE2502 - Ray March", m_vulkan_context, false);
#endif
	m_window = new Window(window_width, window_height, "TE2502 - Main", m_vulkan_context, true);

	m_main_camera = new Camera(m_window->get_glfw_window());
	m_debug_camera = new Camera(m_window->get_glfw_window());
	m_current_camera = m_main_camera;

	m_cpu_raster_image_memory = m_vulkan_context.allocate_host_memory(cpu_image_data_size + 2000);
	m_cpu_ray_march_image_memory = m_vulkan_context.allocate_host_memory(cpu_image_data_size + 2000);
	m_cpu_raster_image = GPUBuffer(m_vulkan_context, window_width * window_height * pixel_size, VK_IMAGE_USAGE_TRANSFER_DST_BIT, m_cpu_raster_image_memory);
#ifdef RAY_MARCH_WINDOW
	m_cpu_ray_march_image = GPUBuffer(m_vulkan_context, window_width * window_height * pixel_size, VK_IMAGE_USAGE_TRANSFER_DST_BIT, m_cpu_ray_march_image_memory);
#endif

	// Memory map image data
	vkMapMemory(m_vulkan_context.get_device(), m_cpu_raster_image.get_memory(), 0, cpu_image_data_size, 0, &m_raster_data);
#ifdef RAY_MARCH_WINDOW
	vkMapMemory(m_vulkan_context.get_device(), m_cpu_ray_march_image.get_memory(), 0, cpu_image_data_size, 0, &m_ray_march_data);
#endif

	m_path_handler.attach_camera(m_main_camera);

	glfwSetWindowPos(m_window->get_glfw_window(), 100, 100);

	terrain_texture_setup(texture_paths);

#ifdef RAY_MARCH_WINDOW
	glfwSetWindowPos(m_ray_march_window->get_glfw_window(), 0, 100);

	// Ray marching
	m_ray_march_set_layout = DescriptorSetLayout(m_vulkan_context);
	// Terrain textures
	for (uint32_t ii = 0; ii < max_terrain_textures; ++ii)
	{
		m_ray_march_set_layout.add_storage_image(VK_SHADER_STAGE_COMPUTE_BIT);
	}
	// Swapchain buffer
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

	m_raster_set_layout = DescriptorSetLayout(m_vulkan_context);
	// Terrain textures
	for (uint32_t ii = 0; ii < max_terrain_textures; ++ii)
	{
		m_raster_set_layout.add_storage_image(VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	m_raster_set_layout.create();

	m_raster_descriptor_set = DescriptorSet(m_vulkan_context, m_raster_set_layout);

	VkPushConstantRange push;
	push.offset = 0;
	push.size = sizeof(DrawData);
	push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	m_render_pass = RenderPass(
		m_vulkan_context,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		true,
		true,
		true,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	m_draw_pipeline_layout = PipelineLayout(m_vulkan_context);
	m_draw_pipeline_layout.add_descriptor_set_layout(m_raster_set_layout);
	m_draw_pipeline_layout.create(&push);

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
		m_window_states.swapchain_framebuffers[i].create(m_render_pass.get_render_pass(), m_window->get_size().x, m_window->get_size().y);

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

	m_debug_render_pass = RenderPass(
		m_vulkan_context, 
		VK_FORMAT_B8G8R8A8_UNORM, 
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		false, true, false, 
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	m_debug_drawer = DebugDrawer(m_vulkan_context, 110000);
	m_tri_debug_drawer = DebugDrawer(m_vulkan_context, 11000000);

	create_pipelines();

#ifdef RAY_MARCH_WINDOW
	// Start ray march thread
	m_ray_march_thread = std::thread(&Application::draw_ray_march, this);
#endif

	m_tri_thread = std::thread(&Application::triangulate_thread, this);

	cputri::setup(m_tfile, m_terrain_textures);

	cputri::gpu_data_setup(m_vulkan_context, m_gpu_buffer_memory, m_cpu_buffer_memory, m_gpu_buffer, m_cpu_buffer);
}

Application::~Application()
{
	imgui_shutdown();

	for (uint32_t i = 0; i < m_window->get_swapchain_size(); i++)
	{
		vkDestroySemaphore(m_vulkan_context.get_device(), m_imgui_vulkan_state.done_drawing_semaphores[i], 
			m_vulkan_context.get_allocation_callbacks());
	}

	// Free terrain textures
	for (uint32_t ii = 0; ii < max_terrain_textures; ++ii)
	{
		if (m_terrain_textures[ii].texture_data != m_default_texture)
		{
			stbi_image_free(m_terrain_textures[ii].texture_data);
		}
	}

	delete m_debug_camera;
	delete m_main_camera;

	vkUnmapMemory(m_vulkan_context.get_device(), m_cpu_raster_image.get_memory());
#ifdef RAY_MARCH_WINDOW
	delete m_ray_march_window;
	vkUnmapMemory(m_vulkan_context.get_device(), m_cpu_ray_march_image.get_memory());
#endif

	delete m_window;

	glfwTerminate();

	cputri::destroy(m_vulkan_context, m_cpu_buffer);
}

void Application::run(bool auto_triangulate)
{
	bool right_mouse_clicked = false;
	bool f_pressed = false;
	bool camera_switch_pressed = false;
	bool f5_pressed = false;
	bool q_pressed = false;
	bool left_mouse_clicked = false;
	bool k_pressed = false;

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

		// Clear terrain
		if (!q_pressed && glfwGetKey(m_window->get_glfw_window(), GLFW_KEY_Q) == GLFW_PRESS)
		{
			q_pressed = true;
			cputri::clear_terrain();
		}
		else if (q_pressed && glfwGetKey(m_window->get_glfw_window(), GLFW_KEY_Q) == GLFW_RELEASE)
			q_pressed = false;

		// Start/stop making new camera path
		if (!k_pressed && glfwGetKey(m_window->get_glfw_window(), GLFW_KEY_K) == GLFW_PRESS)
		{
			k_pressed = true;
			if (m_path_handler.get_mode() == MODE::NOTHING)
				m_path_handler.start_new_path();
			else
				m_path_handler.finish_new_path();
		}
		else if (k_pressed && glfwGetKey(m_window->get_glfw_window(), GLFW_KEY_K) == GLFW_RELEASE)
			k_pressed = false;

		// Cancel current path action
		if (!left_mouse_clicked && glfwGetMouseButton(m_window->get_glfw_window(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS
			&& !ImGui::GetIO().WantCaptureMouse)
		{
			left_mouse_clicked = true;
			m_path_handler.cancel_new_path();
			m_path_handler.stop_following();
		}
		else if (m_window && glfwGetMouseButton(m_window->get_glfw_window(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE
			&& !ImGui::GetIO().WantCaptureMouse)
			left_mouse_clicked = false;

		// Refinement button
		m_triangulate_button_held = glfwGetKey(m_window->get_glfw_window(), GLFW_KEY_R) == GLFW_PRESS;

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame(!m_window->get_mouse_locked() && m_show_imgui);
		ImGui_ImplGlfw_SetHandleCallbacks(!m_window->get_mouse_locked() && m_show_imgui);
		ImGui::NewFrame();

		// Reset debug drawer
		m_debug_drawer.new_frame();

		float dt = delta_time.count();
		if (dt > 0.05f)
			dt = 0.05f;
		update(dt, auto_triangulate);

		draw();
	}

	m_quit = true; 
#ifdef RAY_MARCH_WINDOW
	m_cv.notify_all();
	m_ray_march_thread.join();
#endif

	m_tri_thread.join();
}


void Application::update(const float dt, bool auto_triangulate)
{
	m_save_images = false;

	m_path_handler.update(dt);
	m_current_camera->update(dt, m_window->get_mouse_locked(), m_debug_drawer);

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

		const int fps = int(1.f / dt);
		static float running_fps = 0.f;
		const float a = 0.95f;
		running_fps = (a * running_fps) + (1.0f - a) * fps;
		m_time_to_sample1 -= dt;
		m_test_run_time += dt;
		if (m_is_testing && m_time_to_sample1 < 0.f)
		{
			if (!m_started_sampling)
				m_test_run_time = 0.f;
			m_started_sampling = true;
			m_time_to_sample1 = 1.0f / 30;
			m_fps_data.push_back(Fps{ (int)running_fps, m_test_run_time });
		}

		std::string text = "Frame info: " + std::to_string(running_fps) + "fps  "
			+ std::to_string(dt) + "s  " + std::to_string(int(100.f * dt / 0.016f)) + "%%";
		ImGui::Text(text.c_str());
		text = "Position: " + std::to_string(m_main_camera->get_pos().x) + ", " + std::to_string(m_main_camera->get_pos().y) + ", " + std::to_string(m_main_camera->get_pos().z);
		ImGui::Text(text.c_str());
		text = "Debug Position: " + std::to_string(m_debug_camera->get_pos().x) + ", " + std::to_string(m_debug_camera->get_pos().y) + ", " + std::to_string(m_debug_camera->get_pos().z);
		if (ImGui::Button("Teleport Debug to Main"))
			m_debug_camera->set_pos(m_main_camera->get_pos());
		ImGui::Text(text.c_str());
		ImGui::Checkbox("Draw Ray Marched View", &m_draw_ray_march);
		ImGui::Checkbox("Draw Triangulated View", &m_draw_triangulated);
		ImGui::Checkbox("Wireframe", &m_draw_wireframe);

		ImGui::Text((m_path_handler.get_mode() == MODE::CREATING) ? "Making path" : (m_path_handler.get_mode() == MODE::FOLLOWING) ? "Following path" : "");

		auto& path_names = m_path_handler.get_path_names();
		static std::string current_item = (path_names.size() == 0) ? "" : path_names[0];
		if (ImGui::BeginCombo("Camera Paths", current_item.c_str()))
		{
			for (auto& p : path_names)
			{
				bool is_selected = (current_item == p);
				if (ImGui::Selectable(p.c_str(), is_selected))
					current_item = p;
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		if (current_item != "" && m_path_handler.get_mode() == MODE::NOTHING && ImGui::Button("Follow"))
		{
			cputri::clear_terrain();
			m_path_handler.follow_path(current_item);
		}

		ImGui::InputText("Test name", m_test_name, 40);
		if (!m_is_testing && ImGui::Button("Start"))
		{
			m_is_testing = true;
			std::string path = "testresults";
			CreateDirectory(path.c_str(), NULL);
			path += "\\" + std::string(m_test_name);
			CreateDirectory(path.c_str(), NULL);
			m_rast_path = path + "\\rast";
			m_ray_path = path + "\\ray";
			CreateDirectory(m_rast_path.c_str(), NULL);
			CreateDirectory(m_ray_path.c_str(), NULL);

			m_test_data.clear();
			m_test_data.reserve(15000);
			m_fps_data.clear();
			m_fps_data.reserve(15000);

			m_started_sampling = false;
			m_time_to_sample1 = 1.0f; 
			m_time_to_sample2 = 1.0f + m_sample_rate;

			cputri::clear_terrain();
			m_path_handler.follow_path(current_item);
		}
		else if (m_is_testing && m_path_handler.get_mode() == MODE::NOTHING)
		{
			// Testing just ended
			std::ofstream out("testresults\\" + std::string(m_test_name) + "\\fps.txt");
			if (out.is_open())
			{
				out.precision(6);
				for (Fps f : m_fps_data)
				{
					out << f.time << "\t" << f.fps << "\n";
				}
			}
			out.close();
			out.open("testresults\\" + std::string(m_test_name) + "\\draw.txt");
			if (out.is_open())
			{
				out << "Runtime\t\tGen-N\tDraw-N\tGen-T    \tDraw-T\tNew-P\n";

				for (Sample s : m_test_data)
				{
					out.precision(6);
					out << std::fixed << s.run_time << "      \t"
						<< s.gen_nodes << "\t"
						<< s.draw_nodes << "\t"
						<< s.gen_tris << "         \t"
						<< s.draw_tris << "\t"
						<< s.new_points << "\n";
				}
				
				out << "\n\n" << m_test_run_time;
			}
			out.close();
			m_is_testing = false;
		}

		if (m_is_testing && m_started_sampling)
		{
			m_current_sample.gen_nodes += cputri::quadtree.num_generate_nodes_draw;
			m_current_sample.gen_tris += cputri::quadtree.generated_triangle_count;
			m_current_sample.draw_nodes += cputri::quadtree.num_draw_nodes_draw;
			m_current_sample.draw_tris += cputri::quadtree.drawn_triangle_count;
			m_current_sample.new_points += cputri::quadtree.new_points_added;
		}

		cputri::quadtree.generated_triangle_count = 0;
		cputri::quadtree.new_points_added = 0;

		m_time_to_sample2 -= dt;
		if (m_is_testing && m_time_to_sample2 < 0.f)
		{
			m_time_to_sample2 = m_sample_rate;
			
			m_current_sample.run_time = m_test_run_time;

			m_current_sample.gen_nodes *= 4;
			m_current_sample.gen_tris *= 4;
			m_current_sample.draw_nodes *= 4;
			m_current_sample.draw_tris *= 4;
			m_current_sample.new_points *= 4;

			m_test_data.push_back(m_current_sample);
			m_current_sample = Sample();

			if (m_save_snapshots)
				snapshot(m_rast_path + "\\img" + std::to_string(m_snapshot_number) + ".png"
					, m_ray_path + "\\img" + std::to_string(m_snapshot_number) + ".png");
			++m_snapshot_number;
		}

		ImGui::Checkbox("Save snapshots", &m_save_snapshots);

		if (ImGui::Button("Snapshot"))
			snapshot("snapshot_raster.png", "snapshot_ray_march.png");

		ImGui::Checkbox("Update terrain", &m_update_terrain);

		ImGui::End();
	}
	
	static bool show_debug = false;
	static int show_node = -1;
	static int refine_node = -1;
	static int refine_vertices = 8000;
	static int refine_triangles = 8000;
	static int sideshow_bob = -1;
	static float area_mult = 0.0f;
	static float curv_mult = 20.0f;
	static float threshold = 1.1f;

	// DEBUG
	static bool debug_generation = false;
	static int debug_stage = -1;
	static int debug_version = -1;

	static bool refine = true;

	static bool backup = false;
	static bool restore = false;

	static int restore_auto_version = 0;
	static bool restore_auto = false;
	static bool file_backup = false;
	static bool file_restore = false;

	if (m_show_imgui)
	{
		ImGui::Begin("Lol");
		ImGui::SliderInt("Index", &show_node, -1, 15);
		ImGui::SliderInt("Vertices per refine", &refine_vertices, 1, 10);
		ImGui::SliderInt("Triangles per refine", &refine_triangles, 1, 10);
		ImGui::SliderInt("Refine Node", &refine_node, -1, num_nodes - 1);
		ImGui::SliderInt("Sideshow", &sideshow_bob, -1, 9);

		ImGui::End();

		ImGui::Begin("cputri");
		ImGui::Checkbox("Auto Refine", &m_triangulate);
		if (ImGui::Button("Refine"))
		{
			refine = true;
		}
		if (ImGui::Button("Clear Terrain") && m_update_terrain)
		{
			cputri::clear_terrain();
		}

		ImGui::DragFloat("Area mult", &area_mult, 0.01f, 0.0f, 50.0f);
		ImGui::DragFloat("Curv mult", &curv_mult, 0.01f, 0.0f, 50.0f);
		ImGui::DragFloat("Threshold", &threshold, 0.01f, 0.0f, 50.0f);

		ImGui::Checkbox("Show Debug", &show_debug);

		ImGui::Checkbox("Debug Generation", &debug_generation);
		ImGui::SliderInt("Debug Stage", &debug_stage, -1, 20);
		ImGui::SliderInt("Debug Version", &debug_version, -1, 40);
		if (ImGui::Button("-"))
			--debug_version;
		if (ImGui::Button("+"))
			++debug_version;

		if (ImGui::Button("Backup"))
			backup = true;		
		if (ImGui::Button("Restore"))
			restore = true;

		ImGui::SliderInt("Restore Auto Version", &restore_auto_version, 0, cputri::get_num_autosaves());
		if (ImGui::Button("Restore Auto"))
			restore_auto = true;

		if (ImGui::Button("File Backup"))
			file_backup = true;
		if (ImGui::Button("File Restore"))
			file_restore = true;

		ImGui::End();

		std::vector<std::string> hovered_tris = cputri::get_hovered_tris();

		if (m_tri_data.show_hovered && m_tri_data.show_debug)
		{
			ImGui::Begin("Hovered Tris");

			for (std::string& s : hovered_tris)
			{
				ImGui::Text(s.c_str());
			}

			ImGui::End();
		}
	}

	// If triangulation thread is done, prepare it for another pass
	if (m_tri_done && m_tri_mutex.try_lock())
	{
		if (backup)
		{
			cputri::backup();
			backup = false;
		}
		if (restore)
		{
			cputri::restore();
			restore = false;
		}
		if (restore_auto)
		{
			cputri::restore_auto(restore_auto_version);
			restore_auto = false;
		}
		if (file_restore)
		{
			cputri::file_restore();
			file_restore = false;
		}
		if (file_backup)
		{
			cputri::file_backup();
			file_backup = false;
		}


		if (m_update_terrain)
		{
			m_tri_done = false;

			m_tri_debug_drawer.new_frame();

			refine = refine || m_triangulate || m_triangulate_button_held || auto_triangulate;

			// Fill TriData struct
			m_tri_data.dd = &m_tri_debug_drawer;

			m_tri_data.mc_fov = m_main_camera->get_fov();
			m_tri_data.mc_pos = m_main_camera->get_pos();
			m_tri_data.mc_view = m_main_camera->get_view();
			m_tri_data.mc_vp = m_main_camera->get_big_vp();
			m_tri_data.mc_frustum = m_main_camera->get_frustum();

			m_tri_data.cc_fov = m_current_camera->get_fov();
			m_tri_data.cc_pos = m_current_camera->get_pos();
			m_tri_data.cc_view = m_current_camera->get_view();
			m_tri_data.cc_vp = m_current_camera->get_big_vp();
			m_tri_data.cc_frustum = m_current_camera->get_frustum();

			// DEBUG
			m_tri_data.debug_generation = debug_generation;
			m_tri_data.debug_stage = debug_stage;
			m_tri_data.debug_version = debug_version;

			vec2 mouse_pos{ 0, 0 };
			// Get mouse pos
			const bool focused = glfwGetWindowAttrib(m_window->get_glfw_window(), GLFW_FOCUSED) != 0;
			if (focused)
			{
				double mouse_x, mouse_y;
				glfwGetCursorPos(m_window->get_glfw_window(), &mouse_x, &mouse_y);
				mouse_pos = vec2((float)mouse_x, (float)mouse_y);
			}

			int w, h;

			glfwGetWindowSize(m_window->get_glfw_window(), &w, &h);
			vec2 window_size = vec2(w, h);
			m_tri_data.mouse_pos = mouse_pos;
			m_tri_data.window_size = window_size;

			m_tri_data.triangulate = refine;
			refine = false;

			m_tri_data.show_debug = show_debug;
			m_tri_data.show_hovered = glfwGetKey(m_window->get_glfw_window(), GLFW_KEY_C) == GLFW_PRESS;
			m_tri_data.x_pressed = glfwGetKey(m_window->get_glfw_window(), GLFW_KEY_X) == GLFW_PRESS;
			m_tri_data.show_node = show_node;
			m_tri_data.refine_node = refine_node;
			m_tri_data.refine_vertices = refine_vertices;
			m_tri_data.refine_triangles = refine_triangles;
			m_tri_data.sideshow_bob = sideshow_bob;
			m_tri_data.area_mult = area_mult;
			m_tri_data.curv_mult = curv_mult;
			m_tri_data.threshold = threshold;

			m_tri_data.debug_draw_mutex = &m_debug_draw_mutex;
		}

		// Start recording and upload triangulated data to GPU
		m_main_queue.start_recording();
		cputri::upload(m_main_queue, m_gpu_buffer, m_cpu_buffer);
		m_main_queue.end_recording();
		m_main_queue.submit();
		m_main_queue.wait();

		m_tri_mutex.unlock();
	}
	else if (m_show_imgui)
	{
		ImGui::Begin("Working");
		ImGui::Text("Working...");
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

	if (m_save_images)
	{
		// Raster
		uint8_t* raster_image_data = (uint8_t*)m_raster_data;

		// Swap BGRA format to RGBA
		for (size_t ii = 0; ii < m_window_width * m_window_height; ++ii)
		{
			uint8_t temp_b = raster_image_data[ii * 4];

			// B = R
			raster_image_data[ii * 4] = raster_image_data[ii * 4 + 2];
			raster_image_data[ii * 4 + 2] = temp_b;
		}

		stbi_write_png(m_raster_image_name.c_str(), m_window_width, m_window_height, 4, raster_image_data, m_window_width * sizeof(uint8_t) * 4);

#ifdef RAY_MARCH_WINDOW
		// Ray march
		uint8_t* ray_march_image_data = (uint8_t*)m_ray_march_data;

		// Swap BGRA format to RGBA
		for (size_t ii = 0; ii < m_window_width * m_window_height; ++ii)
		{
			uint8_t temp_b = ray_march_image_data[ii * 4];

			// B = R
			ray_march_image_data[ii * 4] = ray_march_image_data[ii * 4 + 2];
			ray_march_image_data[ii * 4 + 2] = temp_b;
		}

		stbi_write_png(m_ray_march_image_name.c_str(), m_window_width, m_window_height, 4, ray_march_image_data, m_window_width * sizeof(uint8_t) * 4);
#endif
	}
}

void Application::draw_main()
{
	const uint32_t index = m_window->get_next_image();
	VkImage image = m_window->get_swapchain_image(index);


	// RENDER-------------------

	if (!m_main_queue.is_recording())
		m_main_queue.start_recording();

	// Transfer images to layouts for rendering targets
	m_main_queue.cmd_image_barrier(
		image,
		VK_ACCESS_MEMORY_READ_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

	m_main_queue.cmd_image_barrier(
		m_window_states.depth_images[index].get_image(),
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

	// Draw
	{
		m_draw_data.vp = m_current_camera->get_vp();
		m_draw_data.camera_pos = glm::vec4(m_current_camera->get_pos(), 1.0f);

		m_raster_descriptor_set.clear();
		// Terrain textures
		for (uint32_t ii = 0; ii < max_terrain_textures; ++ii)
		{
			m_raster_descriptor_set.add_storage_image(m_terrain_image_views[ii], VK_IMAGE_LAYOUT_GENERAL);
		}
		m_raster_descriptor_set.bind();

		m_main_queue.cmd_bind_descriptor_set(m_draw_pipeline_layout.get_pipeline_layout(), 0, m_raster_descriptor_set.get_descriptor_set());

		// Start renderpass
		if (!m_draw_wireframe)
			m_main_queue.cmd_bind_graphics_pipeline(m_draw_pipeline->m_pipeline);
		else
			m_main_queue.cmd_bind_graphics_pipeline(m_draw_wireframe_pipeline->m_pipeline);

		m_main_queue.cmd_begin_render_pass(m_render_pass, m_window_states.swapchain_framebuffers[index]);

		m_main_queue.cmd_push_constants(m_draw_pipeline_layout.get_pipeline_layout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(DrawData), &m_draw_data);

		// Draw terrain
		if (m_draw_triangulated)
			cputri::draw(m_main_queue, m_gpu_buffer, m_cpu_buffer);

		// End renderpass
		m_main_queue.cmd_end_render_pass();
	}

	m_main_queue.cmd_image_barrier(
		image,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	m_main_queue.cmd_image_barrier(
		m_window_states.depth_images[index].get_image(),
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

	// True if debug drawing mutex is locked
	bool locked = false;

	// Do debug drawing
	{
		m_debug_drawer.draw_line({ 0, 0, 0 }, { 1, 0, 0 }, { 1, 0, 0 });
		m_debug_drawer.draw_line({ 0, 0, 0 }, { 0, 1, 0 }, { 0, 1, 0 });
		m_debug_drawer.draw_line({ 0, 0, 0 }, { 0, 0, 1 }, { 0, 0, 1 });

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

		// ---------------------
		// Standard debug drawer
		// ---------------------

		if (m_debug_drawer.get_num_lines() > 0)
		{
			// Copy lines specified on CPU to GPU buffer
			m_main_queue.cmd_copy_buffer(m_debug_drawer.get_cpu_buffer().get_buffer(),
				m_debug_drawer.get_gpu_buffer().get_buffer(),
				m_debug_drawer.get_active_buffer_size());

			// Memory barrier for GPU buffer
			m_main_queue.cmd_buffer_barrier(m_debug_drawer.get_gpu_buffer().get_buffer(),
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);

			m_main_queue.cmd_begin_render_pass(m_debug_render_pass, m_window_states.swapchain_framebuffers[index]);

			m_main_queue.cmd_bind_graphics_pipeline(m_debug_pipeline->m_pipeline);
			m_main_queue.cmd_bind_vertex_buffer(m_debug_drawer.get_gpu_buffer().get_buffer(), 0);
			m_main_queue.cmd_push_constants(m_debug_pipeline_layout.get_pipeline_layout(), VK_SHADER_STAGE_VERTEX_BIT, sizeof(DebugDrawingFrameData), &m_debug_draw_frame_data);
			m_main_queue.cmd_draw(m_debug_drawer.get_num_lines() * 2);

			m_main_queue.cmd_end_render_pass();
		}

		// --------------------------
		// Triangulation debug drawer
		// --------------------------

		if (m_tri_debug_drawer.get_num_lines() > 0)
		{
			// Lock triangulation debug drawing until frame is done
			m_debug_draw_mutex.lock();
			locked = true;

			if (m_tri_debug_drawer.get_num_lines() > 0)
			{
				// Copy lines specified on CPU to GPU buffer
				m_main_queue.cmd_copy_buffer(m_tri_debug_drawer.get_cpu_buffer().get_buffer(),
					m_tri_debug_drawer.get_gpu_buffer().get_buffer(),
					m_tri_debug_drawer.get_active_buffer_size());

				// Memory barrier for GPU buffer
				m_main_queue.cmd_buffer_barrier(m_tri_debug_drawer.get_gpu_buffer().get_buffer(),
					VK_ACCESS_TRANSFER_WRITE_BIT,
					VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
					VK_PIPELINE_STAGE_TRANSFER_BIT,
					VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
			}

			m_main_queue.cmd_begin_render_pass(m_debug_render_pass, m_window_states.swapchain_framebuffers[index]);

			m_main_queue.cmd_bind_graphics_pipeline(m_debug_pipeline->m_pipeline);
			m_main_queue.cmd_bind_vertex_buffer(m_tri_debug_drawer.get_gpu_buffer().get_buffer(), 0);
			m_main_queue.cmd_push_constants(m_debug_pipeline_layout.get_pipeline_layout(), VK_SHADER_STAGE_VERTEX_BIT, sizeof(DebugDrawingFrameData), &m_debug_draw_frame_data);
			m_main_queue.cmd_draw(m_tri_debug_drawer.get_num_lines() * 2);

			// End
			m_main_queue.cmd_end_render_pass();
		}

		m_main_queue.cmd_image_barrier(
			image,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	}

	if (m_save_images)
	{
		// Transfer swapchain image
		m_main_queue.cmd_image_barrier(
			image,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_TRANSFER_READ_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT);

		m_main_queue.cmd_copy_image_to_buffer(m_cpu_raster_image.get_buffer(), 
			image, 
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 
			{ m_window_width, m_window_height, 1 });

		m_main_queue.cmd_image_barrier(
			image,
			VK_ACCESS_TRANSFER_READ_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	}

	m_main_queue.end_recording();
	m_main_queue.submit();
	m_main_queue.wait();

	if (locked)
		m_debug_draw_mutex.unlock();

	imgui_draw(m_imgui_vulkan_state.swapchain_framebuffers[index], m_imgui_vulkan_state.done_drawing_semaphores[index]);

	{
		std::scoped_lock lock(m_present_lock);
		present(m_window, m_main_queue.get_queue(), index, m_imgui_vulkan_state.done_drawing_semaphores[index]);
	}
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
			// Terrain textures
			for (uint32_t ii = 0; ii < max_terrain_textures; ++ii)
			{
				m_ray_march_image_descriptor_set.add_storage_image(m_terrain_image_views[ii], VK_IMAGE_LAYOUT_GENERAL);
			}
			// Swapchain image
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

			if (m_save_images)
			{
				// Transfer swapchain image
				m_ray_march_compute_queue.cmd_image_barrier(
					image,
					VK_ACCESS_SHADER_WRITE_BIT,
					VK_ACCESS_TRANSFER_READ_BIT,
					VK_IMAGE_LAYOUT_GENERAL,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					VK_PIPELINE_STAGE_TRANSFER_BIT);

				m_ray_march_compute_queue.cmd_copy_image_to_buffer(m_cpu_ray_march_image.get_buffer(),
					image,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0,
					{ m_window_width, m_window_height, 1 });

				m_ray_march_compute_queue.cmd_image_barrier(
					image,
					VK_ACCESS_TRANSFER_READ_BIT,
					VK_ACCESS_SHADER_WRITE_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_IMAGE_LAYOUT_GENERAL,
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_PIPELINE_STAGE_TRANSFER_BIT,
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
			}

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

			{
				std::scoped_lock lock(m_present_lock);
				present(m_ray_march_window, m_ray_march_compute_queue.get_queue(), index, VK_NULL_HANDLE);
			}
		}

		m_ray_march_done = true;
		lock.unlock();
		m_cv.notify_all();
	}
}

void Application::triangulate_thread()
{
	while (!m_quit)
	{
		if (!m_tri_done)
		{
			m_tri_mutex.lock();

			cputri::run(&m_tri_data);
			m_tri_done = true;

			m_tri_mutex.unlock();
		}
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
        attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
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

	VertexAttributes debug_attributes;
	debug_attributes.add_buffer();
	debug_attributes.add_attribute(3);
	debug_attributes.add_attribute(3);
	m_debug_pipeline = m_vulkan_context.create_graphics_pipeline("debug", m_window->get_size(), m_debug_pipeline_layout, debug_attributes, m_debug_render_pass, true, false, nullptr, nullptr, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);

	VertexAttributes va(m_vulkan_context);
	va.add_buffer();
	va.add_attribute(4);
	m_draw_pipeline = m_vulkan_context.create_graphics_pipeline(
		"terrain_draw",
		m_window->get_size(),
		m_draw_pipeline_layout,
		va,
		m_render_pass,
		true,
		false,
		nullptr,
		nullptr);

	m_draw_wireframe_pipeline = m_vulkan_context.create_graphics_pipeline(
		"terrain_draw",
		m_window->get_size(),
		m_draw_pipeline_layout,
		va,
		m_render_pass,
		true,
		false,
		nullptr,
		nullptr,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_POLYGON_MODE_LINE);
}

void Application::snapshot(std::string raster_image_name, std::string raymarch_image_name)
{
	m_save_images = true;
	m_raster_image_name = raster_image_name;
	m_ray_march_image_name = raymarch_image_name;
}

void Application::terrain_texture_setup(std::vector<std::string>& texture_path_names)
{
	// Setup default texture
	m_default_texture[0] = 255;
	m_default_texture[1] = 0;
	m_default_texture[2] = 255;
	m_default_texture[3] = 255;

	// Initialize texture data
	for (uint32_t ii = 0; ii < max_terrain_textures; ++ii)
	{
		int comp;

		// Use default texture
		if (ii >= texture_path_names.size())
		{
			m_terrain_textures[ii].texture_data = nullptr;
		}
		else
		{
			// Load image
			m_terrain_textures[ii].texture_data = stbi_load(texture_path_names[ii].c_str(), &m_terrain_textures[ii].x, &m_terrain_textures[ii].y, &comp, 4);

			if (m_terrain_textures[ii].texture_data == nullptr)
			{
				std::cerr << "Failed to load texture '" << texture_path_names[ii] << "'\n";
			}
		}

		// Use default texture
		if (m_terrain_textures[ii].texture_data == nullptr)
		{
			m_terrain_textures[ii].texture_data = m_default_texture;
			m_terrain_textures[ii].x = 1;
			m_terrain_textures[ii].y = 1;
		}
	}

	// Upload textures to GPU
	for (uint32_t ii = 0; ii < max_terrain_textures; ++ii)
	{
		GPUMemory temp_cpu_texture_mem = m_vulkan_context.allocate_host_memory(pixel_size * m_terrain_textures[ii].x * m_terrain_textures[ii].y + 100000);
		GPUBuffer temp_cpu_texture = GPUBuffer(m_vulkan_context, m_terrain_textures[ii].x * m_terrain_textures[ii].y * pixel_size,
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			temp_cpu_texture_mem);

		// Memory map image
		void* temp_data;
		vkMapMemory(m_vulkan_context.get_device(), temp_cpu_texture.get_memory(), 0, VK_WHOLE_SIZE, 0, &temp_data);

		// Copy data into CPU image
		memcpy(temp_data, m_terrain_textures[ii].texture_data, pixel_size * m_terrain_textures[ii].x * m_terrain_textures[ii].y);


		// Unmap memory
		vkUnmapMemory(m_vulkan_context.get_device(), temp_cpu_texture.get_memory());

		// Create GPU image
		m_terrain_image_memory[ii] = m_vulkan_context.allocate_device_memory(pixel_size * m_terrain_textures[ii].x * m_terrain_textures[ii].y + 3000000);
		m_terrain_images[ii] = GPUImage(m_vulkan_context,
			{ (uint32_t)m_terrain_textures[ii].x, (uint32_t)m_terrain_textures[ii].y, 1 },
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
			m_terrain_image_memory[ii]);

		// Create image view
		m_terrain_image_views[ii] = ImageView(m_vulkan_context, m_terrain_images[ii], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

		// Copy image to GPU
		m_main_queue.start_recording();

		// Transfer gpu image layout
		m_main_queue.cmd_image_barrier(
			m_terrain_images[ii].get_image(),
			0,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT);

		// Copy image
		m_main_queue.cmd_copy_buffer_to_image(temp_cpu_texture.get_buffer(), 
			m_terrain_images[ii].get_image(), 
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
			0,
			{ (uint32_t)m_terrain_textures[ii].x, (uint32_t)m_terrain_textures[ii].y, 1 });

		// Transfer to read only layout
		m_main_queue.cmd_image_barrier(
			m_terrain_images[ii].get_image(),
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);

		m_main_queue.end_recording();
		m_main_queue.submit();
		m_main_queue.wait();

	}
}
