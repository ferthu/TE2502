#pragma once

#include <glm/glm.hpp>

// Describes a 3D plane with a normal vector and a distance to origin
class Plane
{
public:
	Plane() {}
	Plane(glm::vec3 normal, float dist) : m_normal(glm::normalize(normal)), m_dist(dist) {}
	Plane(float x, float y, float z, float dist) : m_plane(x, y, z, dist) { m_normal = glm::normalize(m_normal); }

	// Returns signed distance between a point and the plane (positive if plane normal points to the halfspace the point is in)
	float point_dist(glm::vec3 point) { return glm::dot(point, m_normal) - m_dist; }

	// Normalizes plane normal and distance
	void normalize();

	union
	{
		// Normal and dist packed into vec4
		glm::vec4 m_plane;

		struct 
		{
			// Plane normal
			glm::vec3 m_normal;

			// Distance from origin along plane normal
			float m_dist;
		};
	};
};

class Frustum
{
public:
	Frustum() {}

	union
	{
		Plane m_planes[6];
		struct
		{
			Plane m_left;
			Plane m_right;
			Plane m_top;
			Plane m_bottom;
			Plane m_near;
			Plane m_far;
		};
	};
};
