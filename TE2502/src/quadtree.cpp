#include "quadtree.hpp"

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

Quadtree::Quadtree(VulkanContext& context, float total_side_length, uint32_t levels, uint32_t max_nodes, uint32_t max_node_indices, uint32_t max_node_vertices, Window& window)
	: m_context(&context), m_total_side_length(total_side_length), m_levels(levels), m_max_nodes(max_nodes), m_max_indices(max_node_indices), m_max_vertices(max_node_vertices)
{
	assert(levels > 0);

	// (1 << levels) is number of nodes per axis
	m_node_index_to_buffer_index = new uint32_t[(1 << levels) * (1 << levels)];
	memset(m_node_index_to_buffer_index, INVALID, (1 << levels) * (1 << levels) * sizeof(uint32_t));

	m_buffer_index_filled = new bool[max_nodes];
	memset(m_buffer_index_filled, 0, max_nodes * sizeof(bool));

	m_num_generate_nodes = 0;
	m_generate_nodes = new GenerateInfo[max_nodes];

	m_num_draw_nodes = 0;
	m_draw_nodes = new uint32_t[max_nodes];

	m_terrain_queue = context.create_graphics_queue();

	m_node_memory_size = sizeof(VkDrawIndexedIndirectCommand) + max_node_indices * sizeof(uint32_t) + max_node_vertices * sizeof(glm::vec4);
	m_memory = context.allocate_device_memory(m_node_memory_size * max_nodes + 500);
	m_buffer = GPUBuffer(context, m_node_memory_size * max_nodes, 
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | 
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | 
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
		m_memory);

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

	m_generation_pipeline = context.create_compute_pipeline("terrain_generate", m_generation_pipeline_layout);

	m_render_pass = RenderPass(context, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, false, true, false, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	m_draw_pipeline_layout = PipelineLayout(context);
	m_draw_pipeline_layout.create(&push);
	VertexAttributes va(context);
	va.add_buffer();
	va.add_attribute(4);
	m_draw_pipeline = context.create_graphics_pipeline("terrain_draw", window.get_size(), m_draw_pipeline_layout, va, m_render_pass, true);
}

void Quadtree::draw_terrain(Frustum& frustum, DebugDrawer& dd, Framebuffer& framebuffer, Camera& camera)
{
	m_num_generate_nodes = 0;
	m_num_draw_nodes = 0;

	float half_length = m_total_side_length * 0.5f;

	// Gather status of nodes
	intersect(frustum, dd, { {-half_length, -half_length},
		{half_length, half_length} }, 0, 0, 0);

	// Begin recording
	m_terrain_queue.start_recording();

	m_push_data.vp = camera.get_vp();
	m_push_data.camera_pos = glm::vec4(camera.get_pos(), 1.0f);

	// Dispatch terrain generation
	m_terrain_queue.cmd_bind_compute_pipeline(m_generation_pipeline->m_pipeline);
	m_descriptor_set.clear();
	m_descriptor_set.add_storage_buffer(m_buffer);
	m_descriptor_set.bind();
	m_terrain_queue.cmd_bind_descriptor_set_compute(m_generation_pipeline_layout.get_pipeline_layout(), 0, m_descriptor_set.get_descriptor_set());

	for (uint32_t i = 0; i < m_num_generate_nodes; i++)
	{
		m_push_data.buffer_slot = m_generate_nodes[i].index;
		m_push_data.min = m_generate_nodes[i].min;
		m_push_data.max = m_generate_nodes[i].max;

		m_terrain_queue.cmd_push_constants(m_generation_pipeline_layout.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(GenerationData), &m_push_data);
		m_terrain_queue.cmd_dispatch(1, 1, 1);
	}

	// Memory barriers
	for (uint32_t i = 0; i < m_num_generate_nodes; i++)
	{
		m_terrain_queue.cmd_buffer_barrier(m_buffer.get_buffer(),
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
			m_generate_nodes[i].index * m_node_memory_size,
			m_node_memory_size);
	}

	// Start renderpass
	m_terrain_queue.cmd_bind_graphics_pipeline(m_draw_pipeline->m_pipeline);
	m_terrain_queue.cmd_begin_render_pass(m_render_pass, framebuffer);

	m_terrain_queue.cmd_push_constants(m_draw_pipeline_layout.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(GenerationData), &m_push_data);

	// Render nonupdated terrain
	for (uint32_t i = 0; i < m_num_draw_nodes; i++)
	{
		m_terrain_queue.cmd_bind_index_buffer(m_buffer.get_buffer(), m_draw_nodes[i] * m_node_memory_size + sizeof(VkDrawIndexedIndirectCommand));
		m_terrain_queue.cmd_bind_vertex_buffer(m_buffer.get_buffer(), m_draw_nodes[i] * m_node_memory_size + m_max_indices * sizeof(uint32_t) + sizeof(VkDrawIndexedIndirectCommand));
		m_terrain_queue.cmd_draw_indexed_indirect(m_buffer.get_buffer(), m_draw_nodes[i] * m_node_memory_size);
	}

	// Render newly generated terrain
	for (uint32_t i = 0; i < m_num_generate_nodes; i++)
	{
		m_terrain_queue.cmd_bind_index_buffer(m_buffer.get_buffer(), m_generate_nodes[i].index * m_node_memory_size + sizeof(VkDrawIndexedIndirectCommand));
		m_terrain_queue.cmd_bind_vertex_buffer(m_buffer.get_buffer(), m_generate_nodes[i].index * m_node_memory_size + m_max_indices * sizeof(uint32_t) + sizeof(VkDrawIndexedIndirectCommand));
		m_terrain_queue.cmd_draw_indexed_indirect(m_buffer.get_buffer(), m_generate_nodes[i].index * m_node_memory_size);
	}

	// End renderpass
	m_terrain_queue.cmd_end_render_pass();

	// End recording
	m_terrain_queue.end_recording();
	m_terrain_queue.submit();
	m_terrain_queue.wait();
}

void Quadtree::clear_terrain()
{
	memset(m_node_index_to_buffer_index, INVALID, (1 << m_levels) * (1 << m_levels) * sizeof(uint32_t));
	memset(m_buffer_index_filled, 0, m_max_nodes * sizeof(bool));
}

void Quadtree::move_from(Quadtree&& other)
{
	destroy();

	m_memory = std::move(other.m_memory);
	m_buffer = std::move(other.m_buffer);

	m_max_indices = other.m_max_indices;
	m_max_vertices = other.m_max_vertices;

	m_max_nodes = other.m_max_nodes;

	m_total_side_length = other.m_total_side_length;
	m_levels = other.m_levels;

	m_node_index_to_buffer_index = other.m_node_index_to_buffer_index;
	other.m_node_index_to_buffer_index = nullptr;

	m_buffer_index_filled = other.m_buffer_index_filled;
	other.m_buffer_index_filled = nullptr;

	m_context = other.m_context;

	m_render_pass = std::move(other.m_render_pass);
	m_generation_set_layout = std::move(other.m_generation_set_layout);
	m_descriptor_set = std::move(other.m_descriptor_set);
	m_generation_pipeline_layout = std::move(other.m_generation_pipeline_layout);
	m_generation_pipeline = std::move(other.m_generation_pipeline);

	m_draw_pipeline_layout = std::move(other.m_draw_pipeline_layout);
	m_draw_pipeline = std::move(other.m_draw_pipeline);

	m_terrain_queue = std::move(other.m_terrain_queue);

	m_num_generate_nodes = other.m_num_generate_nodes;
	m_num_draw_nodes = other.m_num_draw_nodes;

	m_generate_nodes = other.m_generate_nodes;
	other.m_generate_nodes = nullptr;

	m_draw_nodes = other.m_draw_nodes;
	other.m_draw_nodes = nullptr;

	m_node_memory_size = other.m_node_memory_size;
}

void Quadtree::destroy()
{
	delete[] m_node_index_to_buffer_index;
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
