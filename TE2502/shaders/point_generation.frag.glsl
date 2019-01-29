#version 450 core

//layout(set = 0, binding = 0) buffer input_data_t
//{
//	vec3 dirs[];
//} input_data;
//
//layout(location = 0) out vec4 out_color;
//
//void main(void)
//{
//	out_color = vec4(1, 0, 0, 1);
//}


layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
	outColor = vec4(fragColor, 1.0);
}