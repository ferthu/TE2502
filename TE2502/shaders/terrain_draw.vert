#version 450 core

layout(location = 0) in vec4 pos;

layout(location = 0) out vec3 world_pos;

layout(push_constant) uniform frame_data_t
{
	mat4 camera_vp;
	vec4 camera_pos;
	vec2 min;
	vec2 max;
	uint buffer_slot;
} frame_data;

void main() {
	gl_Position = frame_data.camera_vp * vec4(pos.xyz, 1.0f);
	world_pos = pos.xyz;
}