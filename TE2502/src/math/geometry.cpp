#include "geometry.hpp"

void Plane::normalize()
{
	float mag = 1.0f / sqrt(m_plane.x * m_plane.x + m_plane.y * m_plane.y + m_plane.z * m_plane.z);
	m_plane.x = m_plane.x * mag;
	m_plane.y = m_plane.y * mag;
	m_plane.z = m_plane.z * mag;
	m_plane.w = m_plane.w * mag;
}

bool frustum_aabbxz_intersection(Frustum& frustum, AabbXZ& aabb)
{
	// Check if AABB is inside frustum planes
	for (int i = 0; i < 6; i++)
	{
		float min_y = -300.0f;
		float max_y =  100.0f;

		// Number of points on the negative halfspace
		int num_out = 0;
		num_out += ((glm::dot(frustum.m_planes[i].m_plane, glm::vec4(aabb.m_min.x, min_y, aabb.m_min.y, 1.0f)) < 0.0f) ? 1 : 0);	
		num_out += ((glm::dot(frustum.m_planes[i].m_plane, glm::vec4(aabb.m_max.x, min_y, aabb.m_min.y, 1.0f)) < 0.0f) ? 1 : 0);
		num_out += ((glm::dot(frustum.m_planes[i].m_plane, glm::vec4(aabb.m_min.x, max_y, aabb.m_min.y, 1.0f)) < 0.0f) ? 1 : 0);
		num_out += ((glm::dot(frustum.m_planes[i].m_plane, glm::vec4(aabb.m_max.x, max_y, aabb.m_min.y, 1.0f)) < 0.0f) ? 1 : 0);
		num_out += ((glm::dot(frustum.m_planes[i].m_plane, glm::vec4(aabb.m_min.x, min_y, aabb.m_max.y, 1.0f)) < 0.0f) ? 1 : 0);
		num_out += ((glm::dot(frustum.m_planes[i].m_plane, glm::vec4(aabb.m_max.x, min_y, aabb.m_max.y, 1.0f)) < 0.0f) ? 1 : 0);
		num_out += ((glm::dot(frustum.m_planes[i].m_plane, glm::vec4(aabb.m_min.x, max_y, aabb.m_max.y, 1.0f)) < 0.0f) ? 1 : 0);
		num_out += ((glm::dot(frustum.m_planes[i].m_plane, glm::vec4(aabb.m_max.x, max_y, aabb.m_max.y, 1.0f)) < 0.0f) ? 1 : 0);

		// If all points are in the negative halfspace, the frustum is completely outside the AABB
		if (num_out == 8) 
			return false;
	}

	// Check that frustum is inside AABB x and z

	// Number of points outside box
	int num_out;

	// If all points are outside and on the same side of AABB, miss
	num_out = 0; for (int i = 0; i < 8; i++) num_out += ((frustum.m_corners[i].x > aabb.m_max.x) ? 1 : 0); if (num_out == 8) return false;
	num_out = 0; for (int i = 0; i < 8; i++) num_out += ((frustum.m_corners[i].x < aabb.m_min.x) ? 1 : 0); if (num_out == 8) return false;
	num_out = 0; for (int i = 0; i < 8; i++) num_out += ((frustum.m_corners[i].z > aabb.m_max.y) ? 1 : 0); if (num_out == 8) return false;
	num_out = 0; for (int i = 0; i < 8; i++) num_out += ((frustum.m_corners[i].z < aabb.m_min.y) ? 1 : 0); if (num_out == 8) return false;

	return true;
}

AabbXZ::AabbXZ(glm::vec2 min, glm::vec2 max) : m_min(min), m_max(max)
{
}
