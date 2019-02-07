#version 450 core

#define WORK_GROUP_SIZE 1024

layout(local_size_x = WORK_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

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

	uint vertex_count;
	uint new_points_count;
	uint pad[2];

	uint indices[99995];
	vec4 positions[10000];
	Triangle triangles[33331];
	vec4 new_points[11000];
};

layout(set = 0, binding = 0) buffer terrain_buffer_t
{
	terrain_data_t data[100];
} terrain_buffer;


struct Edge
{
	uint p1;
	uint p2;
};

shared uint edge_count;
shared Edge s_edges[40 * 3];

void main2()
{}

void main(void)
{
	if (terrain_buffer.data[frame_data.node_index].new_points_count == 0)
		return;

	uint thid = gl_GlobalInvocationID.x;

	if (thid == 0)
		edge_count = 0;
	barrier();
	memoryBarrierShared();

	uint new_point_index = 0;  // TEMPORARY

	vec4 current_point = terrain_buffer.data[frame_data.node_index].new_points[new_point_index];

	uint triangle_count = terrain_buffer.data[frame_data.node_index].index_count / 3;

	uint i = gl_GlobalInvocationID.x;
	while (i < triangle_count)
	{
		vec2 circumcentre = terrain_buffer.data[frame_data.node_index].triangles[i].circumcentre;
		float circumradius = terrain_buffer.data[frame_data.node_index].triangles[i].circumradius;

		float dx = current_point.x - circumcentre.x;
		float dy = current_point.z - circumcentre.y;
		if (dx * dx + dy * dy < circumradius)
		{
			// Add triangle edges to edge buffer
			uint ec = atomicAdd(edge_count, 3);
			s_edges[ec + 0].p1 = terrain_buffer.data[frame_data.node_index].indices[i * 3 + 0];
			s_edges[ec + 0].p2 = terrain_buffer.data[frame_data.node_index].indices[i * 3 + 1];
			s_edges[ec + 1].p1 = terrain_buffer.data[frame_data.node_index].indices[i * 3 + 1];
			s_edges[ec + 1].p2 = terrain_buffer.data[frame_data.node_index].indices[i * 3 + 2];
			s_edges[ec + 2].p1 = terrain_buffer.data[frame_data.node_index].indices[i * 3 + 2];
			s_edges[ec + 2].p2 = terrain_buffer.data[frame_data.node_index].indices[i * 3 + 0];

			//atomicAdd(s_edges[ec + 0].p1, terrain_buffer.data[frame_data.node_index].indices[i * 3 + 0]);
			//atomicAdd(s_edges[ec + 0].p2, terrain_buffer.data[frame_data.node_index].indices[i * 3 + 1]);
			//atomicAdd(s_edges[ec + 1].p1, terrain_buffer.data[frame_data.node_index].indices[i * 3 + 1]);
			//atomicAdd(s_edges[ec + 1].p2, terrain_buffer.data[frame_data.node_index].indices[i * 3 + 2]);
			//atomicAdd(s_edges[ec + 2].p1, terrain_buffer.data[frame_data.node_index].indices[i * 3 + 2]);
			//atomicAdd(s_edges[ec + 2].p2, terrain_buffer.data[frame_data.node_index].indices[i * 3 + 0]);

			// Remove triangle from triangle list
			// TODO...
		}
		i += WORK_GROUP_SIZE;
	}

	barrier();
	memoryBarrierShared();
	memoryBarrierBuffer();

	if (thid == 0)
	{
		// Delete all doubly specified edges from edge buffer (this leaves the edges of the enclosing polygon only)
		for (uint i = 0; i < edge_count; ++i)  // Optimize! by splitting out to multiple threads
		{
			for (uint j = 0; j < edge_count; ++j)
			{
				if (i != j && s_edges[i].p1 == s_edges[j].p1 && s_edges[i].p2 == s_edges[j].p2)
				{
					s_edges[i].p1 = ~0u;
					s_edges[j].p1 = ~0u;
				}
			}
		}
		
		uint vertex_count = terrain_buffer.data[frame_data.node_index].vertex_count;
		terrain_buffer.data[frame_data.node_index].positions[vertex_count] = current_point;

		// Add to the triangle list all triangles formed between the point and the edges of the enclosing polygon
		uint index_count = terrain_buffer.data[frame_data.node_index].index_count;
		for (uint i = 0; i < edge_count; ++i)
		{
			if (s_edges[i].p1 != ~0u)
			{
				// TODO: verify winding order
				terrain_buffer.data[frame_data.node_index].indices[index_count + 0] = s_edges[i].p1;
				terrain_buffer.data[frame_data.node_index].indices[index_count + 1] = s_edges[i].p2;
				terrain_buffer.data[frame_data.node_index].indices[index_count + 2] = vertex_count;

				index_count += 3;
			}
		}
		++vertex_count;
		terrain_buffer.data[frame_data.node_index].vertex_count = vertex_count;
		terrain_buffer.data[frame_data.node_index].index_count = index_count;

		terrain_buffer.data[frame_data.node_index].new_points_count = 0;
	}
}