#include "algorithm/common.hpp"

void replace_connection_index(TerrainBuffer* tb, cuint node_index, cuint triangle_to_check, cuint index_to_replace, cuint new_value)
{
	if (triangle_to_check < INVALID - 9)
	{
		for (uint tt = 0; tt < 3; ++tt)
		{
			const uint triangle_index = tb->data[node_index].triangle_connections[triangle_to_check * 3 + tt];
			if (triangle_index == index_to_replace)
			{
				tb->data[node_index].triangle_connections[triangle_to_check * 3 + tt] = new_value;
				break;
			}
		}
	}
}

float curvature(vec3 p, float* log_filter)
{
	const float pi = 3.1415f;

	float sample_step = 1.0f;

	float curvature = 0.0f;

	for (int x = -filter_radius; x <= filter_radius; x++)
	{
		for (int y = -filter_radius; y <= filter_radius; y++)
		{
			curvature += terrain(vec2(p.x, p.z) + vec2(sample_step * x, sample_step * y)) * log_filter[(y + filter_radius) * filter_side + (x + filter_radius)];
		}
	}

	// Normalize for height
	curvature += terrain(vec2(p.x, p.z));

	return abs(curvature);
}

#pragma region CC

// Function to find the line given two points
void line_from_points(vec2 p1, vec2 p2, float& a, float& b, float& c)
{
	a = p2.y - p1.y;
	b = p1.x - p2.x;
	c = a * p1.x + b * p2.y;
}

// Function which converts the input line to its 
// perpendicular bisector. It also inputs the points 
// whose mid-point lies on the bisector 
void perpendicular_bisector_from_line(vec2 p1, vec2 p2, float& a, float& b, float& c)
{
	vec2 mid_point = vec2((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);

	// c = -bx + ay 
	c = -b * mid_point.x + a * mid_point.y;

	float temp = a;
	a = -b;
	b = temp;
}

// Returns the intersection point of two lines 
vec2 line_line_intersection(float a1, float b1, float c1, float a2, float b2, float c2)
{
	float determinant = a1 * b2 - a2 * b1;

	float x = (b2 * c1 - b1 * c2) / determinant;
	float y = (a1 * c2 - a2 * c1) / determinant;

	return vec2(x, y);
}

vec2 find_circum_center(vec2 P, vec2 Q, vec2 R)
{
	// Line PQ is represented as ax + by = c 
	float a, b, c;
	line_from_points(P, Q, a, b, c);

	// Line QR is represented as ex + fy = g 
	float e, f, g;
	line_from_points(Q, R, e, f, g);

	// Converting lines PQ and QR to perpendicular 
	// vbisectors. After this, L = ax + by = c 
	// M = ex + fy = g 
	perpendicular_bisector_from_line(P, Q, a, b, c);
	perpendicular_bisector_from_line(Q, R, e, f, g);

	// The point of intersection of L and M gives 
	// the circumcenter 
	return line_line_intersection(a, b, c, e, f, g);
}

float find_circum_radius_squared(float a, float b, float c)
{
	return (a * a * b * b * c * c) / ((a + b + c) * (b + c - a) * (c + a - b) * (a + b - c));
}

float find_circum_radius_squared(vec2 P, vec2 Q, vec2 R)
{
	float a = distance(P, Q);
	float b = distance(Q, R);
	float c = distance(R, P);
	return (a * a * b * b * c * c) / ((a + b + c) * (b + c - a) * (c + a - b) * (a + b - c));
}


#pragma endregion
