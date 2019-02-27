#version 450 core

layout(location = 0) in vec3 world_pos;

layout(push_constant) uniform frame_data_t
{
	mat4 camera_vp;
	vec4 camera_pos;
	vec2 min;
	vec2 max;
	uint buffer_slot;
} frame_data;

layout(location = 0) out vec4 out_color;

const mat2 rotate2D = mat2(1.3623, 1.7531, -1.7131, 1.4623);
#define HASHSCALE1 .1031
vec2 add = vec2(1.0, 0.0);
vec3 sunLight = normalize(vec3(0.4, 0.4, 0.48));
vec3 sunColour = vec3(1.0, .9, .83);
float specular = 0.0;
float ambient;

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

void DoLighting(inout vec3 mat, in vec3 pos, in vec3 normal, in vec3 eyeDir)
{
	float h = dot(sunLight, normal);
	float c = max(h, 0.0) + ambient;
	mat = mat * sunColour * c;
	// Specular...
	if (h > 0.0)
	{
		vec3 R = reflect(sunLight, normal);
		float specAmount = pow(max(dot(R, normalize(eyeDir)), 0.0), 3.0)*specular;
		mat = mix(mat, sunColour, specAmount);
	}
}


vec3 TerrainColour(vec3 pos, vec3 normal)
{
	vec3 mat;
	specular = .0;
	ambient = .1;
	vec3 dir = normalize(pos - (frame_data.camera_vp * vec4(0,0,0,1)).xyz);

	vec3 matPos = pos * 2.0;

	float f = clamp(Noise(matPos.xz*.05), 0.0, 1.0);//*10.8;
	f += Noise(matPos.xz*.1 + normal.yz*1.08)*.85;
	f *= .55;
	vec3 m = mix(vec3(.63*f + .2, .7*f + .1, .7*f + .1), vec3(f*.43 + .1, f*.3 + .2, f*.35 + .1), f*.65);
	mat = m * vec3(f*m.x + .36, f*m.y + .30, f*m.z + .28);
	// Should have used smoothstep to add colours, but left it using 'if' for sanity...
	if (normal.y < .5)
	{
		float v = normal.y;
		float c = (.5 - normal.y) * 4.0;
		c = clamp(c*c, 0.1, 1.0);
		f = Noise(vec2(matPos.x*.09, matPos.z*.095 + matPos.yy*0.15));
		f += Noise(vec2(matPos.x*2.233, matPos.z*2.23))*0.5;
		mat = mix(mat, vec3(.4*f), c);
		specular += .1;
	}

	// Grass. Use the normal to decide when to plonk grass down...
	if (normal.y > .65)
	{

		m = vec3(Noise(matPos.xz*.023)*.5 + .15, Noise(matPos.xz*.03)*.6 + .25, 0.0);
		m *= (normal.y - 0.65)*.6;
		mat = mix(mat, m, clamp((normal.y - .65)*1.3 * (45.35 - matPos.y)*0.1, 0.0, 1.0));
	}

	// Snow topped mountains...
	if (matPos.y > 80.0 && normal.y > .42)
	{
		float snow = clamp((matPos.y - 80.0 - Noise(matPos.xz * .1)*28.0) * 0.035, 0.0, 1.0);
		mat = mix(mat, vec3(.7, .7, .8), snow);
		specular += snow;
		ambient += snow * .3;
	}

	DoLighting(mat, pos, normal, dir);

	return mat;
}

vec3 PostEffects(vec3 rgb)
{
	rgb = (1.0 - exp(-rgb * 6.0)) * 1.0024;
	return rgb;
}

void main()
{
	vec3 camera_pos = frame_data.camera_pos.xyz;
	float dist = length(world_pos - camera_pos);

	float p = min(.3, .0005 + .00005 * dist*dist);
	vec3 nor = vec3(0.0, Terrain2(world_pos.xz), 0.0);
	vec3 v2 = nor - vec3(p, Terrain2(world_pos.xz + vec2(p, 0.0)), 0.0);
	vec3 v3 = nor - vec3(0.0, Terrain2(world_pos.xz + vec2(0.0, -p)), -p);
	nor = cross(v2, v3);
	nor = normalize(nor);

	out_color = vec4(PostEffects(TerrainColour(world_pos, nor)), 1.0);
}