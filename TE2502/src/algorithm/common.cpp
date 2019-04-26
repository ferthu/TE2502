#include "algorithm/common.hpp"

void replace_connection_index(TerrainBuffer* tb, cuint node_index, cuint triangle_to_check, cuint index_to_replace, cuint new_value)
{
	if (triangle_to_check <= INVALID - 9)
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
	curvature -= terrain(vec2(p.x, p.z));

	return abs(curvature);
}


#pragma region TERRAIN

vec2 addxy = vec2(1.0f, 0.0f);
vec2 addxx = vec2(1.0f, 1.0f);
vec2 addyx = vec2(0.0f, 1.0f);
#define HASHSCALE1 .1031f

float mix(float a, float b, float i)
{
	return a * (1.0f - i) + b * i;
}

float clamp(float a, float min, float max)
{
	if (a < min)
		a = min;
	if (a > max)
		a = max;

	return a;
}

//// Low-res stuff

float hash12(vec2 p)
{
	vec3 p3 = fract(vec3(p.x, p.y, p.x) * HASHSCALE1);
	p3 += dot(p3, vec3(p3.y, p3.z, p3.x) + 19.19f);
	return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 x)
{
	vec2 p = floor(x);
	vec2 f = fract(x);
	f = f * f * (3.0f - 2.0f * f);

	float res = mix(mix(hash12(p), hash12(p + addxy), f.x),
		mix(hash12(p + addyx), hash12(p + addxx), f.x), f.y);
	return res;
}

float terrain(vec2 p)
{
	vec2 pos = p * 0.05f;
	float w = (noise(pos * 0.25f) * 0.75f + 0.15f);
	w = 66.0f * w * w;
	vec2 dxy = vec2(0.0, 0.0);
	float f = .0;
	for (int i = 0; i < 5; i++)
	{
		f += w * noise(pos);
		w = -w * 0.4f;
		pos = rotate2D * pos;
	}
	float ff = noise(pos * 0.002f);

	f += pow(abs(ff), 5.0f) * 275.0f - 5.0f;
	return f;
}

float height_to_surface(vec3 p)
{
	float h = terrain({ p.x, p.z });

	return -p.y - h;
}

///////

//// High-res stuff
//#define HASHSCALE3 vec3(.1031f, .1030f, .0973f)
//#define HASHSCALE4 vec4(1031f, .1030f, .0973f, .1099f)
//
//vec2 hash22(vec2 p)
//{
//	vec3 p3 = fract(vec3(p.x, p.y, p.x) * HASHSCALE3);
//	p3 += dot(p3, vec3(p3.y, p3.z, p3.x) + 19.19f);
//	return fract((vec2(p3.x, p3.x) + vec2(p3.y, p3.z)) * vec2(p3.z, p3.y));
//
//}
//
//vec2 noise2(vec2 x)
//{
//	vec2 p = floor(x);
//	vec2 f = fract(x);
//	f = f * f * (3.0f - 2.0f * f);
//	float n = p.x + p.y * 57.0f;
//	vec2 res = mix(mix(hash22(p), hash22(p + addxy), f.x),
//		mix(hash22(p + addyx), hash22(p + addxx), f.x), f.y);
//	return res;
//}
//
////--------------------------------------------------------------------------
//// High def version only used for grabbing normal information.
//float terrain2(vec2 p)
//{
//	// There's some real magic numbers in here! 
//	// The noise calls add large mountain ranges for more variation over distances...
//	vec2 pos = p * 0.05f;
//	float w = (noise(pos * 0.25f) * 0.75f + 0.15f);
//	w = 66.0f * w * w;
//	vec2 dxy = vec2(0.0f, 0.0f);
//	float f = 0.0f;
//	for (int i = 0; i < 5; i++)
//	{
//		f += w * noise(pos);
//		w = -w * 0.4f;	//...Flip negative and positive for varition	   
//		pos = rotate2D * pos;
//	}
//	float ff = noise(pos * 0.002f);
//	f += powf(abs(ff), 5.0f) * 275.0f - 5.0f;
//
//
//	// That's the last of the low resolution, now go down further for the Normal data...
//	for (int i = 0; i < 6; i++)
//	{
//		f += w * noise(pos);
//		w = -w * 0.4f;
//		pos = rotate2D * pos;
//	}
//
//	return f;
//}

///////

//// Coloring

//vec3 sun_light = normalize(vec3(0.4, 0.4, 0.48));
//vec3 sun_color = vec3(1.0, .9, .83);
//float specular = 0.0;
//
//// Calculate sun light...
//void do_lighting(vec3& mat, vec3 pos, vec3 normal, vec3 eye_dir, cuint ambient)
//{
//	float h = dot(sun_light, normal);
//	float c = std::max(h, 0.0f) + ambient;
//	mat = mat * sun_color * c;
//	// Specular...
//	if (h > 0.0)
//	{
//		vec3 R = reflect(sun_light, normal);
//		float spec_amount = pow(std::max(dot(R, normalize(eye_dir)), 0.0f), 3.0f) * specular;
//		mat = mix(mat, sun_color, spec_amount);
//	}
//}
//
//// Hack the height, position, and normal data to create the coloured landscape
//vec3 surface_color(vec3 pos, vec3 normal, float dis, vec3 cam_pos)
//{
//	vec3 mat;
//	specular = 0.0f;
//	float ambient = 0.1f;
//	vec3 dir = normalize(pos - cam_pos);
//
//	vec3 mat_pos = pos * 2.0f;// ... I had change scale halfway though, this lazy multiply allow me to keep the graphic scales I had
//
//	float f = clamp(noise(vec2(mat_pos.x, mat_pos.z) * 0.05f), 0.0f, 1.0f);
//	f += noise(vec2(mat_pos.x, mat_pos.z) * 0.1f + vec2(normal.y, normal.z) * 1.08f) * 0.85f;
//	f *= 0.55f;
//	vec3 m = mix(vec3(.63*f + .2, .7*f + .1, .7*f + .1), vec3(f*.43 + .1, f*.3 + .2, f*.35 + .1), f * 0.65f);
//	mat = m * vec3(f * m.x + .36f, f * m.y + .30f, f * m.z + .28f);
//	// Should have used smoothstep to add colours, but left it using 'if' for sanity...
//	if (normal.y < .5f)
//	{
//		float v = normal.y;
//		float c = (0.5f - normal.y) * 4.0f;
//		c = clamp(c*c, 0.1f, 1.0f);
//		f = noise(vec2(mat_pos.x * 0.09f, mat_pos.z * 0.095f) + vec2(mat_pos.y, mat_pos.y) * 0.15f);
//		f += noise(vec2(mat_pos.x * 2.233f, mat_pos.z * 2.23f)) * 0.5f;
//		mat = mix(mat, vec3(.4f * f), c);
//		specular += 0.1f;
//	}
//
//	// Grass. Use the normal to decide when to plonk grass down...
//	if (normal.y > .65f)
//	{
//		m = vec3(noise(vec2(mat_pos.x, mat_pos.z) * 0.023f) * 0.5f + 0.15f, noise(vec2(mat_pos.x, mat_pos.z) * 0.03f) * 0.6f + 0.25f, 0.0f);
//		m *= (normal.y - 0.65f) * 0.6f;
//		mat = mix(mat, m, clamp((normal.y - .65f) * 1.3f * (45.35f - mat_pos.y)* 0.1f, 0.0f, 1.0f));
//	}
//
//	// Snow topped mountains...
//	if (mat_pos.y > 80.0f && normal.y > .42f)
//	{
//		float snow = clamp((mat_pos.y - 80.0f - noise(vec2(mat_pos.x, mat_pos.z) * 0.1f) * 28.0f) * 0.035f, 0.0f, 1.0f);
//		mat = mix(mat, vec3(.7f, .7f, .8f), snow);
//		specular += snow;
//		ambient += snow * 0.3f;
//	}
//
//	do_lighting(mat, pos, normal, dir, ambient);
//
//	return mat;
//}
//
//// Some would say, most of the magic is done in post! :D
//vec3 post_effects(vec3 rgb)
//{
//	return (1.0f - exp(-rgb * 6.0f)) * 1.0024f;
//}

#pragma endregion


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
