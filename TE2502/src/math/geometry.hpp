#pragma once

#include <glm/glm.hpp>

// Describes a 3D plane with a normal vector and a distance to origin
class Plane
{
public:
	Plane();
	Plane(glm::vec3 normal, float dist);
	Plane(float x, float y, float z, float dist);
	~Plane();

	// Returns signed distance between a point and the plane (positive if plane normal points to the halfspace the point is in)
	float point_dist(glm::vec3 point);

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
	union 
	{
		Plane m_planes[6];
		struct 
		{
			Plane m_front;
			Plane m_back;
			Plane m_left;
			Plane m_right;
			Plane m_top;
			Plane m_bottom;
		}
	};
};
