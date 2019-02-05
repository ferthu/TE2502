#version 450 core

layout(location = 0) in vec4 pos;

layout(push_constant) uniform frame_data_t
{
	mat4 camera_vp;
	vec4 camera_pos;
} frame_data;

void main() {
	gl_Position = vec4(pos.xyz, 1.0);
}