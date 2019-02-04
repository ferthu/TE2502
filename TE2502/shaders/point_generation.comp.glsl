#version 450 core

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform input_data
{
	vec3 dirs[];
};
layout(push_constant) uniform frame_data_t
{
	mat4 camera_vp;
	vec4 pos;
} frame_data;

layout(set = 0, binding = 1) buffer output_data_t
{
	uint vertex_count;
	uint instance_count;
	uint first_vertex;
	uint first_instance;
	vec3 points[];
} output_data;

void main(void)
{
	output_data.points[gl_GlobalInvocationID.x] = vec3(0, 1, 0);
	if (gl_GlobalInvocationID.x == 0)
	{
		output_data.vertex_count = 32;
		output_data.instance_count = 1;
		output_data.first_vertex = 0;
		output_data.first_instance = 0;
	}
}