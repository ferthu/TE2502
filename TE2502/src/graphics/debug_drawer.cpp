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

void DebugDrawer::draw_plane(Plane& plane, glm::vec3 position, float size, glm::vec3 plane_color, glm::vec3 normal_color)
{
	// A vector parallell to the plane
	glm::vec3 plane_tangent;

	if (glm::dot(plane.m_normal, glm::vec3(0, 1, 0)) < 0.1f)
	{
		plane_tangent = glm::normalize(glm::cross(plane.m_normal, glm::vec3(0, 1, 0)));
	}
	else
	{
		plane_tangent = glm::normalize(glm::cross(plane.m_normal, glm::vec3(0, 0, 1)));
	}

	glm::vec3 plane_bitangent = glm::cross(plane.m_normal, plane_tangent);

	// Draw plane normal
	draw_line(position, position + plane.m_normal, normal_color);

	int gridsize = 5;
	for (int i = -gridsize; i <= gridsize; i++)
	{
		draw_line(position + plane_tangent * float(i) * size + plane_bitangent * float(gridsize) * size,
				  position + plane_tangent * float(i) * size - plane_bitangent * float(gridsize) * size,
				  plane_color);

		draw_line(position + plane_bitangent * float(i) * size + plane_tangent * float(gridsize) * size,
				  position + plane_bitangent * float(i) * size - plane_tangent * float(gridsize) * size,
				  plane_color);
	}
}

void DebugDrawer::draw_frustum(glm::mat4 vp_matrix, glm::vec3 color)
{
	glm::mat4 inv_vp = glm::inverse(vp_matrix);

	// Near plane
	draw_line(inv_vp * glm::vec4(-1, -1, 0, 1), inv_vp * glm::vec4(1, -1, 0, 1), color);
	draw_line(inv_vp * glm::vec4(-1, 1, 0, 1), inv_vp * glm::vec4(1, 1, 0, 1), color);
	draw_line(inv_vp * glm::vec4(1, -1, 0, 1), inv_vp * glm::vec4(1, 1, 0, 1), color);
	draw_line(inv_vp * glm::vec4(-1, -1, 0, 1), inv_vp * glm::vec4(-1, 1, 0, 1), color);

	// Far plane
	draw_line(inv_vp * glm::vec4(-1, -1, 1, 1), inv_vp * glm::vec4(1, -1, 1, 1), color);
	draw_line(inv_vp * glm::vec4(-1, 1, 1, 1), inv_vp * glm::vec4(1, 1, 1, 1), color);
	draw_line(inv_vp * glm::vec4(1, -1, 1, 1), inv_vp * glm::vec4(1, 1, 1, 1), color);
	draw_line(inv_vp * glm::vec4(-1, -1, 1, 1), inv_vp * glm::vec4(-1, 1, 1, 1), color);

	// Connect near and far planes
	draw_line(inv_vp * glm::vec4(-1, -1, 0, 1), inv_vp * glm::vec4(-1, -1, 1, 1), color);
	draw_line(inv_vp * glm::vec4(-1, 1, 0, 1), inv_vp * glm::vec4(-1, 1, 1, 1), color);
	draw_line(inv_vp * glm::vec4(1, -1, 0, 1), inv_vp * glm::vec4(1, -1, 1, 1), color);
	draw_line(inv_vp * glm::vec4(1, 1, 0, 1), inv_vp * glm::vec4(1, 1, 1, 1), color);
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