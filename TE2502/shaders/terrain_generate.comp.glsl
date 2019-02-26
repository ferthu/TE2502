#version 450 core

#define GRID_SIDE TERRAIN_GENERATE_GRID_SIDE
#define WORK_GROUP_SIZE 1024

layout(local_size_x = WORK_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

const uint num_indices = TERRAIN_GENERATE_NUM_INDICES;
const uint num_vertices = TERRAIN_GENERATE_NUM_VERTICES;
const uint num_nodes = TERRAIN_GENERATE_NUM_NODES;
const uint num_new_points = TRIANGULATE_MAX_NEW_POINTS;
const uint quadtree_levels = QUADTREE_LEVELS;
const uint border_zones = BORDER_ZONES;
const uint border_proximity = BORDER_START_PROXIMITY;

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

		float proximity[4 * border_zones];
		uint proximity_count[4 * border_zones];
		uint border_level[4 * border_zones];
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


layout(push_constant) uniform frame_data_t
{
	mat4 camera_vp;
	vec4 camera_pos;
	vec2 min;
	vec2 max;
	uint node_index;
} frame_data;


///// TERRAIN

vec2 add = vec2(1.0, 0.0);
#define HASHSCALE1 .1031

// This peturbs the fractal positions for each iteration down...
// Helps make nice twisted landscapes...
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
	const uint node_index = frame_data.node_index;

	if (gl_GlobalInvocationID.x == 0)
	{
		terrain_buffer.data[node_index].index_count = 6 * (GRID_SIDE - 1) * (GRID_SIDE - 1);
		terrain_buffer.data[node_index].instance_count = 1;
		terrain_buffer.data[node_index].first_index = 0;
		terrain_buffer.data[node_index].vertex_offset = 0;
		terrain_buffer.data[node_index].first_instance = 0;

		terrain_buffer.data[node_index].vertex_count = GRID_SIDE * GRID_SIDE;
		terrain_buffer.data[node_index].new_points_count = 0;

		terrain_buffer.data[node_index].min = frame_data.min;
		terrain_buffer.data[node_index].max = frame_data.max;

		const float proximity = (frame_data.max.x - frame_data.min.x) * border_proximity / 100.f;
		for (uint b = 0; b < 4 * border_zones; ++b)
		{
			terrain_buffer.data[node_index].proximity[b] = proximity;
			terrain_buffer.data[node_index].border_level[b] = 1;
		}
	}

	// Positions
	uint i = gl_GlobalInvocationID.x;
	while (i < GRID_SIDE * GRID_SIDE)
	{
		float x = frame_data.min.x + ((i % GRID_SIDE) / float(GRID_SIDE - 1)) * (frame_data.max.x - frame_data.min.x);
		float z = frame_data.min.y + float(i / GRID_SIDE) / float(GRID_SIDE - 1) * (frame_data.max.y - frame_data.min.y);

		terrain_buffer.data[node_index].positions[i] = vec4(x, -terrain(vec2(x, z)) - 0.5, z, 1.0);


		//if (x < 0 && z < 0 && i == 0)
		//{
		//	int max = 10;
		//	terrain_buffer.data[node_index].new_points_count = max * max;
		//	for (int a = 0; a < max; ++a)
		//	{
		//		for (int b = 0; b < max; ++b)
		//		{
		//			terrain_buffer.data[node_index].new_points[a * max + b] = vec4(-200 + a * 4, -100, -60 + b * 0.5, 1);
		//		}
		//	}
		//}

		i += WORK_GROUP_SIZE;
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
		uint offset = i * 6;
		terrain_buffer.data[node_index].indices[offset] = index;
		terrain_buffer.data[node_index].indices[offset + 1] = index + GRID_SIDE + 1;
		terrain_buffer.data[node_index].indices[offset + 2] = index + 1;

		terrain_buffer.data[node_index].indices[offset + 3] = index;
		terrain_buffer.data[node_index].indices[offset + 4] = index + GRID_SIDE;
		terrain_buffer.data[node_index].indices[offset + 5] = index + GRID_SIDE + 1;

		// Circumcentres
		offset = i * 2;
		vec2 P1 = terrain_buffer.data[node_index].positions[index].xz;
		vec2 Q1 = terrain_buffer.data[node_index].positions[index + GRID_SIDE + 1].xz;
		vec2 R1 = terrain_buffer.data[node_index].positions[index + 1].xz;
		terrain_buffer.data[node_index].triangles[offset].circumcentre = find_circum_center(P1, Q1, R1);

		vec2 P2 = terrain_buffer.data[node_index].positions[index].xz;
		vec2 Q2 = terrain_buffer.data[node_index].positions[index + GRID_SIDE].xz;
		vec2 R2 = terrain_buffer.data[node_index].positions[index + GRID_SIDE + 1].xz;
		terrain_buffer.data[node_index].triangles[offset + 1].circumcentre = find_circum_center(P2, Q2, R2);

		// Circumradii
		terrain_buffer.data[node_index].triangles[offset].circumradius = find_circum_radius_squared(P1, Q1, R1);
		terrain_buffer.data[node_index].triangles[offset + 1].circumradius = find_circum_radius_squared(P2, Q2, R2);

		i += WORK_GROUP_SIZE;
	}
}
