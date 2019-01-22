#include "camera.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

Camera::Camera(GLFWwindow* window)
{
	m_window = window;
	glfwGetWindowSize(m_window, &m_window_width, &m_window_height);
	m_perspective = glm::perspective(m_fov * 0.56f, (float)m_window_width / m_window_height, m_near, m_far);
	glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

Camera::~Camera()
{
}

void Camera::update(const float dt)
{
	// Handle keyboard input
	glm::vec2 horiz_dir = glm::vec2(0);  // x = left, y = forward
	float up = 0.f;
	float speed_modifier = 0.f;
	if (glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS)  // Forward
		horiz_dir.y += 1.f;
	if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS)  // Backward
		horiz_dir.y -= 1.f;
	if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS)  // Left
		horiz_dir.x += 1.f;
	if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS)  // Right
		horiz_dir.x -= 1.f;
	if (glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS)  // Up
		up += 1.f;
	if (glfwGetKey(m_window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)  // Down
		up -= 1.f;
	if (glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)  // Down
		speed_modifier = 1.f;

	horiz_dir = glm::normalize(horiz_dir);
	const float speed = (m_slow_speed * (1.f - speed_modifier) + m_fast_speed * speed_modifier) * dt;
	horiz_dir *= speed;
	up *= speed;


	// Handle mouse input
	double x_pos, y_pos;
	glfwGetCursorPos(m_window, &x_pos, &y_pos);
	const float dx = ((float)x_pos - m_window_width) * m_mouse_sensitivity;
	const float dy = ((float)y_pos - m_window_height) * m_mouse_sensitivity;
	m_yaw += dx;
	m_pitch += dy;

	// Update

	const glm::mat4 yaw = glm::rotate(glm::mat4(1), m_yaw, glm::vec3(0, 1, 0));
	const glm::mat4 pitch = glm::rotate(glm::mat4(1), m_pitch, glm::vec3(1, 0, 0));

	const glm::vec3 forward_dir = glm::vec3(yaw * pitch * glm::vec4(0, 1, 0, 0));
	const glm::vec3 left_dir = glm::vec3(yaw * pitch * glm::vec4(1, 0, 0, 0));

	m_position += forward_dir * horiz_dir.y + left_dir * horiz_dir.x + up;

	m_view = glm::lookAt(m_position, m_position + forward_dir, glm::vec3(0, 1, 0));
	m_vp = m_perspective * m_view;
}

const glm::vec3 & Camera::get_pos() const
{
	return m_position;
}

const glm::mat4 & Camera::get_view() const
{
	return m_view;
}

const glm::mat4 & Camera::get_vp() const
{
	return m_vp;
}

const glm::mat4 & Camera::get_perspective() const
{
	return m_perspective;
}

void Camera::set_pos(const glm::vec3 & new_pos)
{
	m_position = new_pos;
}
