#ifdef __cplusplus
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
using namespace glm;
#else
#define inline 
#define static 
#endif

// Created by inigo quilez - iq/2016
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0f Unported License.


// Normals are analytical (true derivatives) for the terrain and for the clouds, that 
// includes the noise, the fbm and the smoothsteps involved chain derivatives correctly.
//
// See here for more info: http://iquilezles.org/www/articles/morenoise/morenoise.htm
//
// Lighting and art composed for this shot/camera
//
// The trees are really cheap (ellipsoids with noise), but they kind of do the job in
// distance and low image resolutions.
//
// I used some cheap reprojection technique to smooth out the render, although it creates
// halows and blurs the image way too much (I don't have the time now to do the tricks
// used in TAA). Enable the STATIC_CAMERA define to see a sharper image.
//
// Lastly, it runs very slow in WebGL (but runs 2x faster in native GL), so I had to make
// a youtube capture, sorry for that!
// 
// https://www.youtube.com/watch?v=VqYROPZrDeU


#define LOWQUALITY



//==========================================================================================
// general utilities
//==========================================================================================

inline float sdEllipsoidY(vec3 p, vec2 r)
{
	return (length(p / vec3(r.x, r.y, r.x)) - 1.0f) * r.x;
}

// return smoothstep and its derivative
inline vec2 smoothstepd(float a, float b, float x)
{
	if (x < a) return vec2(0.0f, 0.0f);
	if (x > b) return vec2(1.0f, 0.0f);
	float ir = 1.0f / (b - a);
	x = (x - a)*ir;
	return vec2(x*x*(3.0f - 2.0f*x), 6.0f*x*(1.0f - x)*ir);
}

inline mat3 setCamera(vec3 ro, vec3 ta, float cr)
{
	vec3 cw = normalize(ta - ro);
	vec3 cp = vec3(sin(cr), cos(cr), 0.0f);
	vec3 cu = normalize(cross(cw, cp));
	vec3 cv = normalize(cross(cu, cw));
	return mat3(cu, cv, cw);
}

//==========================================================================================
// hashes
//==========================================================================================

inline float hash1(vec2 p)
{
	p = 50.0f*fract(p*0.3183099f);
	return fract(p.x*p.y*(p.x + p.y));
}

inline float hash1(float n)
{
	return fract(n*17.0f*fract(n*0.3183099f));
}

inline vec2 hash2(float n) { return fract(sin(vec2(n, n + 1.0f))*vec2(43758.5453123f, 22578.1459123f)); }


inline vec2 hash2(vec2 p)
{
	const vec2 k = vec2(0.3183099f, 0.3678794f);
	p = p * k + vec2(k.y, k.x);
	return fract(16.0f * k*fract(p.x*p.y*(p.x + p.y)));
}

//==========================================================================================
// noises
//==========================================================================================

// value noise, and its analytical derivatives
inline vec4 noised(vec3 x)
{
	vec3 p = floor(x);
	vec3 w = fract(x);

	vec3 u = w * w*w*(w*(w*6.0f - 15.0f) + 10.0f);
	vec3 du = 30.0f*w*w*(w*(w - 2.0f) + 1.0f);

	float n = p.x + 317.0f*p.y + 157.0f*p.z;

	float a = hash1(n + 0.0f);
	float b = hash1(n + 1.0f);
	float c = hash1(n + 317.0f);
	float d = hash1(n + 318.0f);
	float e = hash1(n + 157.0f);
	float f = hash1(n + 158.0f);
	float g = hash1(n + 474.0f);
	float h = hash1(n + 475.0f);

	float k0 = a;
	float k1 = b - a;
	float k2 = c - a;
	float k3 = e - a;
	float k4 = a - b - c + d;
	float k5 = a - c - e + g;
	float k6 = a - b - e + f;
	float k7 = -a + b + c - d + e - f - g + h;

	return vec4(-1.0f + 2.0f*(k0 + k1 * u.x + k2 * u.y + k3 * u.z + k4 * u.x*u.y + k5 * u.y*u.z + k6 * u.z*u.x + k7 * u.x*u.y*u.z),
		2.0f* du * vec3(k1 + k4 * u.y + k6 * u.z + k7 * u.y*u.z,
			k2 + k5 * u.z + k4 * u.x + k7 * u.z*u.x,
			k3 + k6 * u.x + k5 * u.y + k7 * u.x*u.y));
}

inline float noise(vec3 x)
{
	vec3 p = floor(x);
	vec3 w = fract(x);

	vec3 u = w * w*w*(w*(w*6.0f - 15.0f) + 10.0f);

	float n = p.x + 317.0f*p.y + 157.0f*p.z;

	float a = hash1(n + 0.0f);
	float b = hash1(n + 1.0f);
	float c = hash1(n + 317.0f);
	float d = hash1(n + 318.0f);
	float e = hash1(n + 157.0f);
	float f = hash1(n + 158.0f);
	float g = hash1(n + 474.0f);
	float h = hash1(n + 475.0f);

	float k0 = a;
	float k1 = b - a;
	float k2 = c - a;
	float k3 = e - a;
	float k4 = a - b - c + d;
	float k5 = a - c - e + g;
	float k6 = a - b - e + f;
	float k7 = -a + b + c - d + e - f - g + h;

	return -1.0f + 2.0f*(k0 + k1 * u.x + k2 * u.y + k3 * u.z + k4 * u.x*u.y + k5 * u.y*u.z + k6 * u.z*u.x + k7 * u.x*u.y*u.z);
}

inline vec3 noised(vec2 x)
{
	vec2 p = floor(x);
	vec2 w = fract(x);

	vec2 u = w * w*w*(w*(w*6.0f - 15.0f) + 10.0f);
	vec2 du = 30.0f*w*w*(w*(w - 2.0f) + 1.0f);

	float a = hash1(p + vec2(0, 0));
	float b = hash1(p + vec2(1, 0));
	float c = hash1(p + vec2(0, 1));
	float d = hash1(p + vec2(1, 1));

	float k0 = a;
	float k1 = b - a;
	float k2 = c - a;
	float k4 = a - b - c + d;

	return vec3(-1.0f + 2.0f*(k0 + k1 * u.x + k2 * u.y + k4 * u.x*u.y),
		2.0f* du * vec2(k1 + k4 * u.y,
			k2 + k4 * u.x));
}

inline float noise(vec2 x)
{
	vec2 p = floor(x);
	vec2 w = fract(x);
	vec2 u = w * w*w*(w*(w*6.0f - 15.0f) + 10.0f);

#if 0
	p *= 0.3183099f;
	float kx0 = 50.0f*fract(p.x);
	float kx1 = 50.0f*fract(p.x + 0.3183099f);
	float ky0 = 50.0f*fract(p.y);
	float ky1 = 50.0f*fract(p.y + 0.3183099f);

	float a = fract(kx0*ky0*(kx0 + ky0));
	float b = fract(kx1*ky0*(kx1 + ky0));
	float c = fract(kx0*ky1*(kx0 + ky1));
	float d = fract(kx1*ky1*(kx1 + ky1));
#else
	float a = hash1(p + vec2(0, 0));
	float b = hash1(p + vec2(1, 0));
	float c = hash1(p + vec2(0, 1));
	float d = hash1(p + vec2(1, 1));
#endif

	return -1.0f + 2.0f*(a + (b - a)*u.x + (c - a)*u.y + (a - b - c + d)*u.x*u.y);
}

//==========================================================================================
// fbm constructions
//==========================================================================================

static const mat3 m3 = mat3(0.00f, 0.80f, 0.60f,
	-0.80f, 0.36f, -0.48f,
	-0.60f, -0.48f, 0.64f);
static const mat3 m3i = mat3(0.00f, -0.80f, -0.60f,
	0.80f, 0.36f, -0.48f,
	0.60f, -0.48f, 0.64f);
static const mat2 m2 = mat2(0.80f, 0.60f,
	-0.60f, 0.80f);
static const mat2 m2i = mat2(0.80f, -0.60f,
	0.60f, 0.80f);

//------------------------------------------------------------------------------------------

inline float fbm_4(vec3 x)
{
	float f = 2.0f;
	float s = 0.5f;
	float a = 0.0f;
	float b = 0.5f;
	for (int i = 0; i < 4; i++)
	{
		float n = noise(x);
		a += b * n;
		b *= s;
		x = f * m3*x;
	}
	return a;
}

inline vec4 fbmd_8(vec3 x)
{
	float f = 1.92f;
	float s = 0.5f;
	float a = 0.0f;
	float b = 0.5f;
	vec3  d = vec3(0.0f);
	mat3  m = mat3(1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f);
	for (int i = 0; i < 7; i++)
	{
		vec4 n = noised(x);
		a += b * n.x;          // accumulate values		
		d += b * m*vec3(n.y, n.z, n.w);      // accumulate derivatives
		b *= s;
		x = f * m3*x;
		m = f * m3i*m;
	}
	return vec4(a, d);
}

inline float fbm_9(vec2 x)
{
	float f = 1.9f;
	float s = 0.55f;
	float a = 0.0f;
	float b = 0.5f;
	for (int i = 0; i < 9; i++)
	{
		float n = noise(x);
		a += b * n;
		b *= s;
		x = f * m2*x;
	}
	return a;
}

inline vec3 fbmd_9(vec2 x)
{
	float f = 1.9f;
	float s = 0.55f;
	float a = 0.0f;
	float b = 0.5f;
	vec2  d = vec2(0.0f);
	mat2  m = mat2(1.0f, 0.0f, 0.0f, 1.0f);
	for (int i = 0; i < 9; i++)
	{
		vec3 n = noised(x);
		a += b * n.x;          // accumulate values		
		d += b * m*vec2(n.y, n.z);       // accumulate derivatives
		b *= s;
		x = f * m2*x;
		m = f * m2i*m;
	}
	return vec3(a, d);
}

inline float fbm_4(vec2 x)
{
	float f = 1.9f;
	float s = 0.55f;
	float a = 0.0f;
	float b = 0.5f;
	for (int i = 0; i < 4; i++)
	{
		float n = noise(x);
		a += b * n;
		b *= s;
		x = f * m2*x;
	}
	return a;
}

//==========================================================================================

#define ZERO (min(iFrame,0))


//==========================================================================================
// specifics to the actual painting
//==========================================================================================


//------------------------------------------------------------------------------------------
// global
//------------------------------------------------------------------------------------------

const vec3  kSunDir = vec3(-0.624695f, 0.468521f, -0.624695f);

inline vec3 renderSky(vec3 ro, vec3 rd)
{
	// background sky     
	vec3 col = 0.9f*vec3(0.4f, 0.65f, 1.0f) - rd.y*vec3(0.4f, 0.36f, 0.4f);

	return col;
}

inline vec3 fog(vec3 col, float t, vec3 ro, vec3 rd)
{
	vec3 fogCol = renderSky(ro, rd);
	return mix(col, fogCol, 1.0f - exp(-0.0000001f*t*t*t));
}


//------------------------------------------------------------------------------------------
// terrain
//------------------------------------------------------------------------------------------

inline vec2 terrainMap(vec2 p)
{
	const float sca = 0.0010f;
	const float amp = 300.0f;

	p *= sca;
	float e = fbm_9(p + vec2(1.0f, -2.0f));
	float a = 1.0f - smoothstep(0.12f, 0.13f, abs(e + 0.12f)); // flag high-slope areas (-0.25f, 0.0f)
	e = e + 0.15f*smoothstep(-0.08f, -0.01f, e);
	e *= amp;
	return vec2(e, a);
}

inline float terrain(vec2 p)
{
	return terrainMap(p).x;
}

inline float terrain(vec3 p)
{
	return terrainMap(vec2(p.x, p.z)).x;
}

inline vec4 terrainMapD(vec2 p)
{
	const float sca = 0.0010f;
	const float amp = 300.0f;
	p *= sca;
	vec3 e = fbmd_9(p + vec2(1.0f, -2.0f));
	vec2 c = smoothstepd(-0.08f, -0.01f, e.x);
	e.x = e.x + 0.15f*c.x;
	//e.yz = e.yz + 0.15f*c.y*e.yz;
	e.y = (e.y + 0.15f*c.y*e.y)*amp*sca;
	e.z = (e.z + 0.15f*c.y*e.z)*amp*sca;
	e.x *= amp;
	//e.yz *= amp*sca;
	return vec4(e.x, normalize(vec3(-e.y, 1.0f, -e.z)));
}

inline vec3 terrainNormal(vec2 pos)
{
#if 1
	vec4 t = terrainMapD(pos);
	return vec3(t.y, t.z, t.w);
#else    
	vec2 e = vec2(0.03f, 0.0f);
	return normalize(vec3(terrainMap(pos - e.xy).x - terrainMap(pos + e.xy).x,
		2.0f*e.x,
		terrainMap(pos - e.yx).x - terrainMap(pos + e.yx).x));
#endif    
}

inline float terrainShadow(vec3 ro, vec3 rd, float mint)
{
	float res = 1.0f;
	float t = mint;
#ifdef LOWQUALITY
	for (int i = 0; i < 32; i++)
	{
		vec3  pos = ro + t * rd;
		vec2  env = terrainMap(vec2(pos.x, pos.z));
		float hei = pos.y - env.x;
		res = min(res, 32.0f*hei / t);
		if (res < 0.0001f) break;
		t += clamp(hei, 1.0f + t * 0.1f, 50.0f);
	}
#else
	for (int i = 0; i < 128; i++)
	{
		vec3  pos = ro + t * rd;
		vec2  env = terrainMap(vec2(pos.x, pos.z));
		float hei = pos.y - env.x;
		res = min(res, 32.0f*hei / t);
		if (res < 0.0001f) break;
		t += clamp(hei, 0.5f + t * 0.05f, 25.0f);
	}
#endif
	return clamp(res, 0.0f, 1.0f);
}

inline float height_to_surface(vec3 p)
{
	float h = terrain(vec2(p.x, p.z));

	return p.y - h;
}

//--------------------------------------------------------------------------
inline float binary_subdivision(vec3 ro, vec3 rd, vec2 t, int divisions)
{
	// Home in on the surface by dividing by two and split...
	float halfway_t;

	for (int i = 0; i < divisions; i++)
	{
		halfway_t = dot(t, vec2(.5f));
		float d = terrain(ro + halfway_t * rd);
		t = mix(vec2(t.x, halfway_t), vec2(halfway_t, t.y), step(0.5f, d));
	}
	return halfway_t;
}

inline float raymarchTerrain(vec3 ro, vec3 rd, float tmin, float tmax)
{
	//float tt = (150.0f-ro.y)/rd.y; if( tt>0.0f ) tmax = min( tmax, tt );

	float dis, th;
	float t2 = -1.0f;
	float t = tmin;
	float ot = t;
	float odis = 0.0f;
	float odis2 = 0.0f;
	while (t < tmax)
	{
		th = 0.001f*t;

		vec3  pos = ro + t * rd;
		vec2  env = terrainMap(vec2(pos.x, pos.z));
		float hei = env.x;

		// terrain
		dis = pos.y - hei;
		if (dis < th) break;

		ot = t;
		odis = dis;
		t += dis * 0.8f*(1.0f - 0.75f*env.y); // slow down in step areas
		if (t > tmax) break;
	}

	if (t > tmax) t = -1.0f;
	else t = ot + (th - odis)*(t - ot) / (dis - odis); // linear interpolation for better accuracy
	//else t = binary_subdivision(ro, rd, vec2(t, ot), 15);
	return t;

	//float t = 0.01f;
	//float oldT = 0.0f;
	//float delta = 0.0f;
	//vec2 distances;
	//bool fin = false;
	//while (t < 350)
	//{
	//	vec3 p = ro + t * rd;
	//	float h = height_to_surface(p);
	//	// Are we inside, and close enough to fudge a hit?...
	//	if (h < 0.5f)
	//	{
	//		fin = true;
	//		distances = vec2(oldT, t);
	//		break;
	//	}
	//	// Delta ray advance - a fudge between the height returned
	//	// and the distance already travelled.
	//	// It's a really fiddly compromise between speed and accuracy
	//	// Too large a step and the tops of ridges get missed.
	//	delta = max(0.01f, 0.02f*h) + (t*0.00025f);
	//	oldT = t;
	//	t += delta;
	//}
	//if (fin)
	//	return binary_subdivision(ro, rd, distances, 20);
	//return tmax;
}

inline vec3 surface_color(vec3 pos, vec3 ro, float t)
{
	vec3 res = vec3(0.0f);
	pos.y = terrain(pos);

	//if (t > 0.0f)
	{
		vec3 rd = normalize(pos - ro);
		vec3 nor = terrainNormal(vec2(pos.x, pos.z));

		// bump map
		vec4 b = fbmd_8(pos*0.3f*vec3(1.0f, 0.2f, 1.0f));
		nor = normalize(nor + 0.8f*(1.0f - abs(nor.y))*0.8f*vec3(b.y, b.z, b.w));

		vec3 col = vec3(0.18f, 0.11f, 0.10f)*.75f;
		col = 1.0f*mix(col, vec3(0.1f, 0.1f, 0.0f)*0.3f, smoothstep(0.7f, 0.9f, nor.y));

		//col *= 1.0f + 2.0f*fbm( pos*0.2f*vec3(1.0f,4.0f,1.0f) );

		float sha = 0.0f;
		float dif = clamp(dot(nor, kSunDir), 0.0f, 1.0f);
		if (dif > 0.0001f)
		{
			sha = terrainShadow(pos + nor * 0.01f, kSunDir, 0.01f);
			//if( sha>0.0001f ) sha *= cloudsShadow( pos+nor*0.01f, kSunDir, 0.01f, 1000.0f );
			dif *= sha;
		}
		vec3  ref = reflect(rd, nor);
		float bac = clamp(dot(normalize(vec3(-kSunDir.x, 0.0f, -kSunDir.z)), nor), 0.0f, 1.0f)*clamp((pos.y + 100.0f) / 100.0f, 0.0f, 1.0f);
		float dom = clamp(0.5f + 0.5f*nor.y, 0.0f, 1.0f);
		vec3  lin= 1.0f*0.2f*mix(0.1f*vec3(0.1f, 0.2f, 0.0f), vec3(0.7f, 0.9f, 1.0f), dom);//pow(vec3(occ),vec3(1.5f,0.7f,0.5f));
		lin+= 1.0f*5.0f*vec3(1.0f, 0.9f, 0.8f)*dif;
		lin+= 1.0f*0.35f*vec3(1.0f)*bac;

		col *= lin;

		col = fog(col, t, ro, rd);

		res = col;
		//resT = t;
	}

	return res;
}

//------------------------------------------------------------------------------------------
// sky
//------------------------------------------------------------------------------------------

inline vec3 post_effects(vec3 col)
{
	col = sqrt(col);

	col = col * 0.15f + 0.85f*col*col*(3.0f - 2.0f*col);            // contrast
	col = pow(col, vec3(1.0f, 0.92f, 1.0f));   // soft green
	col *= vec3(1.02f, 0.99f, 0.99f);            // tint red
	col.z = (col.z + 0.1f) / 1.1f;                // bias blue
	col = mix(col, vec3(col.y, col.y, col.y), 0.15f);       // desaturate

	col = clamp(col, 0.0f, 1.0f);

	return col;
}


//------------------------------------------------------------------------------------------
// maimage making function
//------------------------------------------------------------------------------------------


inline vec3 calc_ray_color(vec3 ro, vec3 rd)
{
	vec3 col;
	const float tmax = max_view_dist;
	float t = raymarchTerrain(ro, rd, 0.01f, tmax);
	if (t < 0.f)
		col = renderSky(ro, rd);
	else
		col = surface_color(ro + t * rd, ro, t);

	col = post_effects(col);

	return col;
}



//void main()
//	//out vec4 fragColor, vec2 fragCoord)
//{
//	vec2 o = hash2(float(iFrame)) - 0.5f;
//
//	vec2 p = (-iResolution.xy + 2.0f*(fragCoord + o)) / iResolution.y;
//
//	//----------------------------------
//	// setup
//	//----------------------------------
//
//	// camera
//#ifdef  STATIC_CAMERA
//	vec3 ro = vec3(0.0f, -99.25f, 5.0f);
//	vec3 ta = vec3(0.0f, -99.0f, 0.0f);
//#else
//	float time = iTime;
//	vec3 ro = vec3(0.0f, -99.25f, 5.0f) + vec3(10.0f*sin(0.02f*time), 0.0f, -10.0f*sin(0.2f + 0.031f*time));
//	vec3 ta = vec3(0.0f, -98.25f, -45.0f + ro.z);
//#endif
//
//	// ray
//	mat3 ca = setCamera(ro, ta, 0.0f);
//	vec3 rd = ca * normalize(vec3(p.xy, 1.5f));
//
//	float resT = 1000.0f;
//
//	//----------------------------------
//	// sky
//	//----------------------------------
//
//	vec3 col = renderSky(ro, rd);
//
//	//----------------------------------
//	// terrain
//	//----------------------------------
//
//	vec2 tmima = vec2(15.0f, 1000.0f);
//	{
//		vec4 res = renderTerrain(ro, rd, tmima, resT);
//		col = col * (1.0f - res.w) + vec3(res.x, res.y, res.z);
//	}
//
//	//----------------------------------
//	// trees
//	//----------------------------------
//
//
//	//----------------------------------
//	// clouds
//	//----------------------------------
//
//
//	//----------------------------------
//	// final
//	//----------------------------------
//
//	// sun glare    
//
//
//	// gamma
//	col = sqrt(col);
//
//	//----------------------------------
//	// color grading
//	//----------------------------------
//
//	col = col * 0.15f + 0.85f*col*col*(3.0f - 2.0f*col);            // contrast
//	col = pow(col, vec3(1.0f, 0.92f, 1.0f));   // soft green
//	col *= vec3(1.02f, 0.99f, 0.99f);            // tint red
//	col.z = (col.z + 0.1f) / 1.1f;                // bias blue
//	col = mix(col, col.yyy, 0.15f);       // desaturate
//
//	col = clamp(col, 0.0f, 1.0f);
//
//
//	//------------------------------------------
//// reproject from previous frame and average
//	//------------------------------------------
//
//	mat4 oldCam = mat4(textureLod(iChannel0, vec2(0.5f, 0.5f) / iResolution.xy, 0.0f),
//		textureLod(iChannel0, vec2(1.5f, 0.5f) / iResolution.xy, 0.0f),
//		textureLod(iChannel0, vec2(2.5f, 0.5f) / iResolution.xy, 0.0f),
//		0.0f, 0.0f, 0.0f, 1.0f);
//
//
//	// world space
//	vec4 wpos = vec4(ro + rd * resT, 1.0f);
//	// camera space
//	vec3 cpos = (wpos*oldCam).xyz; // note inverse multiply
//	// ndc space
//	vec2 npos = 1.5f * cpos.xy / cpos.z;
//	// screen space
//	vec2 spos = 0.5f + 0.5f*npos*vec2(iResolution.y / iResolution.x, 1.0f);
//	// undo dither
//	spos -= o / iResolution.xy;
//	// raster space
//	vec2 rpos = spos * iResolution.xy;
//
//	if (rpos.y < 1.0f && rpos.x < 3.0f)
//	{
//	}
//	else
//	{
//		vec3 ocol = textureLod(iChannel0, spos, 0.0f).xyz;
//		if (iFrame == 0) ocol = col;
//		col = mix(ocol, col, 0.1f);
//	}
//
//	//----------------------------------
//
//	if (fragCoord.y < 1.0f && fragCoord.x < 3.0f)
//	{
//		if (abs(fragCoord.x - 2.5f) < 0.5f) fragColor = vec4(ca[2], -dot(ca[2], ro));
//		if (abs(fragCoord.x - 1.5f) < 0.5f) fragColor = vec4(ca[1], -dot(ca[1], ro));
//		if (abs(fragCoord.x - 0.5f) < 0.5f) fragColor = vec4(ca[0], -dot(ca[0], ro));
//	}
//	else
//	{
//		fragColor = vec4(col, 1.0f);
//	}
//}
