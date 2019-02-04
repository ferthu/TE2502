#version 450 core

#define GRID_SIDE 200
#define GROUP_SIZE 32

layout(local_size_x = GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

struct terrain_data_t
{
	uint    index_count;
	uint    instance_count;
	uint    first_index;
	int     vertex_offset;
	uint    first_instance;
	uint	indices[300003];
	vec4	positions[80000];
};

layout(set = 0, binding = 0) buffer terrain_buffer_t
{
	terrain_data_t data[100];
} terrain_buffer;

layout(push_constant) uniform frame_data_t
{
	mat4 camera_vp;
	vec2 min;
	vec2 max;
	uint buffer_slot;
} frame_data;


#define HASHSCALE1 .1031
const mat2 rotate2D = mat2(1.3623, 1.7531, -1.7131, 1.4623);
vec2 add = vec2(1.0, 0.0);

float Hash12(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * HASHSCALE1);
	p3 += dot(p3, p3.yzx + 19.19);
	return fract((p3.x + p3.y) * p3.z);
}

float Noise(in vec2 x)
{
	vec2 p = floor(x);
	vec2 f = fract(x);
	f = f * f*(3.0 - 2.0*f);

	float res = mix(mix(Hash12(p), Hash12(p + add.xy), f.x),
		mix(Hash12(p + add.yx), Hash12(p + add.xx), f.x), f.y);
	return res;
}

float Terrain(in vec2 p)
{
	vec2 pos = p * 0.05;
	float w = (Noise(pos*.25)*0.75 + .15);
	w = 66.0 * w * w;
	vec2 dxy = vec2(0.0, 0.0);
	float f = .0;
	for (int i = 0; i < 5; i++)
	{
		f += w * Noise(pos);
		w = -w * 0.4;
		pos = rotate2D * pos;
	}
	float ff = Noise(pos*.002);

	f += pow(abs(ff), 5.0)*275. - 5.0;
	return f;
}

void main(void)
{
	if (gl_GlobalInvocationID.x == 0)
	{
		terrain_buffer.data[frame_data.buffer_slot].index_count = 6 * (GRID_SIDE) * (GRID_SIDE);
		terrain_buffer.data[frame_data.buffer_slot].instance_count = 1;
		terrain_buffer.data[frame_data.buffer_slot].first_index = 0;
		terrain_buffer.data[frame_data.buffer_slot].vertex_offset = 0;
		terrain_buffer.data[frame_data.buffer_slot].first_instance = 0;
	}

	for (uint i = 0; i < GRID_SIDE * GRID_SIDE / GROUP_SIZE + 1; i++)
	{
		if (gl_GlobalInvocationID.x + i * GROUP_SIZE < GRID_SIDE * GRID_SIDE)
		{
			float x = frame_data.min.x + (((gl_GlobalInvocationID.x + i * GROUP_SIZE) % (GRID_SIDE)) / float(GRID_SIDE - 1)) * (frame_data.max.x - frame_data.min.x);
			float y = frame_data.min.y + float((gl_GlobalInvocationID.x + i * GROUP_SIZE) / GRID_SIDE) / float(GRID_SIDE - 1) * (frame_data.max.y - frame_data.min.y);

			terrain_buffer.data[frame_data.buffer_slot].positions[gl_GlobalInvocationID.x + i * GROUP_SIZE] = vec4(x, -Terrain(vec2(x, y)), y, 1.0);

			if (gl_GlobalInvocationID.x + i * GROUP_SIZE < GRID_SIDE * GRID_SIDE - GRID_SIDE && (gl_GlobalInvocationID.x + i * GROUP_SIZE) % GRID_SIDE != GRID_SIDE - 1)
			{
				terrain_buffer.data[frame_data.buffer_slot].indices[(gl_GlobalInvocationID.x + i * GROUP_SIZE) * 6    ] = gl_GlobalInvocationID.x + i * GROUP_SIZE;
				terrain_buffer.data[frame_data.buffer_slot].indices[(gl_GlobalInvocationID.x + i * GROUP_SIZE) * 6 + 1] = gl_GlobalInvocationID.x + i * GROUP_SIZE + GRID_SIDE + 1;
				terrain_buffer.data[frame_data.buffer_slot].indices[(gl_GlobalInvocationID.x + i * GROUP_SIZE) * 6 + 2] = gl_GlobalInvocationID.x + i * GROUP_SIZE + 1;

				terrain_buffer.data[frame_data.buffer_slot].indices[(gl_GlobalInvocationID.x + i * GROUP_SIZE) * 6 + 3] = gl_GlobalInvocationID.x + i * GROUP_SIZE;
				terrain_buffer.data[frame_data.buffer_slot].indices[(gl_GlobalInvocationID.x + i * GROUP_SIZE) * 6 + 4] = gl_GlobalInvocationID.x + i * GROUP_SIZE + GRID_SIDE;
				terrain_buffer.data[frame_data.buffer_slot].indices[(gl_GlobalInvocationID.x + i * GROUP_SIZE) * 6 + 5] = gl_GlobalInvocationID.x + i * GROUP_SIZE + GRID_SIDE + 1;
			}
		}
	}
}