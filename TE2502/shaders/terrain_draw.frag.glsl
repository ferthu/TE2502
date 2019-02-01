#version 450 core

layout(location = 0) in vec3 world_pos;

layout(location = 0) out vec4 out_color;

void main() {
	float height = -world_pos.y / 200.0;

	out_color = height.xxxx;
}