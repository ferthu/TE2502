#version 450 core

#define WORK_GROUP_SIZE 32
layout(local_size_x = WORK_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

//// INPUT
layout(set = 0, binding = 0) buffer input_data_t
{
	vec4 dirs[];
} input_data;

layout(push_constant) uniform frame_data_t
{
	mat4 camera_vp;
	mat4 ray_march_view;
	vec4 position;
	int dir_count;
} frame_data;


//// OUPUT
//layout(set = 0, binding = 1) buffer point_counts_t
//{
//	uint counts[];
//} point_counts;

layout(set = 0, binding = 1) buffer output_data_t
{
	uint vertex_count;
	uint instance_count;
	uint first_vertex;
	uint first_instance;
	vec4 points[];
} output_data;



vec2 add = vec2(1.0, 0.0);
#define HASHSCALE1 .1031
#define HASHSCALE3 vec3(.1031, .1030, .0973)

// This peturbs the fractal positions for each iteration down...
// Helps make nice twisted landscapes...
const mat2 rotate2D = mat2(1.3623, 1.7531, -1.7131, 1.4623);

//  1 out, 2 in...
float hash12(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * HASHSCALE1);
	p3 += dot(p3, p3.yzx + 19.19);
	return fract((p3.x + p3.y) * p3.z);
}
vec2 hash22(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * HASHSCALE3);
	p3 += dot(p3, p3.yzx + 19.19);
	return fract((p3.xx + p3.yz)*p3.zy);

}

float noise(in vec2 x)
{
	vec2 p = floor(x);
	vec2 f = fract(x);
	f = f * f*(3.0 - 2.0*f);

	float res = mix(mix(hash12(p), hash12(p + add.xy), f.x),
		mix(hash12(p + add.yx), hash12(p + add.xx), f.x), f.y);
	return res;
}

vec2 noise2(in vec2 x)
{
	vec2 p = floor(x);
	vec2 f = fract(x);
	f = f * f*(3.0 - 2.0*f);
	float n = p.x + p.y * 57.0;
	vec2 res = mix(mix(hash22(p), hash22(p + add.xy), f.x),
		mix(hash22(p + add.yx), hash22(p + add.xx), f.x), f.y);
	return res;
}


//--------------------------------------------------------------------------
// Low def version for ray-marching through the height field...
// Thanks to IQ for all the noise stuff...

float terrain(in vec2 p)
{
	vec2 pos = p * 0.05;
	float w = (noise(pos*.25)*0.75 + .15);
	w = 66.0 * w * w;
	vec2 dxy = vec2(0.0, 0.0);
	float f = .0;
	for (int i = 0; i < 5; i++)
	{
		f += w * noise(pos);
		w = -w * 0.4;	//...Flip negative and positive for variation
		pos = rotate2D * pos;
	}
	float ff = noise(pos*.002);

	f += pow(abs(ff), 5.0)*275. - 5.0;
	return f;
}

//--------------------------------------------------------------------------
// Map to lower resolution for height field mapping for Scene function...
float height_to_surface(in vec3 p)
{
	float h = terrain(p.xz);

	return -p.y - h;
}

//--------------------------------------------------------------------------
// High def version only used for grabbing normal information.
float terrain2(in vec2 p)
{
	// There's some real magic numbers in here! 
	// The noise calls add large mountain ranges for more variation over distances...
	vec2 pos = p * 0.05;
	float w = (noise(pos*.25)*0.75 + .15);
	w = 66.0 * w * w;
	vec2 dxy = vec2(0.0, 0.0);
	float f = .0;
	for (int i = 0; i < 5; i++)
	{
		f += w * noise(pos);
		w = -w * 0.4;	//...Flip negative and positive for varition	   
		pos = rotate2D * pos;
	}
	float ff = noise(pos*.002);
	f += pow(abs(ff), 5.0)*275. - 5.0;


	// That's the last of the low resolution, now go down further for the Normal data...
	for (int i = 0; i < 6; i++)
	{
		f += w * noise(pos);
		w = -w * 0.4;
		pos = rotate2D * pos;
	}

	return f;
}

//--------------------------------------------------------------------------
float binary_subdivision(in vec3 rO, in vec3 rD, in vec2 t, in int divisions)
{
	// Home in on the surface by dividing by two and split...
	float halfway_t;

	for (int i = 0; i < divisions; i++)
	{
		halfway_t = dot(t, vec2(.5));
		float d = height_to_surface(rO + halfway_t * rD);
		t = mix(vec2(t.x, halfway_t), vec2(halfway_t, t.y), step(0.5, d));
	}
	return halfway_t;
}



#define POINTS_PER_DIR 5
//shared int s_points_found[WORK_GROUP_SIZE * 2];

void main(void)
{
	if (gl_GlobalInvocationID.x >= frame_data.dir_count)
		return;

	////// RAY MARCH
	/*for (int d = 0; d < frame_data.dir_count / WORK_GROUP_SIZE + 1; ++d)
	{*/
		//uint index = d * WORK_GROUP_SIZE + gl_GlobalInvocationID.x;
		//if (index > frame_data.dir_count)
		//	break;

		vec3 ray_dir = (frame_data.ray_march_view * normalize(vec4(input_data.dirs[gl_GlobalInvocationID.x]))).xyz;

		vec3 origin = frame_data.position.xyz;

		for (int i = 0; i < POINTS_PER_DIR; ++i)
		{
			output_data.points[gl_GlobalInvocationID.x * POINTS_PER_DIR + i] = vec4(0, -50, 0, 1);
		}

		int points_found = 0;
		//vec3 points[POINTS_PER_DIR];

		float distance = 0.01;
		float old_distance = 0.0;
		float delta = 0.0;
		vec2 distances;
		float my_sign = 1.0;

		for (int j = 0; j < 350; j++)
		{
			if (points_found == POINTS_PER_DIR || distance > 1000.0)
				break;
			vec3 p = origin + distance * ray_dir;
			float h = height_to_surface(p);
			// Check if close to surface
			if (sign(h - 0.5) != my_sign)
			{
				distances = vec2(old_distance, distance);
				float exact_distance = binary_subdivision(origin, ray_dir, distances, 12);
				vec3 surface_point = origin + exact_distance * ray_dir;
				output_data.points[gl_GlobalInvocationID.x * POINTS_PER_DIR + points_found] = vec4(surface_point, 1);
				++points_found;
				distance += delta * 2;
				my_sign *= -1;
			}

			// Step forward
			delta = max(0.1, 0.2*h*my_sign) + (distance*0.0025);
			old_distance = distance;
			distance += delta;
		}
	//}


	////// PREFIX SUM
	//s_points_found[]
	//barrier();







	
	if (gl_GlobalInvocationID.x == 0)
	{
		output_data.vertex_count = frame_data.dir_count * POINTS_PER_DIR;
		output_data.instance_count = 1;
		output_data.first_vertex = 0;
		output_data.first_instance = 0;
	}
}