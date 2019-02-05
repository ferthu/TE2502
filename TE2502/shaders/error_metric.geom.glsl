#version 450 core

layout(triangles) in;

layout(triangle_strip, max_vertices = 3) out;
layout(location = 0) out vec3 color;

layout(push_constant) uniform frame_data_t
{
	mat4 camera_vp;
	vec4 camera_pos;
} frame_data;

void main() {
	vec4 pos[3];

	for (uint i = 0; i < 3; i++)
	{
		pos[i] = frame_data.camera_vp * gl_in[i].gl_Position;
		pos[i] /= pos[i].w;
	}

	// Calculate area

	// a, b, c is triangle side lengths
	float a = length(pos[0].xy - pos[1].xy);
	float b = length(pos[0].xy - pos[2].xy);
	float c = length(pos[1].xy - pos[2].xy);

	// s is semiperimeter
	float s = (a + b + c) * 0.5;

	float area = sqrt(s * (s - a) * (s - b) * (s - c));

	for (uint i = 0; i < 3; i++)
	{
		gl_Position = pos[i];
		color = vec3(area * 0.01, 0.0, 0.0);
		
		EmitVertex();
	}

	EndPrimitive();
}