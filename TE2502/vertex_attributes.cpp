#include "vertex_attributes.hpp"


VertexAttributes::VertexAttributes(VulkanContext& context) : m_context(&context)
{
}

VertexAttributes::~VertexAttributes()
{
}

VertexAttributes::VertexAttributes(VertexAttributes&& other)
{
	move_from(std::move(other));
}

VertexAttributes& VertexAttributes::operator=(VertexAttributes&& other)
{
	if (this != &other)
		move_from(std::move(other));

	return *this;
}

void VertexAttributes::add_buffer()
{
	VkVertexInputBindingDescription buffer;
	buffer.binding = static_cast<uint32_t>(m_bindings.size());
	buffer.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	buffer.stride = 0;
	m_bindings.push_back(buffer);

	m_current_offset = 0;
}

void VertexAttributes::add_attribute(uint32_t num_floats)
{
	assert(m_bindings.size() > 0);

	VkFormat format;
	switch (num_floats)
	{
	case 1:
		format = VK_FORMAT_R32_SFLOAT;
		break;
	case 2:
		format = VK_FORMAT_R32G32_SFLOAT;
		break;
	case 3:
		format = VK_FORMAT_R32G32B32_SFLOAT;
		break;
	case 4:
		format = VK_FORMAT_R32G32B32A32_SFLOAT;
		break;
	default:
		assert(false);
	}

	VkVertexInputAttributeDescription attribute;
	attribute.binding = static_cast<uint32_t>(m_bindings.size() - 1);
	attribute.location = m_next_location;
	m_next_location++;
	attribute.format = format;
	attribute.offset = m_current_offset;
	m_current_offset += num_floats * sizeof(float);
	m_bindings[m_bindings.size() - 1].stride = m_current_offset;

	m_attributes.push_back(attribute);
}

uint32_t VertexAttributes::get_num_bindings()
{
	return static_cast<uint32_t>(m_bindings.size());
}

VkVertexInputBindingDescription* VertexAttributes::get_bindings()
{
	return m_bindings.data();
}

uint32_t VertexAttributes::get_num_attributes()
{
	return static_cast<uint32_t>(m_attributes.size());
}

VkVertexInputAttributeDescription* VertexAttributes::get_attributes()
{
	return m_attributes.data();
}

void VertexAttributes::move_from(VertexAttributes&& other)
{
	destroy();

	m_context = other.m_context;

	m_bindings = std::move(other.m_bindings);
	m_attributes = std::move(other.m_attributes);
	m_current_offset = other.m_current_offset;
	other.m_current_offset = 0;
	m_next_location = other.m_next_location;
	other.m_next_location = 0;
}

void VertexAttributes::destroy()
{
}
