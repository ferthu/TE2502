#version 450 core

layout(set = 0, binding = 0) uniform dirs
{
	vec3 d[];
};
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout(push_constant) uniform frame_data_t
{
	mat4 camera_vp;
	vec4 pos;
} frame_data;

layout(set = 0, binding = 1) buffer points
{
	uint vertex_count;
	uint instance_count;
	uint first_vertex;
	uint first_instance;
	vec3 p[];
};

void main(void)
{

}