#version 450 core

#define GRID_SIDE TERRAIN_GENERATE_GRID_SIDE
#define GROUP_SIZE 512

layout(local_size_x = GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

const uint num_indices = TERRAIN_GENERATE_NUM_INDICES;
const uint num_vertices = TERRAIN_GENERATE_NUM_VERTICES;
const uint num_nodes = TERRAIN_GENERATE_NUM_NODES;
const uint num_new_points = TRIANGULATE_MAX_NEW_POINTS;
const uint quadtree_levels = QUADTREE_LEVELS;

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
	// }

	uint indices[num_indices];
	vec4 positions[num_vertices];
	Triangle triangles[num_indices / 3];
	vec4 new_points[num_new_points];
};

const uint num_quadtree_nodes = (1 << quadtree_levels) * (1 << quadtree_levels);
const uint aligned_quadtree_index_num = (num_quadtree_nodes + 4) + (16 - ((num_quadtree_nodes + 4) % 16));

layout(set = 0, binding = 0) buffer terrain_buffer_t
{
	uint quadtree_index_map[aligned_quadtree_index_num - 4];
	vec2 quadtree_min;
	vec2 quadtree_max;
	terrain_data_t data[num_nodes];
} terrain_buffer;

layout(push_constant) uniform frame_data_t
{
	mat4 camera_vp;
	vec4 camera_pos;
	vec2 min;
	vec2 max;
	uint buffer_slot;
} frame_data;


///// TERRAIN

#define HASHSCALE1 .1031
const mat2 rotate2D = mat2(1.3623, 1.7531, -1.7131, 1.4623);
vec2 add = vec2(1.0, 0.0);

float Hash12(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * HASHSCALE1);
	p3 += dot(p3, p3.yzx + 19.19);
	return fract((p3.x + p3.y) * p3.z);
}

float Noise(in vec2 x)
{
	vec2 p = floor(x);
	vec2 f = fract(x);
	f = f * f*(3.0 - 2.0*f);

	float res = mix(mix(Hash12(p), Hash12(p + add.xy), f.x),
		mix(Hash12(p + add.yx), Hash12(p + add.xx), f.x), f.y);
	return res;
}

float Terrain(in vec2 p)
{
	vec2 pos = p * 0.05;
	float w = (Noise(pos*.25)*0.75 + .15);
	w = 66.0 * w * w;
	vec2 dxy = vec2(0.0, 0.0);
	float f = .0;
	for (int i = 0; i < 5; i++)
	{
		f += w * Noise(pos);
		w = -w * 0.4;
		pos = rotate2D * pos;
	}
	float ff = Noise(pos*.002);

	f += pow(abs(ff), 5.0)*275. - 5.0;
	return f;
}

///////////////////

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

float find_circum_radius_squared(vec2 P, vec2 Q, vec2 R)
{
	float a = distance(P, Q);
	float b = distance(P, R);
	float c = distance(R, Q);

	return (a * a * b * b * c * c) / ((a + b + c) * (b + c - a) * (c + a - b) * (a + b - c));
}

/////////////////

void main(void)
{
	if (gl_GlobalInvocationID.x == 0)
	{
		terrain_buffer.data[frame_data.buffer_slot].index_count = 6 * (GRID_SIDE - 1) * (GRID_SIDE - 1);
		terrain_buffer.data[frame_data.buffer_slot].instance_count = 1;
		terrain_buffer.data[frame_data.buffer_slot].first_index = 0;
		terrain_buffer.data[frame_data.buffer_slot].vertex_offset = 0;
		terrain_buffer.data[frame_data.buffer_slot].first_instance = 0;

		terrain_buffer.data[frame_data.buffer_slot].vertex_count = GRID_SIDE * GRID_SIDE;
		terrain_buffer.data[frame_data.buffer_slot].new_points_count = 0;

		terrain_buffer.data[frame_data.buffer_slot].min = frame_data.min;
		terrain_buffer.data[frame_data.buffer_slot].max = frame_data.max;
	}

	// Positions
	uint i = gl_GlobalInvocationID.x;
	while (i < GRID_SIDE * GRID_SIDE)
	{
		float x = frame_data.min.x + ((i % GRID_SIDE) / float(GRID_SIDE - 1)) * (frame_data.max.x - frame_data.min.x);
		float z = frame_data.min.y + float(i / GRID_SIDE) / float(GRID_SIDE - 1) * (frame_data.max.y - frame_data.min.y);

		terrain_buffer.data[frame_data.buffer_slot].positions[i] = vec4(x, -Terrain(vec2(x, z)) - 0.5, z, 1.0);

		i += GROUP_SIZE;
	}

	barrier();
	memoryBarrierBuffer();

	// Triangles
	i = gl_GlobalInvocationID.x; 
	while (i < (GRID_SIDE - 1) * (GRID_SIDE - 1))
	{
		uint y = i / (GRID_SIDE - 1);
		uint x = i % (GRID_SIDE - 1);
		uint index = y * GRID_SIDE + x;

		// Indices
		terrain_buffer.data[frame_data.buffer_slot].indices[i * 6] = index;
		terrain_buffer.data[frame_data.buffer_slot].indices[i * 6 + 1] = index + GRID_SIDE + 1;
		terrain_buffer.data[frame_data.buffer_slot].indices[i * 6 + 2] = index + 1;

		terrain_buffer.data[frame_data.buffer_slot].indices[i * 6 + 3] = index;
		terrain_buffer.data[frame_data.buffer_slot].indices[i * 6 + 4] = index + GRID_SIDE;
		terrain_buffer.data[frame_data.buffer_slot].indices[i * 6 + 5] = index + GRID_SIDE + 1;

		// Circumcentres
		vec2 P1 = terrain_buffer.data[frame_data.buffer_slot].positions[index].xz;
		vec2 Q1 = terrain_buffer.data[frame_data.buffer_slot].positions[index + GRID_SIDE + 1].xz;
		vec2 R1 = terrain_buffer.data[frame_data.buffer_slot].positions[index + 1].xz;
		terrain_buffer.data[frame_data.buffer_slot].triangles[i * 2].circumcentre = find_circum_center(P1, Q1, R1);

		vec2 P2 = terrain_buffer.data[frame_data.buffer_slot].positions[index].xz;
		vec2 Q2 = terrain_buffer.data[frame_data.buffer_slot].positions[index + GRID_SIDE].xz;
		vec2 R2 = terrain_buffer.data[frame_data.buffer_slot].positions[index + GRID_SIDE + 1].xz;
		terrain_buffer.data[frame_data.buffer_slot].triangles[i * 2 + 1].circumcentre = find_circum_center(P2, Q2, R2);

		// Circumradii
		terrain_buffer.data[frame_data.buffer_slot].triangles[i * 2].circumradius = find_circum_radius_squared(P1, Q1, R1);
		terrain_buffer.data[frame_data.buffer_slot].triangles[i * 2 + 1].circumradius = find_circum_radius_squared(P2, Q2, R2);

		i += GROUP_SIZE;
	}
}
