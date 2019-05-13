// Mountains. By David Hoskins - 2013
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.

#ifdef __cplusplus
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
using namespace glm;
#else
#define inline 
#define static 
#endif




const vec2 add = vec2(1.0f, 0.0f);
#define HASHSCALE1 .1031f

const mat2 rotate2D = mat2(1.3623f, 1.7531f, -1.7131f, 1.4623f);

//// Low-res stuff
inline float hash12(vec2 p)
{
	vec3 p3 = fract(vec3(p.x, p.y, p.x) * HASHSCALE1);
	p3 += dot(p3, vec3(p3.y, p3.z, p3.x) + 19.19f);
	return fract((p3.x + p3.y) * p3.z);
}

inline float noise(vec2 x)
{
	vec2 p = floor(x);
	vec2 f = fract(x);
	f = f * f *(3.0f - 2.0f * f);

	float res = mix(mix(hash12(p), hash12(p + vec2(add.x, add.y)), f.x),
		mix(hash12(p + vec2(add.y, add.x)), hash12(p + vec2(add.x, add.x)), f.x), f.y);
	return res;
}

inline float terrain(vec2 p)
{
	vec2 pos = p * 0.05f;
	float w = (noise(pos*.25f)*0.75f + .15f);
	w = 66.0f * w * w;
	vec2 dxy = vec2(0.0f, 0.0f);
	float f = .0f;
	for (int i = 0; i < 5; i++)
	{
		f += w * noise(pos);
		w = -w * 0.4f;
		pos = rotate2D * pos;
	}
	float ff = noise(pos*.002f);

	f += pow(abs(ff), 5.0f)*275.f - 5.0f;
#ifdef __cplusplus
	f += 0.5f;
#endif // __cplusplus

	return f;
}

inline float height_to_surface(vec3 p)
{
	float h = terrain(vec2(p.x, p.z));

	return p.y - h;
}

///////

//// High-res stuff
#define HASHSCALE3 vec3(.1031f, .1030f, .0973f)
#define HASHSCALE4 vec4(1031, .1030f, .0973f, .1099f)

inline vec2 hash22(vec2 p)
{
	vec3 p3 = fract(vec3(p.x, p.y, p.x) * HASHSCALE3);
	p3 += dot(p3, vec3(p3.y, p3.z, p3.x) + 19.19f);
	return fract((vec2(p3.x, p3.x) + vec2(p3.y, p3.z))*vec2(p3.z, p3.y));
}

inline vec2 noise2(vec2 x)
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
inline float terrain2(vec2 p)
{
	// There's some real magic numbers in here! 
	// The noise calls add large mountain ranges for more variation over distances...
	vec2 pos = p * 0.05f;
	float w = (noise(pos*.25f)*0.75f + .15f);
	w = 66.0f * w * w;
	vec2 dxy = vec2(0.0f, 0.0f);
	float f = .0f;
	for (int i = 0; i < 5; i++)
	{
		f += w * noise(pos);
		w = -w * 0.4f;	//...Flip negative and positive for varition	   
		pos = rotate2D * pos;
	}
	float ff = noise(pos*.002f);
	f += pow(abs(ff), 5.0f)*275.f - 5.0f;


	// That's the last of the low resolution, now go down further for the Normal data...
	for (int i = 0; i < 6; i++)
	{
		f += w * noise(pos);
		w = -w * 0.4f;
		pos = rotate2D * pos;
	}

	return f;
}

///////


//// Coloring

const vec3 sun_light = normalize(vec3(0.4f, 0.4f, 0.48f));
const vec3 sun_color = vec3(1.0f, .9f, .83f);
static float specular = 0.0f;
static float ambient;

// Calculate sun light...
inline vec3 do_lighting(vec3 mat, vec3 pos, vec3 normal, vec3 eye_dir, float dis)
{
	float h = dot(sun_light, normal);
	float c = max(h, 0.0f) + ambient;
	mat = mat * sun_color * c;
	// Specular...
	if (h > 0.0f)
	{
		vec3 r = reflect(sun_light, normal);
		float spec_amount = pow(max(dot(r, normalize(eye_dir)), 0.0f), 3.0f)*specular;
		mat = mix(mat, sun_color, spec_amount);
	}
	return mat;
}

inline vec3 get_sky(vec3 rd)
{
	return clear_color;
	float sunAmount = max( dot( rd, sun_light), 0.0f );
	float v = pow(1.0f-max(rd.y,0.0f),5.)*.5f;
	vec3  sky = vec3(v*sun_color.x*0.4f+0.18f, v*sun_color.y*0.4f+0.22f, v*sun_color.z*0.4f+.4f);
	// Wide glare effect...
	sky = sky + sun_color * pow(sunAmount, 6.5f)*.32f;
	// Actual sun...
	sky = sky+ sun_color * min(pow(sunAmount, 1150.0f), .3f)*.65f;
	return sky;
}

// Merge mountains into the sky background for correct disappearance...
inline vec3 apply_fog(vec3 rgb, float dis, vec3 dir)
{
	//float fogAmount = 1.0f - exp(-0.000000000000001f*dis*dis*dis);  // Short
	float fogAmount = 1.0f - exp(-0.0000000000000000003f*dis*dis*dis);  // Far
	return mix(rgb, get_sky(dir), fogAmount);
}

// 
inline vec3 surface_color(vec3 pos, vec3 cam_pos, float dist)
{
	vec3 mat;
	specular = .0f;
	ambient = .1f;

	const float dis_sqrd = dist * dist;// Squaring it gives better distance scales.
	float p = min(.3f, .0005f + .00005f * dis_sqrd);
	vec2 pos2 = vec2(pos.x, pos.z);
	vec3 normal = vec3(0.0f, terrain2(pos2), 0.0f);
	vec3 v2 = normal - vec3(p, terrain2(pos2 + vec2(p, 0.0f)), 0.0f);
	vec3 v3 = normal - vec3(0.0f, terrain2(pos2 + vec2(0.0f, -p)), -p);
	normal = cross(v2, v3);
	normal = normalize(normal);

	vec3 dir = normalize(pos - cam_pos);

	vec3 mat_pos = pos * 2.0f;// ... I had change scale halfway though, this lazy multiply allow me to keep the graphic scales I had

	float f = clamp(noise(vec2(mat_pos.x, mat_pos.z)*.05f), 0.0f, 1.0f);//*10.8f;
	f += noise(vec2(mat_pos.x, mat_pos.z)*.1f + vec2(normal.y, normal.z)*1.08f)*.85f;
	f *= .55f;
	vec3 m = mix(vec3(.63f*f + .2f, .7f*f + .1f, .7f*f + .1f), vec3(f*.43f + .1f, f*.3f + .2f, f*.35f + .1f), f*.65f);
	mat = m * vec3(f*m.x + .36f, f*m.y + .30f, f*m.z + .28f);
	// Should have used smoothstep to add colours, but left it using 'if' for sanity...
	if (normal.y < .5f)
	{
		float v = normal.y;
		float c = (.5f - normal.y) * 4.0f;
		c = clamp(c*c, 0.1f, 1.0f);
		f = noise(vec2(mat_pos.x*.09f, mat_pos.z*.095f + mat_pos.y*0.15f));
		f += noise(vec2(mat_pos.x*2.233f, mat_pos.z*2.23f))*0.5f;
		mat = mix(mat, vec3(.4f*f), c);
		specular += .1f;
	}

	// Grass. Use the normal to decide when to plonk grass down...
	if (normal.y > .65f)
	{
		m = vec3(noise(vec2(mat_pos.x, mat_pos.z)*.023f)*.5f + .15f, noise(vec2(mat_pos.x, mat_pos.z)*.03f)*.6f + .25f, 0.0f);
		m *= (normal.y - 0.65f)*.6f;
		mat = mix(mat, m, clamp((normal.y - .65f)*1.3f * (45.35f - mat_pos.y)*0.1f, 0.0f, 1.0f));
	}

	// Snow topped mountains...
	if (mat_pos.y > 80.0f && normal.y > .42f)
	{
		float snow = clamp((mat_pos.y - 80.0f - noise(vec2(mat_pos.x, mat_pos.z) * .1f)*28.0f) * 0.035f, 0.0f, 1.0f);
		mat = mix(mat, vec3(.7f, .7f, .8f), snow);
		specular += snow;
		ambient += snow * .3f;
	}

	mat = do_lighting(mat, pos, normal, dir, dis_sqrd);

	mat = apply_fog(mat, dis_sqrd, dir);

	return mat;
}

// Some would say, most of the magic is done in post! :D
inline vec3 post_effects(vec3 rgb)
{
	return (1.0f - exp(-rgb * 6.0f)) * 1.0024f;
}

////////////////


#pragma region ray_march
//--------------------------------------------------------------------------
inline float binary_subdivision(vec3 ro, vec3 rd, vec2 t, int divisions)
{
	// Home in on the surface by dividing by two and split...
	float halfway_t;

	for (int i = 0; i < divisions; i++)
	{
		halfway_t = dot(t, vec2(.5f));
		float d = height_to_surface(ro + halfway_t * rd);
		t = mix(vec2(t.x, halfway_t), vec2(halfway_t, t.y), step(0.5f, d));
	}
	return halfway_t;
}

//--------------------------------------------------------------------------
inline float ray_march(vec3 ro, vec3 rd)
{
	float t = 0.01f;
	float oldT = 0.0f;
	float delta = 0.0f;
	vec2 distances;
	bool fin = false;
	while (t < max_view_dist)
	{
		vec3 p = ro + t * rd;
		float h = height_to_surface(p);
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
		delta = max(0.1f, 0.2f*h) + (t*0.0025f);
		oldT = t;
		t += delta;
	}
	if (fin)
		return binary_subdivision(ro, rd, distances, 10);
	return max_view_dist;
}


inline vec3 calc_ray_color(vec3 ro, vec3 rd)
{
	vec3 color;
	float dist = ray_march(ro, rd);
	if (dist >= max_view_dist)
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
