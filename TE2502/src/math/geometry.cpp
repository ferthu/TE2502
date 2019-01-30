#include "geometry.hpp"

void Plane::normalize()
{
	float mag = 1.0f / sqrt(m_plane.x * m_plane.x + m_plane.y * m_plane.y + m_plane.z * m_plane.z);
	m_plane.x = m_plane.x * mag;
	m_plane.y = m_plane.y * mag;
	m_plane.z = m_plane.z * mag;
	m_plane.w = m_plane.w * mag;
}
