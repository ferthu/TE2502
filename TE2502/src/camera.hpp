#pragma once
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include "graphics/debug_drawer.hpp"
#include "math/geometry.hpp"
#include "terrain_interface.h"

// Camera with movement
class Camera
{
public:
	Camera(GLFWwindow* window);
	virtual ~Camera();

	// Checks for input and updates
	// Does not rotate if mouse_locked is true
	void update(const float dt, bool mouse_locked, DebugDrawer& dd);

	// Get current camera position
	const glm::vec3& get_pos() const;
	// Get view matrix
	const glm::mat4& get_view() const;
	// Get the combined view-perspective matrix
	const glm::mat4& get_vp() const;
	// Get the combined view-big perspective matrix
	const glm::mat4& get_big_vp() const;
	// Get perspective matrix
	const glm::mat4& get_perspective() const;
	// Get big perspective matrix
	const glm::mat4& get_big_perspective() const;
	// Get ray march view matrix
	const glm::mat4& get_ray_march_view() const;
	// Get camera yaw (rotation)
	inline float get_yaw() const { return m_yaw; }
	// Get camera pitch (up/down)
	inline float get_pitch() const { return m_pitch; }

	inline float get_fov() const { return m_fov; }

	// Set new camera position
	// Is preferrably called before camera.update()
	void set_pos(const glm::vec3& new_pos);

	// Set yaw and pitch
	void set_yaw_pitch(float yaw, float pitch);

	Frustum get_frustum() const;

	static glm::mat4 calculate_perspective(float horiz_fov_degrees, float near, float far, float window_width, float window_height);

private:
	// Fills m_frustum with data on current frustum planes
	void get_camera_planes();

	const float m_slow_speed = 4.f;
	const float m_fast_speed = 300.f;
	const float m_fov = 90.f;	// Horizontal FOV in degrees
	const float m_near = 0.05f;
	const float m_far = max_view_dist;
	const float m_mouse_sensitivity = 0.01f;
	const float m_fov_multiplier = 1.2f;	// This value is multiplied by m_fov to get the big frustum FOV

	GLFWwindow* m_window;
	int m_window_width;
	int m_window_height;
	glm::vec3 m_position = glm::vec3(0, 50, 0);
	float m_yaw = 0.f;  // Radians
	float m_pitch = 0.f;  // Radians
	glm::mat4 m_view;
	glm::mat4 m_perspective;
	glm::mat4 m_big_perspective;
	glm::mat4 m_vp;
	glm::mat4 m_big_vp;
	glm::mat4 m_ray_march_view;

	Frustum m_frustum;
};
