#include "application.hpp"
#include "graphics/graphics_queue.hpp"
#include "graphics/compute_queue.hpp"
#include "graphics/transfer_queue.hpp"
#include "graphics/gpu_memory.hpp"
#include "graphics/gpu_image.hpp"
#include "graphics/gpu_buffer.hpp"
#include "graphics/pipeline_layout.hpp"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

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

	m_window = new Window(1080, 720, "TE2502 - Main", m_vulkan_context);
	m_ray_march_window = new Window(1080, 720, "TE2502 - Ray March", m_vulkan_context);
	m_main_camera = new Camera(m_ray_march_window->get_glfw_window());
	m_debug_camera = new Camera(m_ray_march_window->get_glfw_window());
	m_current_camera = m_main_camera;

	glfwSetWindowPos(m_window->get_glfw_window(), 1000, 100);

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

	m_ray_march_compute_pipeline = m_vulkan_context.create_compute_pipeline("terrain", m_ray_march_pipeline_layout);

	m_ray_march_compute_queue = m_vulkan_context.create_compute_queue();
	// !Ray marching

	// Point generation
	// Compute
	m_point_gen_buffer_set_layout_compute = DescriptorSetLayout(m_vulkan_context);
	m_point_gen_buffer_set_layout_compute.add_uniform_buffer(VK_SHADER_STAGE_COMPUTE_BIT);
	m_point_gen_buffer_set_layout_compute.add_storage_buffer(VK_SHADER_STAGE_COMPUTE_BIT);
	m_point_gen_buffer_set_layout_compute.create();

	m_point_gen_buffer_set_compute = DescriptorSet(m_vulkan_context, m_point_gen_buffer_set_layout_compute);

	m_point_gen_pipeline_layout_compute = PipelineLayout(m_vulkan_context);
	m_point_gen_pipeline_layout_compute.add_descriptor_set_layout(m_point_gen_buffer_set_layout_compute);
	{
		// Set up push constant range for frame data
		VkPushConstantRange push_range;
		push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		push_range.offset = 0;
		push_range.size = sizeof(PointGenerationFrameData);

		m_point_gen_pipeline_layout_compute.create(&push_range);
	}

	m_point_gen_compute_pipeline = m_vulkan_context.create_compute_pipeline("test", m_point_gen_pipeline_layout_compute);

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
		push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		push_range.offset = 0;
		push_range.size = sizeof(PointGenerationFrameData);

		m_point_gen_pipeline_layout_graphics.create(&push_range);
	}

	VertexAttributes vertex_attributes;
	m_point_gen_graphics_pipeline = m_vulkan_context.create_graphics_pipeline("test", m_window->get_size(), m_point_gen_pipeline_layout_compute, vertex_attributes);
	// !Point generation

	m_vulkan_context.create_render_pass(m_ray_march_window);

	glfwSetKeyCallback(m_ray_march_window->get_glfw_window(), key_callback);
	glfwSetKeyCallback(m_window->get_glfw_window(), key_callback);

	imgui_setup();

	m_ray_march_window_states.swapchain_framebuffers.resize(m_ray_march_window->get_swapchain_size());
	for (uint32_t i = 0; i < m_ray_march_window->get_swapchain_size(); i++)
	{
		m_ray_march_window_states.swapchain_framebuffers[i] = Framebuffer(m_vulkan_context);
		m_ray_march_window_states.swapchain_framebuffers[i].add_attachment(m_ray_march_window->get_swapchain_image_view(i));
		m_ray_march_window_states.swapchain_framebuffers[i].create(m_imgui_vulkan_state.render_pass, m_ray_march_window->get_size().x, m_ray_march_window->get_size().y);
	}
	m_window_states.swapchain_framebuffers.resize(m_window->get_swapchain_size());
	for (uint32_t i = 0; i < m_window->get_swapchain_size(); i++)
	{
		m_window_states.swapchain_framebuffers[i] = Framebuffer(m_vulkan_context);
		m_window_states.swapchain_framebuffers[i].add_attachment(m_window->get_swapchain_image_view(i));
		m_window_states.swapchain_framebuffers[i].create(m_imgui_vulkan_state.render_pass, m_window->get_size().x, m_window->get_size().y);
	}

	m_imgui_vulkan_state.done_drawing_semaphores.resize(m_ray_march_window->get_swapchain_size());
	for (uint32_t i = 0; i < m_ray_march_window->get_swapchain_size(); i++)
	{
		VkSemaphoreCreateInfo create_info;
		create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		create_info.pNext = nullptr;
		create_info.flags = 0;
		VK_CHECK(vkCreateSemaphore(m_vulkan_context.get_device(), &create_info, m_vulkan_context.get_allocation_callbacks(), 
			&m_imgui_vulkan_state.done_drawing_semaphores[i]), "Semaphore creation failed!")
	}
}

Application::~Application()
{
	imgui_shutdown();

	for (uint32_t i = 0; i < m_ray_march_window->get_swapchain_size(); i++)
	{
		vkDestroySemaphore(m_vulkan_context.get_device(), m_imgui_vulkan_state.done_drawing_semaphores[i], 
			m_vulkan_context.get_allocation_callbacks());
	}

	delete m_debug_camera;
	delete m_main_camera;
	delete m_ray_march_window;
	delete m_window;

	glfwTerminate();
}

void Application::run()
{
	bool right_mouse_clicked = false;
	bool f_pressed = false;
	bool demo_window = true;

	while (!glfwWindowShouldClose(m_ray_march_window->get_glfw_window()))
	{
		auto stop_time = m_timer;
		m_timer = std::chrono::high_resolution_clock::now();
		std::chrono::duration<float> delta_time = m_timer - stop_time;

		glfwPollEvents();

		// Toggle camera controls
		if (!right_mouse_clicked && glfwGetMouseButton(m_ray_march_window->get_glfw_window(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS
			&& !ImGui::GetIO().WantCaptureMouse)
		{
			m_ray_march_window->set_mouse_locked(!m_ray_march_window->get_mouse_locked());
			right_mouse_clicked = true;
		}
		else if (right_mouse_clicked && glfwGetMouseButton(m_ray_march_window->get_glfw_window(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_RELEASE
			&& !ImGui::GetIO().WantCaptureMouse)
			right_mouse_clicked = false;

		// Toggle imgui
		if (!f_pressed && glfwGetKey(m_ray_march_window->get_glfw_window(), GLFW_KEY_F) == GLFW_PRESS)
		{
			f_pressed = true;
			m_show_imgui = !m_show_imgui;
		}
		else if (f_pressed && glfwGetKey(m_ray_march_window->get_glfw_window(), GLFW_KEY_F) == GLFW_RELEASE)
			f_pressed = false;

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame(!m_ray_march_window->get_mouse_locked() && m_show_imgui);
		ImGui_ImplGlfw_SetHandleCallbacks(!m_ray_march_window->get_mouse_locked() && m_show_imgui);
		ImGui::NewFrame();

		update(delta_time.count());

		draw();
	}
}


void Application::update(const float dt)
{
	m_current_camera->update(dt, m_ray_march_window->get_mouse_locked());

	m_ray_march_frame_data.view = m_current_camera->get_view();
	m_ray_march_frame_data.screen_size = m_ray_march_window->get_size();
	m_ray_march_frame_data.position = glm::vec4(m_current_camera->get_pos(), 0);

	if (m_show_imgui)
	{
		ImGui::Begin("Info");
		std::string text = "Frame info: " + std::to_string(int(1.f / dt)) + "fps  "
			+ std::to_string(dt) + "s  " + std::to_string(int(100.f * dt / 0.016f)) + "%%";
		ImGui::Text(text.c_str());
		text = "Position: " + std::to_string(m_current_camera->get_pos().x) + ", " + std::to_string(m_current_camera->get_pos().y) + ", " + std::to_string(m_current_camera->get_pos().z);
		ImGui::Text(text.c_str());
		ImGui::End();
	}
}


void Application::draw()
{
	draw_main();
	draw_ray_march();
}

void Application::draw_main()
{
	const uint32_t index = m_window->get_next_image();
	VkImage image = m_window->get_swapchain_image(index);

	m_point_gen_buffer_set_compute.clear();
	//m_point_gen_buffer_set_compute.add_uniform_buffer();
	//m_point_gen_buffer_set_compute.add_storage_buffer();
	m_point_gen_buffer_set_compute.bind();

	m_point_gen_queue.start_recording();

	// RENDER-------------------

	// Fritjof stuff
	{
		// Bind pipeline
		m_point_gen_queue.cmd_bind_compute_pipeline(m_point_gen_compute_pipeline->m_pipeline);

		// Bind descriptor set
		m_point_gen_queue.cmd_bind_descriptor_set_compute(m_point_gen_compute_pipeline->m_pipeline_layout.get_pipeline_layout(), 0, m_point_gen_image_descriptor_set.get_descriptor_set());

		// Push frame data
		m_point_gen_queue.cmd_push_constants(m_point_gen_pipeline_layout_compute.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, sizeof(RayMarchFrameData), &m_point_gen_frame_data);

		// Dispatch
		const uint32_t group_size = 32;
		m_point_gen_queue.cmd_dispatch(m_window->get_size().x / group_size + 1, m_window->get_size().y / group_size + 1, 1);

		m_point_gen_queue.end_recording();
		m_point_gen_queue.submit();
		m_point_gen_queue.wait();
	}

	// end of RENDER------------------

	// Transfer swapchin image to color attachment
	m_debug_queue.cmd_image_barrier(image,
		VK_ACCESS_MEMORY_READ_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

	// Do debug drawing
	m_debug_queue.start_recording();
	// wip

	imgui_draw(m_window_states.swapchain_framebuffers[index], m_imgui_vulkan_state.done_drawing_semaphores[index]);

	present(m_window, m_point_gen_queue.get_queue(), index, m_imgui_vulkan_state.done_drawing_semaphores[index]);

	m_debug_queue.end_recording();
	m_debug_queue.submit();
	m_debug_queue.wait();
}

void Application::draw_ray_march()
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

	present(m_ray_march_window, m_ray_march_compute_queue.get_queue(), index, m_imgui_vulkan_state.done_drawing_semaphores[index]);
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

    ImGui_ImplGlfw_InitForVulkan(m_ray_march_window->get_glfw_window(), true);

	m_imgui_vulkan_state.queue = m_vulkan_context.create_graphics_queue();

	// Create the Render Pass
    {
        VkAttachmentDescription attachment = {};
        attachment.format = m_ray_march_window->get_format();
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
		info.renderArea.extent.width = m_ray_march_window->get_size().x;
		info.renderArea.extent.height = m_ray_march_window->get_size().y;
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