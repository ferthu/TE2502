#version 450 core

#define WORK_GROUP_SIZE 1024
layout(local_size_x = WORK_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

// INPUT
layout(push_constant) uniform frame_data_t
{
	mat4 vp;
	vec4 camera_position;
	vec2 screen_size;
	float threshold;
	float area_multiplier;
	float curvature_multiplier;
	uint node_index;
} frame_data;

const uint num_indices = TERRAIN_GENERATE_NUM_INDICES;
const uint num_vertices = TERRAIN_GENERATE_NUM_VERTICES;
const uint num_nodes = TERRAIN_GENERATE_NUM_NODES;
const uint num_new_points = TRIANGULATE_MAX_NEW_POINTS;
const uint quadtree_levels = QUADTREE_LEVELS;

// OUPUT
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

coherent layout(set = 0, binding = 0) buffer terrain_buffer_t
{
	uint quadtree_index_map[aligned_quadtree_index_num - 4];
	vec2 quadtree_min;
	vec2 quadtree_max;
	terrain_data_t data[num_nodes];
} terrain_buffer;


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
		w = -w * 0.4;	//...Flip negative and positive for variation
		pos = rotate2D * pos;
	}
	float ff = noise(pos*.002);

	f += pow(abs(ff), 5.0)*275. - 5.0;
	return f;
}

float height_to_surface(in vec3 p)
{
	float h = terrain(p.xz);

	return -p.y - h;
}

float binary_subdivision(in vec3 rO, in vec3 rD, in vec2 t, in int divisions)
{
	// Home in on the surface by dividing by two and split...
	float halfway_t;

	for (int i = 0; i < divisions; i++)
	{
		halfway_t = dot(t, vec2(.5));
		float d = height_to_surface(rO + halfway_t * rD);
		t = mix(vec2(t.x, halfway_t), vec2(halfway_t, t.y), step(0.5, d));
	}
	return halfway_t;
}

//////////

// Returns true if p is valid NDC
bool clip(in vec4 p)
{
	return (abs(p.x) <= p.w &&
			abs(p.y) <= p.w &&
			abs(p.z) <= p.w);
}

float curvature(in vec3 p)
{
	const float pi = 3.1415;

	float camera_distance = distance(p, frame_data.camera_position.xyz);

	float sample_step = 5.5 + pow(camera_distance * 0.5, 0.6);
	const float gaussian_width = 1.0;
	const int filter_radius = 2;	// Side length of grid is filter_radius * 2 + 1
	const int filter_side = filter_radius * 2 + 1;
	float log_filter[filter_side * filter_side];

	/////////////////////////////////////////
	float sum = 0.0;

	for (int x = -filter_radius; x <= filter_radius; x++)
	{
		for (int y = -filter_radius; y <= filter_radius; y++)
		{
			// https://homepages.inf.ed.ac.uk/rbf/HIPR2/log.htm
			float t = -((x * x + y * y) / (2.0 * gaussian_width * gaussian_width));
			float log = -(1 / (pi * pow(gaussian_width, 4.0))) * (1.0 + t) * exp(t);

			log_filter[(y + filter_radius) * filter_side + (x + filter_radius)] = log;
			sum += log;
		}
	}

	// Normalize filter
	float correction = 1.0 / sum;
	for (uint i = 0; i < filter_side * filter_side; i++)
	{
		log_filter[i] *= correction;
	}

	float curvature = 0.0;

	for (int x = -filter_radius; x <= filter_radius; x++)
	{
		for (int y = -filter_radius; y <= filter_radius; y++)
		{
			curvature += terrain(p.xz + vec2(sample_step * x, sample_step * y)) * log_filter[(y + filter_radius) * filter_side + (x + filter_radius)];
		}
	}

	// Normalize for height
	curvature -= terrain(p.xz);

	return abs(curvature);
}

const uint max_new_points = TRIANGULATE_MAX_NEW_POINTS / WORK_GROUP_SIZE + 1;
vec2 new_points[max_new_points];
shared uint s_counts[WORK_GROUP_SIZE];
shared uint s_total;

void main(void)
{
	const uint thid = gl_GlobalInvocationID.x;
	const uint node_index = frame_data.node_index;
	const uint index_count = terrain_buffer.data[node_index].index_count;

	uint new_point_count = 0;

	// For every triangle
	for (uint i = thid * 3; i + 3 <= index_count && new_point_count < max_new_points; i += WORK_GROUP_SIZE * 3)
	{
		// Get vertices
		vec4 v0 = terrain_buffer.data[node_index].positions[terrain_buffer.data[node_index].indices[i    ]];
		vec4 v1 = terrain_buffer.data[node_index].positions[terrain_buffer.data[node_index].indices[i + 1]];
		vec4 v2 = terrain_buffer.data[node_index].positions[terrain_buffer.data[node_index].indices[i + 2]];

		// Get clipspace coordinates
		vec4 c0 = frame_data.vp * v0;
		vec4 c1 = frame_data.vp * v1;
		vec4 c2 = frame_data.vp * v2;

		// Check if any vertex is visible (shitty clipping)
		if (clip(c0) || clip(c1) || clip(c2))
		{
			// Calculate screen space area

			c0 /= c0.w;
			c1 /= c1.w;
			c2 /= c2.w;

			// a, b, c is triangle side lengths
			float a = distance(c0.xy, c1.xy);
			float b = distance(c0.xy, c2.xy);
			float c = distance(c1.xy, c2.xy);

			// s is semiperimeter
			float s = (a + b + c) * 0.5;

			float area = pow(/*sqrt*/(s * (s - a) * (s - b) * (s - c)), frame_data.area_multiplier);

			vec3 mid = (v0.xyz + v1.xyz + v2.xyz) / 3.0;
			float curv = pow(curvature(mid), frame_data.curvature_multiplier);

			if (curv * area >= frame_data.threshold)
			{
				new_points[new_point_count] = mid.xz;
				++new_point_count;
			}
		}
	}




	////// PREFIX SUM

	const uint n = WORK_GROUP_SIZE;

	// Load into shared memory
	s_counts[thid] = new_point_count;

	barrier();
	memoryBarrierShared();

	if (thid == 0)
		s_total = s_counts[n - 1];

	barrier();
	memoryBarrierShared();

	int offset = 1;
	for (uint d = n >> 1; d > 0; d >>= 1) // Build sun in place up the tree
	{
		barrier();
		memoryBarrierShared();
		if (thid < d)
		{
			uint ai = offset * (2 * thid + 1) - 1;
			uint bi = offset * (2 * thid + 2) - 1;
			s_counts[bi] += s_counts[ai];
		}
		offset *= 2;
	}
	if (thid == 0) { s_counts[n - 1] = 0; } // Clear the last element
	for (int d = 1; d < n; d *= 2) // Traverse down tree & build scan
	{
		offset >>= 1;
		barrier();
		memoryBarrierShared();
		if (thid < d)
		{
			uint ai = offset * (2 * thid + 1) - 1;
			uint bi = offset * (2 * thid + 2) - 1;

			uint t = s_counts[ai];
			s_counts[ai] = s_counts[bi];
			s_counts[bi] += t;
		}
	}
	barrier();
	memoryBarrierShared();

	// Make sure the total is saved as well
	if (thid == 0)
	{
		s_total += s_counts[n - 1];
		terrain_buffer.data[node_index].new_points_count = min(s_total, num_new_points);
	}

	// Write points to output storage buffer
	const uint base_offset = s_counts[thid];
	for (uint i = 0; i < new_point_count && base_offset + i < num_new_points; ++i)
	{
		//output_data.points[base_offset + i] = new_points[i];
		terrain_buffer.data[node_index].new_points[base_offset + i] = vec4(new_points[i].x, -terrain(new_points[i].xy), new_points[i].y, 1.0);
	}
}