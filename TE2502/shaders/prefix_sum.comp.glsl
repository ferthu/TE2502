#version 450 core

#define WORK_GROUP_SIZE 1024
layout(local_size_x = WORK_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

//// INPUT
layout(set = 0, binding = 1) buffer point_counts_t
{
	uint counts[];
} point_counts;

layout(push_constant) uniform frame_data_t
{
	mat4 camera_vp;
	mat4 ray_march_view;
	vec4 position;
	int dir_count;
} frame_data;


// INPUT/OUPUT
layout(set = 0, binding = 2) buffer output_data_t
{
	uint vertex_count;
	uint instance_count;
	uint first_vertex;
	uint first_instance;
	vec4 points[];
} output_data;




#define POINTS_PER_DIR 5
shared uint temp[WORK_GROUP_SIZE * 2 + 1]; 

//#define NUM_BANKS 16
//#define LOG_NUM_BANKS 4
//#define CONFLICT_FREE_OFFSET(n) \
//    ((n) >> NUM_BANKS + (n) >> (2 * LOG_NUM_BANKS))

void main(void)
{
	uint thid = gl_GlobalInvocationID.x;

	if (thid * 2 >= frame_data.dir_count)
		return;

	//// PREFIX SUM
	int n = frame_data.dir_count;

	int offset = 1;

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



	temp[thid] = point_counts.counts[thid]; // load input into shared memory
	temp[n / 2 + thid] = point_counts.counts[n / 2 + thid];

	barrier();
	memoryBarrierShared();

	uint total = 0;
	if (thid == 0)
		total = temp[n - 1];

	for (int d = n >> 1; d > 0; d >>= 1)                    // build sum in place up the tree
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
	if (thid == 0) { temp[n - 1] = 0; } // clear the last element
	for (int d = 1; d < n; d *= 2) // traverse down tree & build scan
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

	// Make sure the total is saved as well
	if (thid == 0)
	{
		total += temp[n - 1];
		temp[n] = total;

		output_data.vertex_count = total;
		output_data.instance_count = 1;
		output_data.first_vertex = 0;
		output_data.first_instance = 0;
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
}