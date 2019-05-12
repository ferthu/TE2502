#ifdef __cplusplus
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
using namespace glm;
#else
#define inline 
#define static 
#endif

// Created by inigo quilez - iq/2013
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0f Unported License.


// on the derivatives based noise: http://iquilezles.org/www/articles/morenoise/morenoise.htm
// on the soft shadow technique: http://iquilezles.org/www/articles/rmshadows/rmshadows.htm
// on the fog calculations: http://iquilezles.org/www/articles/fog/fog.htm
// on the lighting: http://iquilezles.org/www/articles/outdoorslighting/outdoorslighting.htm
// on the raymarching: http://iquilezles.org/www/articles/terrainmarching/terrainmarching.htm


#define SC (1.0f)

// value noise, and its analytical derivatives
inline vec3 noised(vec2 x)
{
	vec2 f = fract(x);
	vec2 u = f * f*(3.0f - 2.0f*f);

	ivec2 p = ivec2(floor(x));
	float a = imageLoad(storage_tex0, (p + ivec2(0, 0)) & 255).x;
	float b = imageLoad(storage_tex0, (p + ivec2(1, 0)) & 255).x;
	float c = imageLoad(storage_tex0, (p + ivec2(0, 1)) & 255).x;
	float d = imageLoad(storage_tex0, (p + ivec2(1, 1)) & 255).x;

	return vec3(a + (b - a)*u.x + (c - a)*u.y + (a - b - c + d)*u.x*u.y,
		6.0f*f*(1.0f - f)*(vec2(b - a, c - a) + (a - b - c + d)*vec2(u.y, u.x)));
}

const mat2 m2 = mat2(0.8f, -0.6f, 0.6f, 0.8f);


inline float terrainH(vec2 x)
{
	vec2  p = x * 0.003f / SC;
	float a = 0.0f;
	float b = 1.0f;
	vec2  d = vec2(0.0f);
	for (int i = 0; i < 15; i++)
	{
		vec3 n = noised(p);
		d += vec2(n.y, n.z);
		a += b * n.x / (1.0f + dot(d, d));
		b *= 0.5f;
		p = m2 * p*2.0f;
	}

	return SC * 120.0f*a;
}

inline float terrainM(vec2 x)
{
	vec2  p = x * 0.003f / SC;
	float a = 0.0f;
	float b = 1.0f;
	vec2  d = vec2(0.0f);
	for (int i = 0; i < 9; i++)
	{
		vec3 n = noised(p);
		d += vec2(n.y, n.z);
		a += b * n.x / (1.0f + dot(d, d));
		b *= 0.5f;
		p = m2 * p*2.0f;
	}
	return SC * 120.0f*a;
}

inline float terrainL(vec2 x)
{
	vec2  p = x * 0.003f / SC;
	float a = 0.0f;
	float b = 1.0f;
	vec2  d = vec2(0.0f);
	for (int i = 0; i < 3; i++)
	{
		vec3 n = noised(p);
		d += vec2(n.y, n.z);
		a += b * n.x / (1.0f + dot(d, d));
		b *= 0.5f;
		p = m2 * p*2.0f;
	}

	return SC * 120.0f*a;
}

inline float terrain(vec2 p)
{
	return terrainM(p);
}

inline float ray_march(vec3 ro, vec3 rd, float tmin, float tmax)
{
	float t = tmin;
	for (int i = 0; i < 300; i++)
	{
		vec3 pos = ro + t * rd;
		float h = pos.y - terrainM(vec2(pos.x, pos.z));
		if (abs(h) < (0.002f*t) || t > tmax) break;
		t += 0.4f*h;
	}

	return t;
}

inline float softShadow(vec3 ro, vec3 rd)
{
	float res = 1.0f;
	float t = 0.001f;
	for (int i = 0; i < 80; i++)
	{
		vec3  p = ro + t * rd;
		float h = p.y - terrainM(vec2(p.x, p.z));
		res = min(res, 16.0f*h / t);
		t += h;
		if (res<0.001f || p.y>(SC*200.0f)) break;
	}
	return clamp(res, 0.0f, 1.0f);
}

inline vec3 calcNormal(vec3 pos, float t)
{
	vec2 eps = vec2(0.002f*t, 0.0f);
	vec2 p = vec2(pos.x, pos.z);
	vec2 eps1 = vec2(eps.x, eps.y);
	vec2 eps2 = vec2(eps.y, eps.x);
	return normalize(vec3(terrainH(p - eps1) - terrainH(p + eps1),
		2.0f*eps.x,
		terrainH(p - eps2) - terrainH(p + eps2)));
}

inline float fbm(vec2 p)
{
	float f = 0.0f;
	f += 0.5000f*imageLoad(storage_tex0, ivec2(p / 256.0f)).x; p = m2 * p*2.02f;
	f += 0.2500f*imageLoad(storage_tex0, ivec2(p / 256.0f)).x; p = m2 * p*2.03f;
	f += 0.1250f*imageLoad(storage_tex0, ivec2(p / 256.0f)).x; p = m2 * p*2.01f;
	f += 0.0625f*imageLoad(storage_tex0, ivec2(p / 256.0f)).x;
	return f / 0.9375f;
}

//const float kMaxT = 5000.0f*SC;
const vec3 light1 = normalize(vec3(-0.8f, 0.4f, -0.3f));

inline vec3 get_sky(vec3 ro, vec3 rd)
{
	vec3 col;
	// sky		
	col = vec3(0.2f, 0.5f, 0.85f)*1.1f - rd.y*rd.y*0.5f;
	col = mix(col, 0.85f*vec3(0.7f, 0.75f, 0.85f), pow(1.0f - max(rd.y, 0.0f), 4.0f));
	// sun
	float sundot = clamp(dot(rd, light1), 0.0f, 1.0f);
	col += 0.25f*vec3(1.0f, 0.7f, 0.4f)*pow(sundot, 5.0f);
	col += 0.25f*vec3(1.0f, 0.8f, 0.6f)*pow(sundot, 64.0f);
	col += 0.2f*vec3(1.0f, 0.8f, 0.6f)*pow(sundot, 512.0f);
	// clouds
	//vec2 sc = vec2(ro.x, ro.z) + vec2(rd.x, rd.z)*(SC*1000.0f - ro.y) / rd.y;
	//col = mix(col, vec3(1.0f, 0.95f, 1.0f), 0.5f*smoothstep(0.5f, 0.8f, fbm(0.0005f*sc / SC)));
	// horizon
	//col = mix(col, 0.68f*vec3(0.4f, 0.65f, 1.0f), pow(1.0f - max(rd.y, 0.0f), 16.0f));
	return col;
}

inline vec3 post_effects(vec3 col)
{
	// gamma
	col = sqrt(col);
	return col;
}

inline vec3 surface_color(vec3 pos, vec3 ro, float t)
{
	vec3 col;
	pos.y = terrainM(vec2(pos.x, pos.z));
	vec3 rd = normalize(pos - ro);

	float sundot = clamp(dot(rd, light1), 0.0f, 1.0f);

	// mountains		
	//vec3 pos = ro + t * rd;
	vec3 nor = calcNormal(pos, t);
	//nor = normalize( nor + 0.5f*( vec3(-1.0f,0.0f,-1.0f) + vec3(2.0f,1.0f,2.0f)*texture(iChannel1,0.01f*pos.xz).xyz) );
	vec3 ref = reflect(rd, nor);
	float fre = clamp(1.0f + dot(rd, nor), 0.0f, 1.0f);
	vec3 hal = normalize(light1 - rd);

	// rock
	float r = imageLoad(storage_tex0, ivec2((7.0f / SC)*vec2(pos.x, pos.z) / 256.0f)).x;
	col = (r*0.25f + 0.75f)*0.9f*mix(vec3(0.08f, 0.05f, 0.03f), vec3(0.10f, 0.09f, 0.08f),
		imageLoad(storage_tex0, ivec2(0.00007f*vec2(pos.x, pos.y*48.0f) / SC)).x);
	col = mix(col, 0.20f*vec3(0.45f, .30, 0.15f)*(0.50f + 0.50f*r), smoothstep(0.70f, 0.9f, nor.y));
	col = mix(col, 0.15f*vec3(0.30f, .30, 0.10f)*(0.25f + 0.75f*r), smoothstep(0.95f, 1.0f, nor.y));

	// snow
	float h = smoothstep(55.0f, 80.0f, pos.y / SC + 25.0f*fbm(0.01f*vec2(pos.x, pos.z) / SC));
	float e = smoothstep(1.0f - 0.5f*h, 1.0f - 0.1f*h, nor.y);
	float o = 0.3f + 0.7f*smoothstep(0.0f, 0.1f, nor.x + h * h);
	float s = h * e*o;
	col = mix(col, 0.29f*vec3(0.62f, 0.65f, 0.7f), smoothstep(0.1f, 0.9f, s));

	// lighting		
	float amb = clamp(0.5f + 0.5f*nor.y, 0.0f, 1.0f);
	float dif = clamp(dot(light1, nor), 0.0f, 1.0f);
	float bac = clamp(0.2f + 0.8f*dot(normalize(vec3(-light1.x, 0.0f, light1.z)), nor), 0.0f, 1.0f);
	float sh = 1.0f; if (dif >= 0.0001f) sh = softShadow(pos + light1 * SC*0.05f, light1);

	vec3 lin = vec3(0.0f);
	lin += dif * vec3(7.00f, 5.00f, 3.00f)*1.3f*vec3(sh, sh*sh*0.5f + 0.5f*sh, sh*sh*0.8f + 0.2f*sh);
	lin += amb * vec3(0.40f, 0.60f, 1.00f)*1.2f;
	lin += bac * vec3(0.40f, 0.50f, 0.60f);
	col *= lin;

	//col += s*0.1f*pow(fre,4.0f)*vec3(7.0f,5.0f,3.0f)*sh * pow( clamp(dot(nor,hal), 0.0f, 1.0f),16.0f);
	col += s *
		(0.04f + 0.96f*pow(clamp(1.0f + dot(hal, rd), 0.0f, 1.0f), 5.0f))*
		vec3(7.0f, 5.0f, 3.0f)*dif*sh*
		pow(clamp(dot(nor, hal), 0.0f, 1.0f), 16.0f);


	col += s * 0.1f*pow(fre, 4.0f)*vec3(0.4f, 0.5f, 0.6f)*smoothstep(0.0f, 0.6f, ref.y);

	// fog
	//float fo = 1.0f - exp(-0.0000000014f*t*t*t / SC);  // Short
	float fo = 1.0f - exp(-0.00000003f*t*t*t / SC);  // Far
	//vec3 fco = 0.65f*vec3(0.4f, 0.65f, 1.0f);// + 0.1f*vec3(1.0f,0.8f,0.5f)*pow( sundot, 4.0f );
	vec3 fco = get_sky(ro, rd);
	col = mix(col, fco, fo);

	// sun scatter
	//col += 0.3f*vec3(1.0f, 0.7f, 0.3f)*pow(sundot, 8.0f);

	return col;
}

inline vec3 calc_ray_color(vec3 ro, vec3 rd)
{
	vec3 col;
	float t = ray_march(ro, rd, 0.1f, max_view_dist);
	//col = vec3(t / max_view_dist, 0, 0);
	if (t >= max_view_dist)
		col = get_sky(ro, rd);
	else
	{
		vec3 pos = ro + t * rd;
		col = surface_color(pos, ro, t);
	}
	col = post_effects(col);
	return col;
}

//void mainImage(out vec4 fragColor, in vec2 fragCoord)
//{
//	float time = iTime * 0.1f - 0.1f + 0.3f + 4.0f*iMouse.x / iResolution.x;
//
//	// camera position
//	vec3 ro, ta; float cr, fl;
//	moveCamera(time, ro, ta, cr, fl);
//
//	// camera2world transform    
//	mat3 cam = setCamera(ro, ta, cr);
//
//	// pixel
//	vec2 p = (-iResolution.xy + 2.0f*fragCoord) / iResolution.y;
//
//	float t = kMaxT;
//	vec3 tot = vec3(0.0f);
//#if AA>1
//	for (int m = 0; m < AA; m++)
//		for (int n = 0; n < AA; n++)
//		{
//			// pixel coordinates
//			vec2 o = vec2(float(m), float(n)) / float(AA) - 0.5f;
//			vec2 s = (-iResolution.xy + 2.0f*(fragCoord + o)) / iResolution.y;
//#else    
//	vec2 s = p;
//#endif
//
//	// camera ray    
//	vec3 rd = cam * normalize(vec3(s, fl));
//
//	vec4 res = render(ro, rd);
//	t = min(t, res.w);
//
//	tot += res.xyz;
//#if AA>1
//		}
//tot /= float(AA*AA);
//#endif
//
//
////-------------------------------------
//// velocity vectors (through depth reprojection)
//	//-------------------------------------
//float vel = 0.0f;
//if (t < 0.0f)
//{
//	vel = -1.0f;
//}
//else
//{
//
//	// old camera position
//	float oldTime = time; // - 0.1f * 1.0f/30.0f; // 1/30 of a second blur
//	vec3 oldRo, oldTa; float oldCr, oldFl;
//	moveCamera(oldTime, oldRo, oldTa, oldCr, oldFl);
//	mat3 oldCam = setCamera(oldRo, oldTa, oldCr);
//
//	// world space
//#if AA>1
//	vec3 rd = cam * normalize(vec3(p, fl));
//#endif
//	vec3 wpos = ro + rd * t;
//	// camera space
//	vec3 cpos = vec3(dot(wpos - oldRo, oldCam[0]),
//		dot(wpos - oldRo, oldCam[1]),
//		dot(wpos - oldRo, oldCam[2]));
//	// ndc space
//	vec2 npos = oldFl * cpos.xy / cpos.z;
//	// screen space
//	vec2 spos = 0.5f + 0.5f*npos*vec2(iResolution.y / iResolution.x, 1.0f);
//
//
//	// compress velocity vector in a single float
//	vec2 uv = fragCoord / iResolution.xy;
//	spos = clamp(0.5f + 0.5f*(spos - uv) / 0.25f, 0.0f, 1.0f);
//	vel = floor(spos.x*255.0f) + floor(spos.y*255.0f)*256.0f;
//}
//
//fragColor = vec4(tot, vel);
//}