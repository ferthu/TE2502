#version 450 core


layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

struct terrain_data_t
{
	uint    index_count;
	uint    instance_count;
	uint    first_index;
	int     vertex_offset;
	uint    first_instance;
	uint	indices[303];	
	vec4	positions[1200];
};

layout(set = 0, binding = 0) buffer terrain_buffer_t
{
	terrain_data_t data[100];
} terrain_buffer;

layout(push_constant) uniform frame_data_t
{
	mat4 camera_vp;
	vec2 min;
	vec2 max;
	uint buffer_slot;
} frame_data;

void main(void)
{
	terrain_buffer.data[frame_data.buffer_slot].index_count = 6;
	terrain_buffer.data[frame_data.buffer_slot].instance_count = 1;
	terrain_buffer.data[frame_data.buffer_slot].first_index = 0;
	terrain_buffer.data[frame_data.buffer_slot].vertex_offset = 0;
	terrain_buffer.data[frame_data.buffer_slot].first_instance = 0;

	terrain_buffer.data[frame_data.buffer_slot].positions[0] = vec4(frame_data.min.x, 0.0, frame_data.min.y, 1.0);
	terrain_buffer.data[frame_data.buffer_slot].positions[1] = vec4(frame_data.min.x, 0.0, frame_data.max.y, 1.0);
	terrain_buffer.data[frame_data.buffer_slot].positions[2] = vec4(frame_data.max.x, 0.0, frame_data.min.y, 1.0);
	terrain_buffer.data[frame_data.buffer_slot].positions[3] = vec4(frame_data.max.x, 0.0, frame_data.max.y, 1.0);

	terrain_buffer.data[frame_data.buffer_slot].indices[0] = 0;
	terrain_buffer.data[frame_data.buffer_slot].indices[1] = 1;
	terrain_buffer.data[frame_data.buffer_slot].indices[2] = 2;

	terrain_buffer.data[frame_data.buffer_slot].indices[3] = 1;
	terrain_buffer.data[frame_data.buffer_slot].indices[4] = 3;
	terrain_buffer.data[frame_data.buffer_slot].indices[5] = 2;
}