#version 450 core

layout(location = 0) in vec4 pos;

layout(push_constant) uniform frame_data_t
{
	mat4 vp;
	mat4 ray_march_view;
	vec4 pos;
	int dir_count;
} frame_data;

void main(void)
{
	gl_PointSize = 5.0f;
	gl_Position = frame_data.vp * vec4(pos.xyz, 1);
}


//layout(location = 0) out vec3 fragColor;
//
//vec2 positions[3] = vec2[](
//	vec2(0.0, -0.5),
//	vec2(0.5, 0.5),
//	vec2(-0.5, 0.5)
//	);
//
//vec3 colors[3] = vec3[](
//	vec3(1.0, 0.0, 0.0),
//	vec3(0.0, 1.0, 0.0),
//	vec3(0.0, 0.0, 1.0)
//	);
//
//void main() {
//	gl_Position = frame_data.vp * vec4(positions[gl_VertexIndex], 0.0, 1.0);
//	fragColor = colors[gl_VertexIndex];
//}