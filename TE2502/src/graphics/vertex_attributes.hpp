#pragma once

#include <vector>

#include "graphics/vulkan_context.hpp"

// Object describing vertex attribute layouts for use when creating a graphics pipeline
// The pipeline contains a number of vertex buffers. Every vertex buffer contains one or more vertex attributes
// Usage: add slot for a vertex buffer with add_buffer(). Add attributes to the last added buffer with add_attribute()
class VertexAttributes
{
public:
	VertexAttributes() {}
	VertexAttributes(VulkanContext& context);
	~VertexAttributes();

	VertexAttributes(VertexAttributes&& other);
	VertexAttributes& operator=(VertexAttributes&& other);

	// Adds new vertex buffer description
	void add_buffer();

	// Adds an attribute to the last added buffer
	// num_floats is the number of floats in the attribute (3 for vec3, etc.)
	void add_attribute(uint32_t num_floats);

	uint32_t get_num_bindings();
	VkVertexInputBindingDescription* get_bindings();

	uint32_t get_num_attributes();
	VkVertexInputAttributeDescription* get_attributes();

private:
	// Move other into this
	void move_from(VertexAttributes&& other);

	// Destroys object
	void destroy();

	VulkanContext* m_context;

	std::vector<VkVertexInputBindingDescription> m_bindings;
	std::vector<VkVertexInputAttributeDescription> m_attributes;

	// Offset into current vertex attributes
	uint32_t m_current_offset = 0;

	// Location of next attribute
	uint32_t m_next_location = 0;
};

