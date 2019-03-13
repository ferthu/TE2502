#include "quadtree.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

Quadtree::~Quadtree()
{
	destroy();
}

Quadtree::Quadtree(Quadtree&& other)
{
	move_from(std::move(other));
}

Quadtree& Quadtree::operator=(Quadtree&& other)
{
	if (this != &other)
		move_from(std::move(other));

	return *this;
}

Quadtree::Quadtree(
	VulkanContext& context,
	float total_side_length,
	uint32_t levels,
	VkDeviceSize max_nodes,
	VkDeviceSize max_node_indices,
	VkDeviceSize max_node_vertices,
	VkDeviceSize max_node_new_points,
	Window& window,
	GraphicsQueue& queue)
	: m_context(&context),
		m_total_side_length(total_side_length),
		m_levels(levels),
		m_max_nodes(max_nodes),
		m_max_indices(max_node_indices),
		m_max_vertices(max_node_vertices),
		m_max_node_new_points(max_node_new_points)
{
	assert(levels > 0);

	m_buffer_index_filled = new bool[max_nodes];
	memset(m_buffer_index_filled, 0, max_nodes * sizeof(bool));

	m_num_generate_nodes = 0;
	m_generate_nodes = new GenerateInfo[max_nodes];

	m_num_draw_nodes = 0;
	m_draw_nodes = new uint32_t[max_nodes];

	m_node_memory_size = 
		sizeof(VkDrawIndexedIndirectCommand) + 
		sizeof(BufferNodeHeader) + 
		max_node_indices * sizeof(uint32_t) + // Indices
		max_node_vertices * sizeof(glm::vec4) + // Vertices
		(max_node_indices / 3) * sizeof(Triangle) + // Circumcentre and circumradius
		max_node_new_points * sizeof(glm::vec4); // New points

	// Add space for an additional two vec2's to store the quadtree min and max
	m_cpu_index_buffer_size = (1 << levels) * (1 << levels) * sizeof(uint32_t) + sizeof(glm::vec2) * 2;
	m_cpu_index_buffer_size += 64 - (m_cpu_index_buffer_size % 64);

	m_memory = context.allocate_device_memory(m_cpu_index_buffer_size + m_node_memory_size * max_nodes + 1000);
	
	m_buffer = GPUBuffer(context, m_cpu_index_buffer_size + m_node_memory_size * max_nodes,
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | 
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | 
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | 
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		m_memory);

	m_cpu_index_buffer_memory = context.allocate_host_memory(m_cpu_index_buffer_size + 1000);
	m_cpu_index_buffer = GPUBuffer(context, m_cpu_index_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, m_cpu_index_buffer_memory);
	VK_CHECK(vkMapMemory(context.get_device(), m_cpu_index_buffer.get_memory(), 0, m_cpu_index_buffer_size, 0, (void**) &m_node_index_to_buffer_index), "Failed to map memory!");

	// Point to the end of cpu index buffer
	m_quadtree_minmax = (glm::vec2*) (((char*)m_node_index_to_buffer_index) + (1 << levels) * (1 << levels) * sizeof(uint32_t));

	float half_length = m_total_side_length * 0.5f;
	m_quadtree_minmax[0] = glm::vec2(-half_length, -half_length);
	m_quadtree_minmax[1] = glm::vec2(half_length, half_length);

	// (1 << levels) is number of nodes per axis
	memset(m_node_index_to_buffer_index, INVALID, (1 << levels) * (1 << levels) * sizeof(uint32_t));

	m_generation_set_layout = DescriptorSetLayout(context);
	m_generation_set_layout.add_storage_buffer(VK_SHADER_STAGE_COMPUTE_BIT);
	m_generation_set_layout.create();
	m_descriptor_set = DescriptorSet(context, m_generation_set_layout);
	m_generation_pipeline_layout = PipelineLayout(context);
	m_generation_pipeline_layout.add_descriptor_set_layout(m_generation_set_layout);
	VkPushConstantRange push;
	push.offset = 0;
	push.size = sizeof(GenerationData);
	push.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	m_generation_pipeline_layout.create(&push);

	m_render_pass = RenderPass(
			context, 
			VK_FORMAT_B8G8R8A8_UNORM, 
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
			true, 
			true, 
			true, 
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	m_draw_pipeline_layout = PipelineLayout(context);
	m_draw_pipeline_layout.create(&push);

	///// TRIANGULATION
	m_triangulation_pipeline_layout = PipelineLayout(context);
	m_triangulation_pipeline_layout.add_descriptor_set_layout(m_generation_set_layout);
	push.offset = 0;
	push.size = sizeof(TriangulationData);
	push.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	m_triangulation_pipeline_layout.create(&push);

	// Terrain processing
	m_triangle_processing_layout = DescriptorSetLayout(context);
	m_triangle_processing_layout.add_storage_buffer(VK_SHADER_STAGE_COMPUTE_BIT);
	m_triangle_processing_layout.create();

	m_triangle_processing_set = DescriptorSet(context, m_triangle_processing_layout);

	m_triangle_processing_pipeline_layout = PipelineLayout(context);
	m_triangle_processing_pipeline_layout.add_descriptor_set_layout(m_triangle_processing_layout);
	{
		// Set up push constant range for frame data
		VkPushConstantRange push_range;
		push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT;
		push_range.offset = 0;
		push_range.size = sizeof(TriangleProcessingFrameData);

		m_triangle_processing_pipeline_layout.create(&push_range);
	}

	error_metric_setup(window, queue);
	create_pipelines(window);
}

void Quadtree::intersect(GraphicsQueue& queue, Frustum& frustum, DebugDrawer& dd)
{
	m_num_generate_nodes = 0;
	m_num_draw_nodes = 0;
	
	float half_length = m_total_side_length * 0.5f;

	// Gather status of nodes
	intersect(frustum, dd, { {-half_length, -half_length},
		{half_length, half_length} }, 0, 0, 0);

	// Dispatch terrain generation
	queue.cmd_bind_compute_pipeline(m_generation_pipeline->m_pipeline);
	m_descriptor_set.clear();
	m_descriptor_set.add_storage_buffer(m_buffer);
	m_descriptor_set.bind();
	queue.cmd_bind_descriptor_set_compute(m_generation_pipeline_layout.get_pipeline_layout(), 0, m_descriptor_set.get_descriptor_set());

	for (uint32_t i = 0; i < m_num_generate_nodes; i++)
	{
		m_push_data.node_index = m_generate_nodes[i].index;
		m_push_data.min = m_generate_nodes[i].min;
		m_push_data.max = m_generate_nodes[i].max;

		queue.cmd_push_constants(
			m_generation_pipeline_layout.get_pipeline_layout(), 
			VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
			sizeof(GenerationData), 
			&m_push_data);

		queue.cmd_dispatch(1, 1, 1);
	}

	// Copy CPU index buffer to GPU
	queue.cmd_copy_buffer(m_cpu_index_buffer.get_buffer(), m_buffer.get_buffer(), m_cpu_index_buffer_size);
	queue.cmd_buffer_barrier(m_buffer.get_buffer(),
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		m_cpu_index_buffer_size);

	// Memory barriers
	for (uint32_t i = 0; i < m_num_generate_nodes; i++)
	{
		queue.cmd_buffer_barrier(m_buffer.get_buffer(),
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
			m_cpu_index_buffer_size + m_generate_nodes[i].index * m_node_memory_size,
			m_node_memory_size);
	}
}

void Quadtree::draw_terrain(GraphicsQueue& queue, Frustum& frustum, DebugDrawer& dd, Framebuffer& framebuffer, Camera& camera, bool wireframe)
{
	m_push_data.vp = camera.get_vp();
	m_push_data.camera_pos = glm::vec4(camera.get_pos(), 1.0f);

	// Start renderpass
	if (!wireframe)
		queue.cmd_bind_graphics_pipeline(m_draw_pipeline->m_pipeline);
	else
		queue.cmd_bind_graphics_pipeline(m_draw_wireframe_pipeline->m_pipeline);

	queue.cmd_begin_render_pass(m_render_pass, framebuffer);

	queue.cmd_push_constants(m_draw_pipeline_layout.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(GenerationData), &m_push_data);

	// Render nonupdated terrain
	for (uint32_t i = 0; i < m_num_draw_nodes; i++)
	{
		queue.cmd_bind_index_buffer(m_buffer.get_buffer(), get_index_offset_of_node(m_draw_nodes[i]));
		queue.cmd_bind_vertex_buffer(m_buffer.get_buffer(), get_vertex_offset_of_node(m_draw_nodes[i]));
		queue.cmd_draw_indexed_indirect(m_buffer.get_buffer(), get_offset_of_node(m_draw_nodes[i]));
	}

	// Render newly generated terrain
	for (uint32_t i = 0; i < m_num_generate_nodes; i++)
	{
		queue.cmd_bind_index_buffer(m_buffer.get_buffer(), get_index_offset_of_node(m_generate_nodes[i].index));
		queue.cmd_bind_vertex_buffer(m_buffer.get_buffer(), get_vertex_offset_of_node(m_generate_nodes[i].index));
		queue.cmd_draw_indexed_indirect(m_buffer.get_buffer(), get_offset_of_node(m_generate_nodes[i].index));
	}

	// End renderpass
	queue.cmd_end_render_pass();
}

void Quadtree::process_triangles(GraphicsQueue& queue, Camera& camera, Window& window, float em_threshold, float area_multiplier, float curvature_multiplier)
{
	m_triangle_processing_frame_data.vp = camera.get_vp();
	m_triangle_processing_frame_data.camera_position = glm::vec4(camera.get_pos(), 0);

	m_triangle_processing_frame_data.screen_size = window.get_size();
	m_triangle_processing_frame_data.em_threshold = em_threshold;
	m_triangle_processing_frame_data.area_multiplier = area_multiplier;
	m_triangle_processing_frame_data.curvature_multiplier = curvature_multiplier;

	m_triangle_processing_set.clear();
	m_triangle_processing_set.add_storage_buffer(get_buffer());
	m_triangle_processing_set.bind();


	// Bind pipeline
	queue.cmd_bind_compute_pipeline(m_triangle_processing_compute_pipeline->m_pipeline);

	// Bind descriptor set
	queue.cmd_bind_descriptor_set_compute(m_triangle_processing_compute_pipeline->m_pipeline_layout.get_pipeline_layout(), 0, m_triangle_processing_set.get_descriptor_set());


	// Nonupdated terrain
	for (uint32_t i = 0; i < m_num_draw_nodes; i++)
	{
		m_triangle_processing_frame_data.node_index = m_draw_nodes[i];
		queue.cmd_push_constants(
			m_triangle_processing_pipeline_layout.get_pipeline_layout(), 
			VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT, 
			sizeof(TriangleProcessingFrameData), 
			&m_triangle_processing_frame_data);

		// Dispatch triangle processing
		queue.cmd_dispatch(1, 1, 1);
	}

	// Newly generated terrain
	for (uint32_t i = 0; i < m_num_generate_nodes; i++)
	{
		m_triangle_processing_frame_data.node_index = m_generate_nodes[i].index;
		queue.cmd_push_constants(
			m_triangle_processing_pipeline_layout.get_pipeline_layout(),
			VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT,
			sizeof(TriangleProcessingFrameData),
			&m_triangle_processing_frame_data);

		// Dispatch triangle processing
		queue.cmd_dispatch(1, 1, 1);
	}

	// Memory barrier for GPU buffer
	queue.cmd_buffer_barrier(get_buffer().get_buffer(),
		VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
		VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
}

void Quadtree::draw_error_metric(
	GraphicsQueue& queue,
	Frustum& frustum,
	DebugDrawer& dd,
	Framebuffer& framebuffer,
	Camera& camera,
	bool draw_to_screen,
	float area_multiplier,
	float curvature_multiplier,
	bool wireframe)
{
	// Draw error metric image
	m_em_push_data.vp = camera.get_vp();
	m_em_push_data.camera_pos = glm::vec4(camera.get_pos(), 1.0f);
	m_em_push_data.screen_size = glm::vec2(m_em_framebuffer.get_width(), m_em_framebuffer.get_width());
	m_em_push_data.area_multiplier = area_multiplier;
	m_em_push_data.curvature_multiplier = curvature_multiplier;

	queue.cmd_push_constants(m_em_pipeline_layout.get_pipeline_layout(), VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(ErrorMetricData), &m_em_push_data);

	queue.cmd_bind_graphics_pipeline(m_em_pipeline->m_pipeline);
	queue.cmd_begin_render_pass(m_em_render_pass, m_em_framebuffer);

	// Render nonupdated terrain
	for (uint32_t i = 0; i < m_num_draw_nodes; i++)
	{
		queue.cmd_bind_index_buffer(m_buffer.get_buffer(), get_index_offset_of_node(m_draw_nodes[i]));
		queue.cmd_bind_vertex_buffer(m_buffer.get_buffer(), get_vertex_offset_of_node(m_draw_nodes[i]));
		queue.cmd_draw_indexed_indirect(m_buffer.get_buffer(), get_offset_of_node(m_draw_nodes[i]));
	}

	// Render newly generated terrain
	for (uint32_t i = 0; i < m_num_generate_nodes; i++)
	{
		queue.cmd_bind_index_buffer(m_buffer.get_buffer(), get_index_offset_of_node(m_generate_nodes[i].index));
		queue.cmd_bind_vertex_buffer(m_buffer.get_buffer(), get_vertex_offset_of_node(m_generate_nodes[i].index));
		queue.cmd_draw_indexed_indirect(m_buffer.get_buffer(), get_offset_of_node(m_generate_nodes[i].index));
	}

	queue.cmd_end_render_pass();

	// If drawing to screen as well, do the same thing again on swapchain framebuffer
	if (draw_to_screen)
	{
		// Draw error metric image
		m_em_push_data.screen_size = glm::vec2(framebuffer.get_width(), framebuffer.get_width());

		queue.cmd_push_constants(m_em_pipeline_layout.get_pipeline_layout(), VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(ErrorMetricData), &m_em_push_data);

		if (!wireframe)
			queue.cmd_bind_graphics_pipeline(m_em_wireframe_pipeline->m_pipeline);
		else
			queue.cmd_bind_graphics_pipeline(m_em_pipeline->m_pipeline);

		queue.cmd_begin_render_pass(m_em_render_pass, framebuffer);

		// Render nonupdated terrain
		for (uint32_t i = 0; i < m_num_draw_nodes; i++)
		{
			queue.cmd_bind_index_buffer(m_buffer.get_buffer(), get_index_offset_of_node(m_draw_nodes[i]));
		queue.cmd_bind_vertex_buffer(m_buffer.get_buffer(), get_vertex_offset_of_node(m_draw_nodes[i]));
		queue.cmd_draw_indexed_indirect(m_buffer.get_buffer(), get_offset_of_node(m_draw_nodes[i]));
		}

		// Render newly generated terrain
		for (uint32_t i = 0; i < m_num_generate_nodes; i++)
		{
			queue.cmd_bind_index_buffer(m_buffer.get_buffer(), get_index_offset_of_node(m_generate_nodes[i].index));
			queue.cmd_bind_vertex_buffer(m_buffer.get_buffer(), get_vertex_offset_of_node(m_generate_nodes[i].index));
			queue.cmd_draw_indexed_indirect(m_buffer.get_buffer(), get_offset_of_node(m_generate_nodes[i].index));
		}

		queue.cmd_end_render_pass();
	}
}

void Quadtree::clear_terrain()
{
	memset(m_node_index_to_buffer_index, INVALID, (1 << m_levels) * (1 << m_levels) * sizeof(uint32_t));
	memset(m_buffer_index_filled, 0, m_max_nodes * sizeof(bool));
}

void Quadtree::create_pipelines(Window& window)
{
	VertexAttributes va(*m_context);
	va.add_buffer();
	va.add_attribute(4);
	m_draw_pipeline = m_context->create_graphics_pipeline(
		"terrain_draw",
		window.get_size(),
		m_draw_pipeline_layout,
		va,
		m_render_pass,
		true,
		false,
		nullptr,
		nullptr);

	m_draw_wireframe_pipeline = m_context->create_graphics_pipeline(
		"terrain_draw",
		window.get_size(),
		m_draw_pipeline_layout,
		va,
		m_render_pass,
		true,
		false,
		nullptr,
		nullptr,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_POLYGON_MODE_LINE);

	m_generation_pipeline = m_context->create_compute_pipeline("terrain_generate", m_generation_pipeline_layout, nullptr);

	m_triangulation_pipeline = m_context->create_compute_pipeline("triangulate", m_triangulation_pipeline_layout, nullptr);

	m_border_handling_pipeline = m_context->create_compute_pipeline("border_handling", m_triangulation_pipeline_layout, nullptr);

	m_triangle_processing_compute_pipeline = m_context->create_compute_pipeline("triangle_processing", m_triangle_processing_pipeline_layout, nullptr);
}

void Quadtree::triangulate(GraphicsQueue& queue)
{
	queue.cmd_bind_compute_pipeline(m_triangulation_pipeline->m_pipeline);
	queue.cmd_bind_descriptor_set_compute(m_triangulation_pipeline_layout.get_pipeline_layout(), 0, m_descriptor_set.get_descriptor_set());

	for (unsigned i = 0; i < m_num_draw_nodes; ++i)
	{
		m_triangulation_push_data.node_index = m_draw_nodes[i];
		queue.cmd_push_constants(
				m_triangulation_pipeline_layout.get_pipeline_layout(), 
				VK_SHADER_STAGE_COMPUTE_BIT, 
				sizeof(TriangulationData), 
				&m_triangulation_push_data);
		queue.cmd_dispatch(1, 1, 1);
	}

	// Memory barrier for GPU buffer
	queue.cmd_buffer_barrier(get_buffer().get_buffer(),
		VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
		VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
}

void Quadtree::handle_borders(GraphicsQueue& queue)
{
	queue.cmd_bind_compute_pipeline(m_border_handling_pipeline->m_pipeline);
	queue.cmd_bind_descriptor_set_compute(m_triangulation_pipeline_layout.get_pipeline_layout(), 0, m_descriptor_set.get_descriptor_set());

	for (unsigned i = 0; i < m_num_draw_nodes; ++i)
	{
		m_triangulation_push_data.node_index = m_draw_nodes[i];
		queue.cmd_push_constants(
			m_triangulation_pipeline_layout.get_pipeline_layout(),
			VK_SHADER_STAGE_COMPUTE_BIT,
			sizeof(TriangulationData),
			&m_triangulation_push_data);
		queue.cmd_dispatch(1, 1, 1);
	}

	//// Memory barrier for GPU buffer
	//queue.cmd_buffer_barrier(get_buffer().get_buffer(),
	//	VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
	//	VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
	//	VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
	//	VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
}

PipelineLayout& Quadtree::get_triangle_processing_layout()
{
	return m_triangle_processing_pipeline_layout;
}

ImageView& Quadtree::get_em_image_view()
{
	return m_em_image_view;
}

GPUBuffer& Quadtree::get_buffer()
{
	return m_buffer;
}

GPUImage& Quadtree::get_em_image()
{
	return m_em_image;
}

GPUImage& Quadtree::get_em_depth_image()
{
	return m_em_depth_image;
}

RenderPass& Quadtree::get_render_pass()
{
	return m_render_pass;
}

void Quadtree::move_from(Quadtree&& other)
{
	destroy();

	m_context = other.m_context;

	m_memory = std::move(other.m_memory);
	m_buffer = std::move(other.m_buffer);
	m_cpu_index_buffer = std::move(other.m_cpu_index_buffer);
	m_cpu_index_buffer_size = other.m_cpu_index_buffer_size;
	m_cpu_index_buffer_memory = std::move(other.m_cpu_index_buffer_memory);
	m_quadtree_minmax = other.m_quadtree_minmax;
	other.m_quadtree_minmax = nullptr;

	m_generation_set_layout = std::move(other.m_generation_set_layout);
	m_descriptor_set = std::move(other.m_descriptor_set);
	m_generation_pipeline_layout = std::move(other.m_generation_pipeline_layout);
	m_generation_pipeline = std::move(other.m_generation_pipeline);

	m_render_pass = std::move(other.m_render_pass);

	m_max_indices = other.m_max_indices;
	m_max_vertices = other.m_max_vertices;
	m_max_node_new_points = other.m_max_node_new_points;

	m_max_nodes = other.m_max_nodes;

	m_total_side_length = other.m_total_side_length;
	m_levels = other.m_levels;

	m_node_index_to_buffer_index = other.m_node_index_to_buffer_index;
	other.m_node_index_to_buffer_index = nullptr;

	m_buffer_index_filled = other.m_buffer_index_filled;
	other.m_buffer_index_filled = nullptr;

	m_em_memory = std::move(other.m_em_memory);
	m_em_image = std::move(other.m_em_image);
	m_em_image_view = std::move(other.m_em_image_view);
	m_em_depth_image = std::move(other.m_em_depth_image);
	m_em_depth_image_view = std::move(other.m_em_depth_image_view);
	m_em_framebuffer = std::move(other.m_em_framebuffer);
	m_em_pipeline_layout = std::move(other.m_em_pipeline_layout);
	m_em_render_pass = std::move(other.m_em_render_pass);
	m_em_pipeline = std::move(other.m_em_pipeline);
	m_em_wireframe_pipeline = std::move(other.m_em_wireframe_pipeline);

	m_triangulation_pipeline_layout = std::move(other.m_triangulation_pipeline_layout);
	m_triangulation_pipeline = std::move(other.m_triangulation_pipeline);

	m_border_handling_pipeline = std::move(other.m_border_handling_pipeline);

	m_draw_pipeline_layout = std::move(other.m_draw_pipeline_layout);
	m_draw_pipeline = std::move(other.m_draw_pipeline);

	m_num_generate_nodes = other.m_num_generate_nodes;
	m_generate_nodes = other.m_generate_nodes;
	other.m_generate_nodes = nullptr;

	m_num_draw_nodes = other.m_num_draw_nodes;

	m_draw_nodes = other.m_draw_nodes;
	other.m_draw_nodes = nullptr;

	m_node_memory_size = other.m_node_memory_size;

	m_triangle_processing_layout = std::move(other.m_triangle_processing_layout);
	m_triangle_processing_set = std::move(other.m_triangle_processing_set);
	m_triangle_processing_pipeline_layout = std::move(other.m_triangle_processing_pipeline_layout);
	m_triangle_processing_compute_pipeline = std::move(other.m_triangle_processing_compute_pipeline);
}

void Quadtree::destroy()
{
	if (m_node_index_to_buffer_index != nullptr)
	{
		vkUnmapMemory(m_context->get_device(), m_cpu_index_buffer.get_memory());
		m_node_index_to_buffer_index = nullptr;
		m_quadtree_minmax = nullptr;
	}

	delete[] m_buffer_index_filled;

	delete[] m_generate_nodes;
	delete[] m_draw_nodes;
}

void Quadtree::intersect(Frustum& frustum, DebugDrawer& dd, AabbXZ aabb, uint32_t level, uint32_t x, uint32_t y)
{
	if (level == m_levels)
	{
		//float minx = aabb.m_min.x + (aabb.m_max.x - aabb.m_min.x) * 0.05f;
		//float maxx = aabb.m_min.x + (aabb.m_max.x - aabb.m_min.x) * 0.95f;

		//float minz = aabb.m_min.y + (aabb.m_max.y - aabb.m_min.y) * 0.05f;
		//float maxz = aabb.m_min.y + (aabb.m_max.y - aabb.m_min.y) * 0.95f;

		//dd.draw_line({ minx, 0, minz }, { minx, 0, maxz }, { 1, 1, 0 });
		//dd.draw_line({ minx, 0, minz }, { maxx, 0, minz }, { 1, 1, 0 });

		//dd.draw_line({ maxx, 0, maxz }, { maxx, 0, minz }, { 1, 1, 0 });
		//dd.draw_line({ maxx, 0, maxz }, { minx, 0, maxz }, { 1, 1, 0 });

		// Index into m_node_index_to_buffer_index
		uint32_t index = (1 << m_levels) * y + x;
		if (m_node_index_to_buffer_index[index] == INVALID)
		{
			// Visible node does not have data

			uint32_t new_index = find_chunk();
			if (new_index != INVALID)
			{
				m_buffer_index_filled[new_index] = true;
				m_node_index_to_buffer_index[index] = new_index;

				// m_buffer[new_index] needs to be filled with data
				m_generate_nodes[m_num_generate_nodes].index = new_index;
				m_generate_nodes[m_num_generate_nodes].min = aabb.m_min;
				m_generate_nodes[m_num_generate_nodes].max = aabb.m_max;
				m_num_generate_nodes++;
			}
			else
			{
				// No space left! Ignore for now...
			}
		}
		else
		{
			// Visible node has data, draw it

			// m_buffer[m_node_index_to_buffer_index[index]] needs to be drawn
			m_draw_nodes[m_num_draw_nodes] = m_node_index_to_buffer_index[index];
			m_num_draw_nodes++;
		}

		return;
	}

	// This node is visible, check children
	if (frustum_aabbxz_intersection(frustum, aabb))
	{
		glm::vec2 mid = (aabb.m_min + aabb.m_max) * 0.5f;
		float mid_x = (aabb.m_min.x + aabb.m_max.x) * 0.5f;
		float mid_z = (aabb.m_min.y + aabb.m_max.y) * 0.5f;

		intersect(frustum, dd, { {aabb.m_min.x, aabb.m_min.y}, {mid.x, mid.y} }, level + 1, (x << 1)    , (y << 1)    );
		intersect(frustum, dd, { {aabb.m_min.x, mid_z}, {mid.x, aabb.m_max.y} }, level + 1, (x << 1)    , (y << 1) + 1);
		intersect(frustum, dd, { {mid_x, aabb.m_min.y}, {aabb.m_max.x, mid_z} }, level + 1, (x << 1) + 1, (y << 1)    );
		intersect(frustum, dd, { {mid.x, mid.y}, {aabb.m_max.x, aabb.m_max.y} }, level + 1, (x << 1) + 1, (y << 1) + 1);
	}
}

uint32_t Quadtree::find_chunk()
{
	for (uint32_t i = 0; i < m_max_nodes; i++)
	{
		if (!m_buffer_index_filled[i])
			return i;
	}

	return INVALID;
}

uint32_t Quadtree::get_offset(uint32_t node_x, uint32_t node_z)
{
	assert(node_x >= 0u && node_x < (1u << m_levels));
	assert(node_z >= 0u && node_z < (1u << m_levels));

	return m_node_index_to_buffer_index[node_x + (1u << m_levels) * node_z];
}

void Quadtree::error_metric_setup(Window& window, GraphicsQueue& queue)
{
	m_em_memory = m_context->allocate_device_memory(window.get_size().x * window.get_size().y * 8 + 1000000);
	m_em_image = GPUImage(*m_context, VkExtent3D{ window.get_size().x , window.get_size().y, 1 }, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT, m_em_memory);
	m_em_depth_image = GPUImage(*m_context, VkExtent3D{ window.get_size().x , window.get_size().y, 1 }, VK_FORMAT_D32_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, m_em_memory);
	m_em_image_view = ImageView(*m_context, m_em_image, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
	m_em_depth_image_view = ImageView(*m_context, m_em_depth_image, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);
	m_em_framebuffer = Framebuffer(*m_context);
	m_em_framebuffer.add_attachment(m_em_image_view);
	m_em_framebuffer.add_attachment(m_em_depth_image_view);

	m_em_render_pass = RenderPass(*m_context, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true, true, true, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	m_em_framebuffer.create(m_em_render_pass.get_render_pass(), window.get_size().x, window.get_size().y);

	m_em_pipeline_layout = PipelineLayout(*m_context);

	VkPushConstantRange push;
	push.offset = 0;
	push.size = sizeof(ErrorMetricData);
	push.stageFlags = VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	m_em_pipeline_layout.create(&push);

	// Transfer image layouts
	queue.start_recording();

	queue.cmd_image_barrier(m_em_image.get_image(),
		0,
		0,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

	queue.cmd_image_barrier(m_em_depth_image.get_image(),
		0,
		0,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

	queue.end_recording();
	queue.submit();
	queue.wait();
}

VkDeviceSize Quadtree::get_index_offset_of_node(uint32_t i)
{
	return get_offset_of_node(i) + sizeof(VkDrawIndexedIndirectCommand) + sizeof(BufferNodeHeader);
}

VkDeviceSize Quadtree::get_vertex_offset_of_node(uint32_t i)
{
	return get_index_offset_of_node(i) + m_max_indices * sizeof(uint32_t);
}

VkDeviceSize Quadtree::get_offset_of_node(uint32_t i)
{
	return m_cpu_index_buffer_size + i * m_node_memory_size;
}

void Quadtree::derp()
{
	glm::vec2 tnode_min[4] = { {-500, -500}, {-500, 0}, {0, -500}, {0, 0} };
	glm::vec2 tnode_max[4] = { {0, 0}, {0, 500}, {500, 0}, {500, 500} };
	for (int i = 0; i < 4; ++i)
	{
		const glm::vec2 node_min = tnode_min[i];
		const glm::vec2 node_max = tnode_max[i];
		const float side = node_max.x - node_min.x;

		const int neighbur_indexing_x[4] = { 0, 1, 0, -1 };
		const int neighbur_indexing_y[4] = { 1, 0, -1, 0 };

		const int cx = int((node_min.x - (-500) + 1) / side);  // current node x
		const int cy = int((node_min.y - (-500) + 1) / side);  // current node z/y

		const int nodes_per_side = 2;

		float s_border_max[4] = { 0 };

		for (int bb = 0; bb < 4; ++bb)  // TODO: Go through corner neighbour nodes as well
		{
			int nx = cx + neighbur_indexing_x[bb];
			int ny = cy + neighbur_indexing_y[bb];

			bool valid_neighbour = ny >= 0 && ny < nodes_per_side && nx >= 0 && nx < nodes_per_side;
			int neighbour_index = 0;
			//s_border_max[bb] = 50;
			if (valid_neighbour)
			{
				neighbour_index = m_node_index_to_buffer_index[ny * nodes_per_side + nx];
				int neighbour_border = (bb + 2) % 4;
				s_border_max[bb] = 50;
				//s_border_max[bb] = terrain_buffer.data[neighbour_index].border_max[neighbour_border];
			}
		}
		int a = 0;
	}
}
