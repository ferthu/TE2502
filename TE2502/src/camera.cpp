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
	m_perspective = calculate_perspective(90.0f, m_near, m_far, m_window_width, m_window_height);
	get_camera_planes();
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
		horiz_dir.y -= 1.f;
	if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS)  // Backward
		horiz_dir.y += 1.f;
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

	// These need to be inverse
	glm::mat4 camera_rotation = glm::rotate(glm::rotate(glm::mat4(1.0f), -m_pitch, { 1, 0, 0 }), m_yaw, { 0, 1, 0 });
	glm::mat4 direction_rotation = glm::rotate(glm::rotate(glm::mat4(1.0f), -m_yaw, { 0, 1, 0 }), m_pitch, { 1, 0, 0 });

	const glm::vec3 forward_dir = direction_rotation * glm::vec4(0, 0, -1, 0);
	const glm::vec3 left_dir = direction_rotation * glm::vec4(-1, 0, 0, 0);

	m_position += forward_dir * horiz_dir.y + left_dir * horiz_dir.x + glm::vec3(0, up, 0);

	m_view = glm::translate(camera_rotation, glm::vec3{ -m_position });

	static float fov = 90.0f;
	ImGui::Begin("Camera Settings");
	ImGui::DragFloat("fov", &fov, 1.0f, 10.0f, 120.0f);
	ImGui::End();

	m_perspective = calculate_perspective(fov, m_near, m_far, m_window_width, m_window_height);
	m_vp = m_perspective * m_view;

	m_ray_march_view = glm::inverse(m_view);

	get_camera_planes();
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

const glm::mat4& Camera::get_ray_march_view() const
{
	return m_ray_march_view;
}

void Camera::set_pos(const glm::vec3& new_pos)
{
	m_position = new_pos;
}

Frustum Camera::get_frustum() const
{
	return m_frustum;
}

glm::mat4 Camera::calculate_perspective(float horiz_fov_degrees, float n, float f, float window_width, float window_height)
{
	float fov = glm::radians(horiz_fov_degrees);
	float a = window_width / window_height;
	float vert_fov = 2.0f * atanf(tanf(fov*0.5f) / a);
	float c = 1.0f / tanf(vert_fov * 0.5f);

	// Note: every line here is a column, not a row
	glm::mat4 persp;
	persp[0] = {c / a, 0, 0, 0};
	persp[1] = {0, c, 0, 0};
	persp[2] = {0, 0, (f+n)/(f-n), 1};
	persp[3] = {0, 0, -(2*f*n)/(f-n), 0};

	return persp;
}

void Camera::get_camera_planes()
{
	// Left clipping plane
	m_frustum.m_left.m_plane.x = m_vp[0][3] + m_vp[0][0];
	m_frustum.m_left.m_plane.y = m_vp[1][3] + m_vp[1][0];
	m_frustum.m_left.m_plane.z = m_vp[2][3] + m_vp[2][0];
	m_frustum.m_left.m_plane.w = m_vp[3][3] + m_vp[3][0];
	m_frustum.m_left.normalize();

	// Right clipping plane
	m_frustum.m_right.m_plane.x = m_vp[0][3] - m_vp[0][0];
	m_frustum.m_right.m_plane.y = m_vp[1][3] - m_vp[1][0];
	m_frustum.m_right.m_plane.z = m_vp[2][3] - m_vp[2][0];
	m_frustum.m_right.m_plane.w = m_vp[3][3] - m_vp[3][0];
	m_frustum.m_right.normalize();

	// Top clipping plane
	m_frustum.m_top.m_plane.x = m_vp[0][3] + m_vp[0][1];
	m_frustum.m_top.m_plane.y = m_vp[1][3] + m_vp[1][1];
	m_frustum.m_top.m_plane.z = m_vp[2][3] + m_vp[2][1];
	m_frustum.m_top.m_plane.w = m_vp[3][3] + m_vp[3][1];
	m_frustum.m_top.normalize();
	
	// Bottom clipping plane
	m_frustum.m_bottom.m_plane.x = m_vp[0][3] - m_vp[0][1];
	m_frustum.m_bottom.m_plane.y = m_vp[1][3] - m_vp[1][1];
	m_frustum.m_bottom.m_plane.z = m_vp[2][3] - m_vp[2][1];
	m_frustum.m_bottom.m_plane.w = m_vp[3][3] - m_vp[3][1];
	m_frustum.m_bottom.normalize();

	// Near clipping plane
	m_frustum.m_near.m_plane.x = m_vp[0][3] + m_vp[0][2];
	m_frustum.m_near.m_plane.y = m_vp[1][3] + m_vp[1][2];
	m_frustum.m_near.m_plane.z = m_vp[2][3] + m_vp[2][2];
	m_frustum.m_near.m_plane.w = m_vp[3][3] + m_vp[3][2];
	m_frustum.m_near.normalize();

	// Far clipping plane
	m_frustum.m_far.m_plane.x = m_vp[0][3] - m_vp[0][2];
	m_frustum.m_far.m_plane.y = m_vp[1][3] - m_vp[1][2];
	m_frustum.m_far.m_plane.z = m_vp[2][3] - m_vp[2][2];
	m_frustum.m_far.m_plane.w = m_vp[3][3] - m_vp[3][2];
	m_frustum.m_far.normalize();

}
