#version 450 core

layout(location = 0) in vec4 pos;

layout(push_constant) uniform frame_data_t
{
	mat4 vp;
	mat4 ray_march_view;
	vec4 pos;
	uint dir_count;
	uint power2_dir_count;
} frame_data;

layout(location = 0) out vec4 out_pos;

void main(void)
{
	gl_PointSize = 5.0f;
	gl_Position = frame_data.vp * vec4(pos.xyz, 1);
	out_pos = pos;
}
