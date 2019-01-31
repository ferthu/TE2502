// Mountains. By David Hoskins - 2013
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.

#version 450 core

#define WIDTH 1080
#define HEIGHT 720
#define WORKGROUP_SIZE 32
layout(local_size_x = WORKGROUP_SIZE, local_size_y = WORKGROUP_SIZE, local_size_z = 1) in;
//layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout(push_constant) uniform frame_data_t
{
	mat4 view;
	vec4 position;
	vec2 screen_size;
} frame_data;

layout(set = 0, binding = 0, rgba8) uniform image2D image; 



vec3 sun_light = normalize(vec3(0.4, 0.4, 0.48));
vec3 sun_color = vec3(1.0, .9, .83);
float specular = 0.0;
float ambient;
vec2 add = vec2(1.0, 0.0);
#define HASHSCALE1 .1031
#define HASHSCALE3 vec3(.1031, .1030, .0973)
#define HASHSCALE4 vec4(1031, .1030, .0973, .1099)

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
// Calculate sun light...
void do_lighting(inout vec3 mat, in vec3 pos, in vec3 normal, in vec3 eye_dir, in float dis)
{
	float h = dot(sun_light, normal);
	float c = max(h, 0.0) + ambient;
	mat = mat * sun_color * c;
	// Specular...
	if (h > 0.0)
	{
		vec3 R = reflect(sun_light, normal);
		float spec_amount = pow(max(dot(R, normalize(eye_dir)), 0.0), 3.0)*specular;
		mat = mix(mat, sun_color, spec_amount);
	}
}

//--------------------------------------------------------------------------
// Hack the height, position, and normal data to create the coloured landscape
vec3 surface_color(vec3 pos, vec3 normal, float dis)
{
	vec3 mat;
	specular = .0;
	ambient = .1;
	vec3 dir = normalize(pos - frame_data.position.xyz);

	vec3 mat_pos = pos * 2.0;// ... I had change scale halfway though, this lazy multiply allow me to keep the graphic scales I had

	float dis_sqrd = dis * dis;// Squaring it gives better distance scales.

	float f = clamp(noise(mat_pos.xz*.05), 0.0, 1.0);//*10.8;
	f += noise(mat_pos.xz*.1 + normal.yz*1.08)*.85;
	f *= .55;
	vec3 m = mix(vec3(.63*f + .2, .7*f + .1, .7*f + .1), vec3(f*.43 + .1, f*.3 + .2, f*.35 + .1), f*.65);
	mat = m * vec3(f*m.x + .36, f*m.y + .30, f*m.z + .28);
	// Should have used smoothstep to add colours, but left it using 'if' for sanity...
	if (normal.y < .5)
	{
		float v = normal.y;
		float c = (.5 - normal.y) * 4.0;
		c = clamp(c*c, 0.1, 1.0);
		f = noise(vec2(mat_pos.x*.09, mat_pos.z*.095 + mat_pos.yy*0.15));
		f += noise(vec2(mat_pos.x*2.233, mat_pos.z*2.23))*0.5;
		mat = mix(mat, vec3(.4*f), c);
		specular += .1;
	}

	// Grass. Use the normal to decide when to plonk grass down...
	if (normal.y > .65)
	{

		m = vec3(noise(mat_pos.xz*.023)*.5 + .15, noise(mat_pos.xz*.03)*.6 + .25, 0.0);
		m *= (normal.y - 0.65)*.6;
		mat = mix(mat, m, clamp((normal.y - .65)*1.3 * (45.35 - mat_pos.y)*0.1, 0.0, 1.0));
	}

	// Snow topped mountains...
	if (mat_pos.y > 80.0 && normal.y > .42)
	{
		float snow = clamp((mat_pos.y - 80.0 - noise(mat_pos.xz * .1)*28.0) * 0.035, 0.0, 1.0);
		mat = mix(mat, vec3(.7, .7, .8), snow);
		specular += snow;
		ambient += snow * .3;
	}

	do_lighting(mat, pos, normal, dir, dis_sqrd);

	return mat;
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

//--------------------------------------------------------------------------
bool ray_march(in vec3 rO, in vec3 rD, out float resT)
{
	float t = 0.01;
	float oldT = 0.0;
	float delta = 0.0;
	bool fin = false;
	vec2 distances;
	for (int j = 0; j < 350; j++)
	{
		if (fin || t > 1000.0) break;
		vec3 p = rO + t * rD;
		float h = height_to_surface(p); // ...Get this positions height mapping.
		// Are we inside, and close enough to fudge a hit?...
		if (h < 0.5f)
		{
			fin = true;
			distances = vec2(oldT, t);
			break;
		}
		// Delta ray advance - a fudge between the height returned
		// and the distance already travelled.
		// It's a really fiddly compromise between speed and accuracy
		// Too large a step and the tops of ridges get missed.
		delta = max(0.1, 0.2*h) + (t*0.0025);
		oldT = t;
		t += delta;
	}
	if (fin) resT = binary_subdivision(rO, rD, distances, 10);

	return fin;
}

//--------------------------------------------------------------------------
// Some would say, most of the magic is done in post! :D
vec3 post_effects(vec3 rgb)
{
	return (1.0 - exp(-rgb * 6.0)) * 1.0024;;
}

//--------------------------------------------------------------------------
void main(void)
{
	if (gl_GlobalInvocationID.x >= WIDTH || gl_GlobalInvocationID.y >= HEIGHT)
		return;

	vec2 xy = 1.0 + -2.0*gl_GlobalInvocationID.xy / frame_data.screen_size.xy;
	vec2 uv = xy * vec2(frame_data.screen_size.x / frame_data.screen_size.y, 1.0);

	vec3 rd = (frame_data.view * normalize(vec4(uv, 1, 0))).xyz;

	vec3 position = frame_data.position.xyz;

	vec3 color;
	float distance;
	if (!ray_march(position, rd, distance))
	{
		// Missed scene, now just get the sky value...
		color = vec3(0.1, 0.15, 0.3);
	}
	else
	{
		// Get world coordinate of landscape...
		vec3 pos = position + distance * rd;
		// Get normal from sampling the high definition height map
		// Use the distance to sample larger gaps to help stop aliasing...
		float p = min(.3, .0005 + .00005 * distance*distance);
		vec3 nor = vec3(0.0, terrain2(pos.xz), 0.0);
		vec3 v2 = nor - vec3(p, terrain2(pos.xz + vec2(p, 0.0)), 0.0);
		vec3 v3 = nor - vec3(0.0, terrain2(pos.xz + vec2(0.0, -p)), -p);
		nor = cross(v2, v3);
		nor = normalize(nor);

		// Get the colour using all available data...
		color = surface_color(pos, nor, distance);
	}

	color = post_effects(color);

	imageStore(image, ivec2(gl_GlobalInvocationID.xy), vec4(color, 1.0));
}

