#include "camera.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <GLFW/glfw3.h>
#include <iostream>
#include "imgui/imgui.h"

Camera::Camera(GLFWwindow* window)
{
	m_window = window;
	glfwGetWindowSize(m_window, &m_window_width, &m_window_height);
	m_perspective = glm::perspective(90.0f, (float)m_window_width / m_window_height, m_near, m_far);
}

Camera::~Camera()
{
}

void Camera::update(const float dt, bool mouse_locked, DebugDrawer& dd)
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
		up -= 1.f;
	if (glfwGetKey(m_window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)  // Down
		up += 1.f;
	if (glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)  // Speed up
		speed_modifier = 1.f;

	const float speed = (m_slow_speed * (1.f - speed_modifier) + m_fast_speed * speed_modifier) * dt;
	horiz_dir *= speed;
	up *= speed;

	if (mouse_locked)
	{
		// Handle mouse input
		double x_pos, y_pos;
		glfwGetCursorPos(m_window, &x_pos, &y_pos);
		const float dx = (m_window_width / 2 - (float)x_pos) * m_mouse_sensitivity;
		const float dy = (m_window_height / 2 - (float)y_pos) * m_mouse_sensitivity;
		glfwSetCursorPos(m_window, m_window_width / 2, m_window_height / 2);
		m_yaw += dx;
		m_pitch += dy;

		if (m_yaw > glm::pi<float>() * 2.0f)
			m_yaw -= glm::pi<float>() * 2.0f;
		if (m_yaw < -glm::pi<float>() * 2.0f)
			m_yaw += glm::pi<float>() * 2.0f;

		if (m_pitch > glm::pi<float>() * 0.5f - 0.01f)
			m_pitch = glm::pi<float>() * 0.5f - 0.01f;
		if (m_pitch < -glm::pi<float>() * 0.5f + 0.01f)
			m_pitch = -glm::pi<float>() * 0.5f + 0.01f;
	}

	// Update

	// These need to be different because reasons
	glm::mat4 camera_rotation = glm::rotate(glm::rotate(glm::mat4(1.0f), m_pitch, { 1, 0, 0 }), -m_yaw, { 0, 1, 0 });
	glm::mat4 direction_rotation = glm::rotate(glm::rotate(glm::mat4(1.0f), m_yaw, { 0, 1, 0 }), -m_pitch, { 1, 0, 0 });

	const glm::vec3 forward_dir = direction_rotation * glm::vec4(0, 0, -1, 0);
	const glm::vec3 left_dir = direction_rotation * glm::vec4(-1, 0, 0, 0);

	m_position += forward_dir * horiz_dir.y + left_dir * horiz_dir.x + glm::vec3(0, up, 0);

	m_view = glm::translate(camera_rotation, glm::vec3{ -m_position });

	static float fov = 90.0f;
	ImGui::Begin("Camera Settings");
	ImGui::DragFloat("fov", &fov, 1.0f, 10.0f, 120.0f);
	ImGui::End();

	m_perspective = glm::perspective(glm::radians(fov), (float)m_window_width / m_window_height, m_near, m_far);


	m_vp = m_perspective * m_view;
}

const glm::vec3& Camera::get_pos() const
{
	return m_position;
}

const glm::mat4& Camera::get_view() const
{
	return m_view;
}

const glm::mat4& Camera::get_vp() const
{
	return m_vp;
}

const glm::mat4& Camera::get_perspective() const
{
	return m_perspective;
}

void Camera::set_pos(const glm::vec3 & new_pos)
{
	m_position = new_pos;
}
