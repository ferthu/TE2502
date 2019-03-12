#version 450 core

#include "terrain.include"

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
	float dist = length(world_pos - camera_pos);

	float p = min(.3, .0005 + .00005 * dist*dist);
	vec3 nor = vec3(0.0, terrain2(world_pos.xz), 0.0);
	vec3 v2 = nor - vec3(p, terrain2(world_pos.xz + vec2(p, 0.0)), 0.0);
	vec3 v3 = nor - vec3(0.0, terrain2(world_pos.xz + vec2(0.0, -p)), -p);
	nor = cross(v2, v3);
	nor = normalize(nor);

	out_color = vec4(post_effects(surface_color(world_pos, nor, distance(world_pos, frame_data.camera_pos.xyz), frame_data.camera_pos.xyz)), 1.0);
}