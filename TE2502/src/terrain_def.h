#ifdef __cplusplus
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
using namespace glm;

float step(float edge, float x)
{
	return (x < edge) ? 0.0f : 1.0f;
}

#else
#version 450 core
#endif

// INTERFACE:
// THESE FUNCTIONS NEED TO BE DEFINED

// Given a camera position (ray origin) and ray direction, calculate the color for that ray
vec3 calc_ray_color(vec3 ro, vec3 rd);
// Given a position on the plane, return the height for that position
float terrain(vec2 p);




const float max_view_dist = 350.f;

vec2 add = vec2(1.0, 0.0);
#define HASHSCALE1 .1031f

const mat2 rotate2D = mat2(1.3623, 1.7531, -1.7131, 1.4623);

//// Low-res stuff
float hash12(vec2 p)
{
	vec3 p3 = fract(vec3(p.x, p.y, p.x) * HASHSCALE1);
	p3 += dot(p3, vec3(p3.y, p3.z, p3.x) + 19.19f);
	return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 x)
{
	vec2 p = floor(x);
	vec2 f = fract(x);
	f = f * f *(3.0f - 2.0f * f);

	float res = mix(mix(hash12(p), hash12(p + vec2(add.x, add.y)), f.x),
		mix(hash12(p + vec2(add.y, add.x)), hash12(p + vec2(add.x, add.x)), f.x), f.y);
	return res;
}

float terrain(vec2 p)
{
	vec2 pos = p * 0.05f;
	float w = (noise(pos*.25f)*0.75f + .15f);
	w = 66.0 * w * w;
	vec2 dxy = vec2(0.0, 0.0);
	float f = .0;
	for (int i = 0; i < 5; i++)
	{
		f += w * noise(pos);
		w = -w * 0.4;
		pos = rotate2D * pos;
	}
	float ff = noise(pos*.002f);

	f += pow(abs(ff), 5.0f)*275.f - 5.0f;
	return f;
}

float height_to_surface(vec3 p)
{
	float h = terrain(vec2(p.x, p.z));

	return -p.y - h;
}

///////

//// High-res stuff
#define HASHSCALE3 vec3(.1031, .1030, .0973)
#define HASHSCALE4 vec4(1031, .1030, .0973, .1099)

vec2 hash22(vec2 p)
{
	vec3 p3 = fract(vec3(p.x, p.y, p.x) * HASHSCALE3);
	p3 += dot(p3, vec3(p3.y, p3.z, p3.x) + 19.19f);
	return fract((vec2(p3.x, p3.x) + vec2(p3.y, p3.z))*vec2(p3.z, p3.y));
}

vec2 noise2(vec2 x)
{
	vec2 p = floor(x);
	vec2 f = fract(x);
	f = f * f *(3.0f - 2.0f*f);
	float n = p.x + p.y * 57.0f;
	vec2 res = mix(mix(hash22(p), hash22(p + vec2(add.x, add.y)), f.x),
		mix(hash22(p + vec2(add.y, add.x)), hash22(p + vec2(add.x, add.x)), f.x), f.y);
	return res;
}

//--------------------------------------------------------------------------
// High def version only used for grabbing normal information.
float terrain2(vec2 p)
{
	// There's some real magic numbers in here! 
	// The noise calls add large mountain ranges for more variation over distances...
	vec2 pos = p * 0.05f;
	float w = (noise(pos*.25f)*0.75f + .15f);
	w = 66.0 * w * w;
	vec2 dxy = vec2(0.0, 0.0);
	float f = .0;
	for (int i = 0; i < 5; i++)
	{
		f += w * noise(pos);
		w = -w * 0.4;	//...Flip negative and positive for varition	   
		pos = rotate2D * pos;
	}
	float ff = noise(pos*.002f);
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

///////


//// Coloring

vec3 sun_light = normalize(vec3(0.4, 0.4, 0.48));
vec3 sun_color = vec3(1.0, .9, .83);
float specular = 0.0;
float ambient;

// Calculate sun light...
vec3 do_lighting(vec3 mat, vec3 pos, vec3 normal, vec3 eye_dir, float dis)
{
	float h = dot(sun_light, normal);
	float c = max(h, 0.0f) + ambient;
	mat = mat * sun_color * c;
	// Specular...
	if (h > 0.0)
	{
		vec3 r = reflect(sun_light, normal);
		float spec_amount = pow(max(dot(r, normalize(eye_dir)), 0.0f), 3.0f)*specular;
		mat = mix(mat, sun_color, spec_amount);
	}
	return mat;
}

vec3 get_sky(vec3 rd)
{
	//	float sunAmount = max( dot( rd, sun_light), 0.0 );
	//	float v = pow(1.0-max(-rd.y,0.0),5.)*.5;
	//	vec3  sky = vec3(v*sun_color.x*0.4+0.18, v*sun_color.y*0.4+0.22, v*sun_color.z*0.4+.4);
	//	// Wide glare effect...
	//	sky = sky + sun_color * pow(sunAmount, 6.5)*.32;
	//	// Actual sun...
	//	sky = sky+ sun_color * min(pow(sunAmount, 1150.0), .3)*.65;
	//	return sky;
	return vec3(0.1, 0.15, 0.3);
}

// Merge mountains into the sky background for correct disappearance...
vec3 apply_fog(vec3  rgb, float dis, vec3 dir)
{
	float fogAmount = exp(-dis * 0.00005);
	return mix(get_sky(dir), rgb, fogAmount);
}

// 
vec3 surface_color(vec3 pos, vec3 cam_pos, float dist)
{
	vec3 mat;
	specular = .0;
	ambient = .1;

	float p = min(.3, .0005 + .00005 * dist*dist);
	vec2 pos2 = vec2(pos.x, pos.z);
	vec3 normal = vec3(0.0, terrain2(pos2), 0.0);
	vec3 v2 = normal - vec3(p, terrain2(pos2 + vec2(p, 0.0)), 0.0);
	vec3 v3 = normal - vec3(0.0, terrain2(pos2 + vec2(0.0, -p)), -p);
	normal = cross(v2, v3);
	normal = normalize(normal);

	vec3 dir = normalize(pos - cam_pos);

	vec3 mat_pos = pos * 2.0f;// ... I had change scale halfway though, this lazy multiply allow me to keep the graphic scales I had

	float f = clamp(noise(vec2(mat_pos.x, mat_pos.z)*.05f), 0.0f, 1.0f);//*10.8;
	f += noise(vec2(mat_pos.x, mat_pos.z)*.1f + vec2(normal.y, normal.z)*1.08f)*.85f;
	f *= .55;
	vec3 m = mix(vec3(.63*f + .2, .7*f + .1, .7*f + .1), vec3(f*.43 + .1, f*.3 + .2, f*.35 + .1), f*.65);
	mat = m * vec3(f*m.x + .36, f*m.y + .30, f*m.z + .28);
	// Should have used smoothstep to add colours, but left it using 'if' for sanity...
	if (normal.y < .5)
	{
		float v = normal.y;
		float c = (.5 - normal.y) * 4.0;
		c = clamp(c*c, 0.1f, 1.0f);
		f = noise(vec2(mat_pos.x*.09f, mat_pos.z*.095f + vec2(mat_pos.y, mat_pos.y)*0.15f));
		f += noise(vec2(mat_pos.x*2.233, mat_pos.z*2.23))*0.5;
		mat = mix(mat, vec3(.4*f), c);
		specular += .1;
	}

	// Grass. Use the normal to decide when to plonk grass down...
	if (normal.y > .65)
	{
		m = vec3(noise(vec2(mat_pos.x, mat_pos.z)*.023f)*.5 + .15, noise(vec2(mat_pos.x, mat_pos.z)*.03f)*.6 + .25, 0.0);
		m *= (normal.y - 0.65)*.6;
		mat = mix(mat, m, clamp((normal.y - .65)*1.3 * (45.35 - mat_pos.y)*0.1, 0.0, 1.0));
	}

	// Snow topped mountains...
	if (mat_pos.y > 80.0 && normal.y > .42)
	{
		float snow = clamp((mat_pos.y - 80.0 - noise(vec2(mat_pos.x, mat_pos.z) * .1f)*28.0) * 0.035, 0.0, 1.0);
		mat = mix(mat, vec3(.7, .7, .8), snow);
		specular += snow;
		ambient += snow * .3;
	}

	const float dis_sqrd = dist * dist;// Squaring it gives better distance scales.
	do_lighting(mat, pos, normal, dir, dis_sqrd);

	mat = apply_fog(mat, dis_sqrd, dir);

	return mat;
}

// Some would say, most of the magic is done in post! :D
vec3 post_effects(vec3 rgb)
{
	return (1.0f - exp(-rgb * 6.0f)) * 1.0024f;
}

////////////////


#pragma region ray_march
//--------------------------------------------------------------------------
float binary_subdivision(vec3 ro, vec3 rd, vec2 t, int divisions)
{
	// Home in on the surface by dividing by two and split...
	float halfway_t;

	for (int i = 0; i < divisions; i++)
	{
		halfway_t = dot(t, vec2(.5));
		float d = height_to_surface(ro + halfway_t * rd);
		t = mix(vec2(t.x, halfway_t), vec2(halfway_t, t.y), step(0.5, d));
	}
	return halfway_t;
}

//--------------------------------------------------------------------------
float ray_march(vec3 ro, vec3 rd)
{
	float t = 0.01;
	float oldT = 0.0;
	float delta = 0.0;
	vec2 distances;
	while (t < 350)
	{
		//if (fin || t > 14000.0) break;
		vec3 p = ro + t * rd;
		float h = height_to_surface(p); // ...Get this positions height mapping.
		// Are we inside, and close enough to fudge a hit?...
		if (h < 0.5)
		{
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
	return binary_subdivision(ro, rd, distances, 10);
}


vec3 calc_ray_color(vec3 ro, vec3 rd)
{
	vec3 color;
	float dist = ray_march(ro, rd);
	if (dist > max_view_dist)
		color = get_sky(rd);
	else
	{
		vec3 world_pos = ro + rd * dist;
		color = surface_color(world_pos, ro, dist);
	}
	color = post_effects(color);
	return color;
}

#pragma endregion
