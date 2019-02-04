#version 450 core

layout(location = 0) in vec4 pos;

layout(location = 0) out vec4 out_color;

void main() {
	out_color = vec4(mod(pos.x, 200) / 200.0, -pos.y / 70, mod(pos.z, 200) / 200.0, pos.w);
}