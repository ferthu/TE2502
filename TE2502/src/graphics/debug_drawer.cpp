#include "debug_drawer.hpp"

DebugDrawer::DebugDrawer(VulkanContext& context, uint32_t max_lines) : m_context(&context), m_max_lines(max_lines), m_current_lines(0)
{
	const VkDeviceSize extra_memory = 500;

	m_gpu_memory = context.allocate_device_memory(max_lines * sizeof(DebugLine) + extra_memory);
	m_cpu_memory = context.allocate_host_memory(max_lines * sizeof(DebugLine) + extra_memory);

	m_gpu_buffer = GPUBuffer(context, max_lines * sizeof(DebugLine), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, m_gpu_memory);
	m_cpu_buffer = GPUBuffer(context, max_lines * sizeof(DebugLine), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, m_cpu_memory);

	vkMapMemory(context.get_device(), m_cpu_buffer.get_memory(), m_cpu_buffer.get_offset(), m_cpu_buffer.get_size(), 0, (void**) &m_mapped_memory);
}
DebugDrawer::~DebugDrawer()
{
	destroy();
}

DebugDrawer::DebugDrawer(DebugDrawer&& other)
{
	move_from(std::move(other));
}

DebugDrawer& DebugDrawer::operator=(DebugDrawer && other)
{
	if (this != &other)
		move_from(std::move(other));

	return *this;
}

GPUBuffer& DebugDrawer::get_cpu_buffer()
{
	return m_cpu_buffer;
}
GPUBuffer& DebugDrawer::get_gpu_buffer()
{
	return m_gpu_buffer;
}

void DebugDrawer::draw_line(glm::vec3 from, glm::vec3 to, glm::vec3 color)
{
	assert(m_current_lines < m_max_lines);

	m_mapped_memory[m_current_lines].from = from;
	m_mapped_memory[m_current_lines].to = to;
	m_mapped_memory[m_current_lines].start_color = color;
	m_mapped_memory[m_current_lines].end_color = color;

	m_current_lines++;
}
void DebugDrawer::draw_line(glm::vec3 from, glm::vec3 to, glm::vec3 start_color, glm::vec3 end_color)
{
	assert(m_current_lines < m_max_lines);

	m_mapped_memory[m_current_lines].from = from;
	m_mapped_memory[m_current_lines].to = to;
	m_mapped_memory[m_current_lines].start_color = start_color;
	m_mapped_memory[m_current_lines].end_color = end_color;

	m_current_lines++;
}

void DebugDrawer::new_frame()
{
	m_current_lines = 0;
}


VkDeviceSize DebugDrawer::get_active_buffer_size()
{
	return m_current_lines * sizeof(DebugLine);
}

void DebugDrawer::move_from(DebugDrawer&& other)
{
	destroy();

	m_context = other.m_context;
	m_mapped_memory = other.m_mapped_memory;
	other.m_mapped_memory = nullptr;
	m_gpu_memory = std::move(other.m_gpu_memory);
	m_cpu_memory = std::move(other.m_cpu_memory);
	m_gpu_buffer = std::move(other.m_gpu_buffer);
	m_cpu_buffer = std::move(other.m_cpu_buffer);

	m_max_lines = other.m_max_lines;
	m_current_lines = other.m_current_lines;
	other.m_current_lines = 0;
}

void DebugDrawer::destroy()
{
	if (m_mapped_memory != nullptr)
	{
		vkUnmapMemory(m_context->get_device(), m_cpu_buffer.get_memory());
		m_mapped_memory = nullptr;
	}
}