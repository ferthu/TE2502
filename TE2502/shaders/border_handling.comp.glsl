#version 450 core

#define WORK_GROUP_SIZE 32

layout(local_size_x = WORK_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

const uint num_indices = TERRAIN_GENERATE_NUM_INDICES;
const uint num_vertices = TERRAIN_GENERATE_NUM_VERTICES;
const uint num_nodes = TERRAIN_GENERATE_NUM_NODES;
const uint num_new_points = TRIANGULATE_MAX_NEW_POINTS;
const uint quadtree_levels = QUADTREE_LEVELS;

layout(push_constant) uniform frame_data_t
{
	uint node_index;
} frame_data;

struct Triangle
{
	vec2 circumcentre;
	float circumradius;
	uint pad;
};

struct terrain_data_t
{
	uint index_count;
	uint instance_count;
	uint first_index;
	int  vertex_offset;
	uint first_instance;

	// struct BufferNodeHeader {
	uint vertex_count;
	uint new_points_count;
	uint pad;

	vec2 min;
	vec2 max;

	float proximity[4];
	uint proximity_count[4];
	uint border_level[4];
	// }

	uint indices[num_indices];
	vec4 positions[num_vertices];
	Triangle triangles[num_indices / 3];
	vec4 new_points[num_new_points];
};

const uint quadtree_data_size = (1 << quadtree_levels) * (1 << quadtree_levels) + 4;
const uint pad_size = 16 - (quadtree_data_size % 16);

coherent layout(set = 0, binding = 0) buffer terrain_buffer_t
{
	uint quadtree_index_map[(1 << quadtree_levels) * (1 << quadtree_levels)];
	vec2 quadtree_min;
	vec2 quadtree_max;
	uint pad[pad_size];
	terrain_data_t data[num_nodes];
} terrain_buffer;



///// CIRCUMCIRCLE

// Function to find the line given two points
void line_from_points(vec2 p1, vec2 p2, out float a, out float b, out float c)
{
	a = p2.y - p1.y;
	b = p1.x - p2.x;
	c = a * p1.x + b * p2.y;
}

// Function which converts the input line to its 
// perpendicular bisector. It also inputs the points 
// whose mid-point lies on the bisector 
void perpendicular_bisector_from_line(vec2 p1, vec2 p2, inout float a, inout float b, inout float c)
{
	vec2 mid_point = vec2((p1.x + p2.x) * 0.5, (p1.y + p2.y) * 0.5);

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

///////////////////


//// TERRAIN 

vec2 add = vec2(1.0, 0.0);
#define HASHSCALE1 .1031

const mat2 rotate2D = mat2(1.3623, 1.7531, -1.7131, 1.4623);

float hash12(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * HASHSCALE1);
	p3 += dot(p3, p3.yzx + 19.19);
	return fract((p3.x + p3.y) * p3.z);
}

float noise(in vec2 x)
{
	vec2 p = floor(x);
	vec2 f = fract(x);
	f = f * f*(3.0 - 2.0*f);

	float res = mix(mix(hash12(p), hash12(p + add.xy), f.x),
		mix(hash12(p + add.yx), hash12(p + add.xx), f.x), f.y);
	return res;
}

float terrain(in vec2 p)
{
	vec2 pos = p * 0.05;
	float w = (noise(pos*.25)*0.75 + .15);
	w = 66.0 * w * w;
	vec2 dxy = vec2(0.0, 0.0);
	float f = .0;
	for (int i = 0; i < 5; i++)
	{
		f += w * noise(pos);
		w = -w * 0.4;
		pos = rotate2D * pos;
	}
	float ff = noise(pos*.002);

	f += pow(abs(ff), 5.0)*275. - 5.0;
	return f;
}

//////////

void main(void)
{
	const uint node_index = frame_data.node_index;

	const uint thid = gl_GlobalInvocationID.x;

	const vec2 node_min = terrain_buffer.data[node_index].min;
	const vec2 node_max = terrain_buffer.data[node_index].max;
	const float side = node_max.x - node_min.x;

	if (thid == 0)
	{
		uint count = terrain_buffer.data[node_index].new_points_count;

		// Borders
		for (uint bb = 0; bb < 4; ++bb)
		{
			vec2 min = node_min;
			vec2 max = node_max;
			uint level = terrain_buffer.data[node_index].border_level[bb];

			int nodes_per_side = 1 << quadtree_levels;
			int ny = int((node_min.y - terrain_buffer.quadtree_min.y + 1) / side);
			int nx = int((node_min.x - terrain_buffer.quadtree_min.x + 1) / side);

			if (bb == 0)
			{
				min.y = max.y;
				++ny;
			}
			else if (bb == 1)
			{
				min.x = max.x;
				++nx;
			}
			else if (bb == 2)
			{
				max.y = min.y;
				--ny;
			}
			else if (bb == 3)
			{
				max.x = min.x;
				--nx;
			}

			int neighbour_index = ny * nodes_per_side + nx;
			uint neighbour_border = (bb + 2) % 4;
			uint neighbour_level = 0;

			// Check if valid neighbour
			if (ny >= 0 && ny < nodes_per_side && nx >= 0 && nx < nodes_per_side)
				neighbour_level = terrain_buffer.data[terrain_buffer.quadtree_index_map[neighbour_index]].border_level[neighbour_border];

			const float limiter = 0.35f;

			// Check if border needs splitting
			while (terrain_buffer.data[node_index].proximity_count[bb] > level
				|| neighbour_level > level)
			{
				terrain_buffer.data[node_index].proximity[bb] *= limiter;
				level *= 2;

				float part = 1.f / level;
				float p = part;
				while (p < 1.f)
				{
					float x = mix(min.x, max.x, p);
					float z = mix(min.y, max.y, p);
					vec4 border_point = vec4(x, -terrain(vec2(x, z)), z, 0);
					terrain_buffer.data[node_index].new_points[count++] = border_point;

					p += part * 2;
				}
			}
			terrain_buffer.data[node_index].border_level[bb] = level;
		}
		terrain_buffer.data[node_index].new_points_count = count;
	}
}