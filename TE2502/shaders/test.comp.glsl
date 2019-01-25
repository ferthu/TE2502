#version 450 core

layout (set=0, binding=0, rgba8) uniform image2D image;
layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

void main (void)
{
	imageStore(image, ivec2(gl_GlobalInvocationID.xy), vec4(float(gl_GlobalInvocationID.x) / 1080.0, float(gl_GlobalInvocationID.y) / 720.0, 0.0, 1.0));
}