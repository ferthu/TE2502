#version 450 core

layout (set=0, binding=0, rgba8) uniform image2D image;
layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout (push_constant) uniform frame_data_t
{
	mat4 camera_vp;
	float screen_width;
	float screen_height;
} frame_data;

void main (void)
{
	imageStore(image, ivec2(gl_GlobalInvocationID.xy), 
		vec4(float(gl_GlobalInvocationID.x) / frame_data.screen_width, 
		float(gl_GlobalInvocationID.y) / frame_data.screen_height, 1.0, 1.0));
}