#pragma once

#include <glm/glm.hpp>

#include "vulkan_context.hpp"
#include "gpu_memory.hpp"
#include "gpu_buffer.hpp"
#include "math/geometry.hpp"

// Class handling debug drawing
class DebugDrawer
{
public:
	DebugDrawer() {}
	DebugDrawer(VulkanContext& context, uint32_t max_lines);
	~DebugDrawer();
	DebugDrawer(DebugDrawer&& other);
	DebugDrawer& operator=(DebugDrawer&& other);

	GPUBuffer& get_cpu_buffer();
	GPUBuffer& get_gpu_buffer();


	// Draws a line
	void draw_line(glm::vec3 from, glm::vec3 to, glm::vec3 color);

	// Draws a line
	void draw_line(glm::vec3 from, glm::vec3 to, glm::vec3 start_color, glm::vec3 end_color);

	// Draws a line (performs perspective divide)
	void draw_line(glm::vec4 from, glm::vec4 to, glm::vec3 color);

	// Draws a plane
	void draw_plane(Plane& plane, glm::vec3 position, float size, glm::vec3 plane_color, glm::vec3 normal_color);

	// Draws frustum using VP matrix
	void draw_frustum(glm::mat4 vp_matrix, glm::vec3 color);

	// Clear line buffer to prepare for new frame
	void new_frame();

	uint32_t get_num_lines() { return m_current_lines; }

	// Returns the size of the populated region of CPU buffer
	VkDeviceSize get_active_buffer_size();

private:
	// Move other into this
	void move_from(DebugDrawer&& other);

	// Destroys object
	void destroy();

	VulkanContext* m_context;

	struct DebugLine
	{
		glm::vec3 from; 
		glm::vec3 start_color; 
		glm::vec3 to; 
		glm::vec3 end_color;
	};

	DebugLine* m_mapped_memory = nullptr;

	GPUMemory m_gpu_memory;
	GPUMemory m_cpu_memory;

	GPUBuffer m_gpu_buffer;
	GPUBuffer m_cpu_buffer;

	uint32_t m_max_lines = 0;
	uint32_t m_current_lines = 0;
};

