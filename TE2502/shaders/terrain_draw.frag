#version 450 core

#include "../src/terrain_interface.h"

layout(location = 0) in vec3 world_pos;

layout(push_constant) uniform frame_data_t
{
	mat4 camera_vp;
	vec4 camera_pos;
	vec2 min;
	vec2 max;
	uint buffer_slot;
} frame_data;

layout(location = 0) out vec4 out_color;

void main()
{
	vec3 camera_pos = frame_data.camera_pos.xyz;
	float dist = distance(world_pos, camera_pos);

	//float t = (world_pos.y + 100) / 200.0f;
	//out_color = vec4(t, t, t, 1);
	out_color = vec4(post_effects(surface_color(world_pos, camera_pos, dist)), 1.0);
}