#version 450 core

#define WORK_GROUP_SIZE 1024
layout(local_size_x = WORK_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

const uint num_indices = TERRAIN_GENERATE_NUM_INDICES;
const uint num_vertices = TERRAIN_GENERATE_NUM_VERTICES;
const uint num_nodes = TERRAIN_GENERATE_NUM_NODES;
const uint num_new_points = TRIANGULATE_MAX_NEW_POINTS;
const uint quadtree_levels = QUADTREE_LEVELS;

// INPUT
layout(set = 0, binding = 1) buffer point_counts_t
{
	uint counts[];
} point_counts;

// INPUT/OUPUT
layout(set = 0, binding = 2) buffer output_data_t
{
	vec4 points[];
} output_data;

layout(push_constant) uniform frame_data_t
{
	mat4 camera_vp;
	mat4 ray_march_view;
	vec4 position;
	uint dir_count;
	uint power2_dir_count;
} frame_data;

// OUTPUT

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

layout(set = 0, binding = 3) buffer terrain_buffer_t
{
	uint quadtree_index_map[aligned_quadtree_index_num - 4];
	vec2 quadtree_min;
	vec2 quadtree_max;
	terrain_data_t data[num_nodes];
} terrain_buffer;




#define POINTS_PER_DIR 5
shared uint temp[WORK_GROUP_SIZE * 2 + 1]; 
shared uint s_total;

//#define NUM_BANKS 16
//#define LOG_NUM_BANKS 4
//#define CONFLICT_FREE_OFFSET(n) ((n) >> NUM_BANKS + (n) >> (2 * LOG_NUM_BANKS))

void main(void)
{
	uint thid = gl_GlobalInvocationID.x;
	uint n = frame_data.dir_count;
	uint m = frame_data.power2_dir_count;

	if (thid * 2 >= m)
		return;

	//// PREFIX SUM

	int offset = 1;

	{
		//uint ai = thid;
		//uint bi = thid + (n / 2);
		//uint bankOffsetA = CONFLICT_FREE_OFFSET(ai);
		//uint bankOffsetB = CONFLICT_FREE_OFFSET(bi);
		//temp[ai + bankOffsetA] = point_counts.counts[ai];
		//temp[bi + bankOffsetB] = point_counts.counts[bi];

		//for (int d = n >> 1; d > 0; d >>= 1)  // Build sum in place up the tree
		//{
		//	memoryBarrierShared(); 
		//	barrier();

		//	if (thid < d)
		//	{
		//		uint ai = offset * (2 * thid + 1) - 1;
		//		uint bi = offset * (2 * thid + 2) - 1;
		//		ai += CONFLICT_FREE_OFFSET(ai);
		//		bi += CONFLICT_FREE_OFFSET(bi);

		//		temp[bi] += temp[ai];
		//	}
		//	offset *= 2;
		//}

		//if (thid == 0) { temp[n - 1 + CONFLICT_FREE_OFFSET(n - 1)] = 0; }

		//for (int d = 1; d < n; d *= 2)  // Traverse down tree & build scan
		//{
		//	offset >>= 1;
		//	memoryBarrierShared();
		//	barrier();
		//	if (thid < d)
		//	{
		//		uint ai = offset * (2 * thid + 1) - 1;
		//		uint bi = offset * (2 * thid + 2) - 1;
		//		ai += CONFLICT_FREE_OFFSET(ai);
		//		bi += CONFLICT_FREE_OFFSET(bi);

		//		uint t = temp[ai];
		//		temp[ai] = temp[bi];
		//		temp[bi] += t;
		//	}
		//}

		//memoryBarrierShared();
		//barrier();

		//point_counts.counts[ai] = temp[ai + bankOffsetA]; // Optimize!!!
		//point_counts.counts[bi] = temp[bi + bankOffsetB]; // Optimize!!!

	}

	// Load input into shared memory
	temp[thid] = point_counts.counts[thid]; 
	temp[m / 2 + thid] = point_counts.counts[m / 2 + thid];

	barrier();
	memoryBarrierShared();

	if (thid == 0)
		s_total = temp[n - 1];
	barrier();
	memoryBarrierShared();

	for (uint d = m >> 1; d > 0; d >>= 1) // Build sum in place up the tree
	{
		barrier();
		memoryBarrierShared();
		if (thid < d)
		{
			uint ai = offset * (2 * thid + 1) - 1;
			uint bi = offset * (2 * thid + 2) - 1;
			temp[bi] += temp[ai];
		}
		offset *= 2;
	}
	if (thid == 0) { temp[m - 1] = 0; } // Clear the last element
	for (int d = 1; d < m; d *= 2) // Traverse down tree & build scan
	{
		offset >>= 1;
		barrier();
		memoryBarrierShared();
		if (thid < d)
		{
			uint ai = offset * (2 * thid + 1) - 1;
			uint bi = offset * (2 * thid + 2) - 1;

			uint t = temp[ai];
			temp[ai] = temp[bi];
			temp[bi] += t;
		}
	}
	barrier();
	memoryBarrierShared();

	point_counts.counts[thid] = temp[thid];
	point_counts.counts[m / 2 + thid] = temp[m / 2 + thid];

	if (thid * 2 >= n)
		return;

	// Make sure the total is saved as well
	if (thid == 0)
	{
		s_total += temp[n - 1];
		temp[n] = s_total;
	}

	barrier();
	memoryBarrierShared();

	//// WRITE PACKED POINTS TO OUTPUT

	// Save some points locally
	uint local_point_count = 0;
	vec4 local_points[2 * POINTS_PER_DIR];
	uint count = temp[thid * 2 + 1] - temp[thid * 2 + 0];

	for (uint i = 0; i < count; ++i)
	{
		local_points[i] = output_data.points[thid * POINTS_PER_DIR * 2 + i];  // Optimize global reads to get memory burst?
	}

	local_point_count = count;
	count = temp[thid * 2 + 2] - temp[thid * 2 + 1];
	for (uint i = 0; i < count; ++i)
	{
		local_points[local_point_count + i] = output_data.points[thid * POINTS_PER_DIR * 2 + POINTS_PER_DIR + i];
	}
	local_point_count += count;

	barrier();
	memoryBarrierShared();

	// Write points to output storage buffer
	for (uint i = 0; i < local_point_count; ++i)
	{
		output_data.points[temp[thid * 2] + i] = local_points[i];
	}

	barrier();
	memoryBarrierShared();
	memoryBarrierBuffer();

	// Add points to correct node
	uint i = thid;
	while (i < s_total)
	{
		vec4 pos = output_data.points[i];
		for (uint n = 0; n < num_nodes; ++n)
		{
			vec2 min = terrain_buffer.data[n].min;
			vec2 max = terrain_buffer.data[n].max;
			if (pos.x > min.x && 
				pos.x < max.x && 
				pos.z > min.y && 
				pos.z < max.y)
			{
				uint index = atomicAdd(terrain_buffer.data[n].new_points_count, 1);  // TODO: Optimize away atomicAdd?
				terrain_buffer.data[n].new_points[index] = pos;
			}
		}

		i += WORK_GROUP_SIZE;
	}
}