#version 450 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 col;

layout(push_constant) uniform frame_data_t
{
	mat4 vp;
} frame_data;

layout(location = 0) out vec3 color;

void main() {
	gl_Position = frame_data.vp *  vec4(pos, 1.0);
	color = col;
}