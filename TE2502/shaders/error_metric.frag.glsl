#version 450 core

layout(location = 0) in float area;
layout(location = 1) in vec3 world_pos;

layout(push_constant) uniform frame_data_t
{
	mat4 camera_vp;
	vec4 camera_pos;
	vec2 screen_size;
	float area_multiplier;
	float curvature_multiplier;
} frame_data;

layout(location = 0) out vec4 out_color;

const mat2 rotate2D = mat2(1.3623, 1.7531, -1.7131, 1.4623);
#define HASHSCALE1 .1031
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

float Terrain2(in vec2 p)
{
	// There's some real magic numbers in here! 
	// The Noise calls add large mountain ranges for more variation over distances...
	vec2 pos = p * 0.05;
	float w = (Noise(pos*.25)*0.75 + .15);
	w = 66.0 * w * w;
	vec2 dxy = vec2(0.0, 0.0);
	float f = .0;
	for (int i = 0; i < 5; i++)
	{
		f += w * Noise(pos);
		w = -w * 0.4;	//...Flip negative and positive for varition	   
		pos = rotate2D * pos;
	}
	float ff = Noise(pos*.002);
	f += pow(abs(ff), 5.0)*275. - 5.0;


	// That's the last of the low resolution, now go down further for the Normal data...
	for (int i = 0; i < 6; i++)
	{
		f += w * Noise(pos);
		w = -w * 0.4;
		pos = rotate2D * pos;
	}


	return f;
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

void main()
{
	const float pi = 3.1415;

	float camera_distance = distance(world_pos, frame_data.camera_pos.xyz);

	float sample_step = 5.5 + pow(camera_distance * 0.5, 0.6);
	const float gaussian_width = 1.0;
	const int filter_radius = 2;	// Side length of grid is filter_radius * 2 + 1
	const int filter_side = filter_radius * 2 + 1;
	float log_filter[filter_side * filter_side];

	/////////////////////////////////////////
	float sum = 0.0;

	for (int x = -filter_radius; x <= filter_radius; x++)
	{
		for (int y = -filter_radius; y <= filter_radius; y++)
		{
			// https://homepages.inf.ed.ac.uk/rbf/HIPR2/log.htm
			float t = -((x * x + y * y) / (2.0 * gaussian_width * gaussian_width));
			float log = -(1 / (pi * pow(gaussian_width, 4.0))) * (1.0 + t) * exp(t);

			log_filter[(y + filter_radius) * filter_side + (x + filter_radius)] = log;
			sum += log;
		}
	}

	// Normalize filter
	float correction = 1.0 / sum;
	for (uint i = 0; i < filter_side * filter_side; i++)
	{
		log_filter[i] *= correction;
	}

	float curvature = 0.0;

	for (int x = -filter_radius; x <= filter_radius; x++)
	{
		for (int y = -filter_radius; y <= filter_radius; y++)
		{
			curvature += Terrain(world_pos.xz + vec2(sample_step * x, sample_step * y)) * log_filter[(y + filter_radius) * filter_side + (x + filter_radius)];
		}
	}

	// Normalize for height
	curvature -= Terrain(world_pos.xz);

	/////////////////////////////////////////

	out_color = vec4(area / 1100.0, pow(abs(curvature * frame_data.curvature_multiplier), 1.3), 0.0, 1.0);

	out_color = pow(out_color, vec4((1.0 / 2.2).xxx, 1.0));
}