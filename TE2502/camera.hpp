#pragma once
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

// Camera with movement
class Camera
{
public:
	Camera(GLFWwindow* window);
	virtual ~Camera();

	// Checks for input and updates
	void update(const float dt);

	// Get current camera position
	const glm::vec3& get_pos() const;
	// Get view matrix
	const glm::mat4& get_view() const;
	// Get the combined view-perspective matrix
	const glm::mat4& get_vp() const;
	// Get perspective matrix
	const glm::mat4& get_perspective() const;

	// Set new camera position
	// Is preferrably called before camera.update()
	void set_pos(const glm::vec3& new_pos);

private:
	const float m_slow_speed = 3.f;
	const float m_fast_speed = 30.f;
	const float m_fov = 90.f;
	const float m_near = 0.1f;
	const float m_far = 1000.f;
	const float m_mouse_sensitivity = 0.02f;

	GLFWwindow* m_window;
	int m_window_width;
	int m_window_height;
	glm::vec3 m_position = glm::vec3(0);
	float m_yaw = 0.f;  // Radians
	float m_pitch = 0.f;  // Radians
	glm::mat4 m_view;
	glm::mat4 m_perspective;
	glm::mat4 m_vp;
};