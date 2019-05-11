#ifdef __cplusplus
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include "algorithm/terrain_texture.hpp"
using namespace glm;

inline vec4 imageLoad(TerrainTexture* tex, ivec2 texel_coords)
{
	if (texel_coords.x < 0 || texel_coords.x >= tex->x ||
		texel_coords.y < 0 || texel_coords.y >= tex->y)
		return vec4(0, 0, 0, 0);
	
	vec4 res;
	res.x = (float)tex->texture_data[texel_coords.y * pixel_size + texel_coords.x + 0] / 255.0f;
	res.y = (float)tex->texture_data[texel_coords.y * pixel_size + texel_coords.x + 1] / 255.0f;
	res.z = (float)tex->texture_data[texel_coords.y * pixel_size + texel_coords.x + 2] / 255.0f;
	res.w = 1.0f;
}
#else
#define inline 
#define static 

// Terrain textures
layout(set = 0, binding = 0, rgba8) uniform image2D storage_tex0;
layout(set = 0, binding = 1, rgba8) uniform image2D storage_tex1;
layout(set = 0, binding = 2, rgba8) uniform image2D storage_tex2;
layout(set = 0, binding = 3, rgba8) uniform image2D storage_tex3;
#endif


// INTERFACE:
// THESE FUNCTIONS NEED TO BE DEFINED

// Given a camera position (ray origin) and ray direction, calculate the color for that ray
vec3 calc_ray_color(vec3 ro, vec3 rd);
// Given a position on the plane, return the height for that position
float terrain(vec2 p);
// Calculate the surface color at the given pos
vec3 surface_color(vec3 pos, vec3 cam_pos, float dist);
// Apply post processing effects on the final color
vec3 post_effects(vec3 rgb);

const float max_view_dist = 450.f;

//#include "../src/terrain_mountains.h"
#include "../src/terrain_rainforest.h"
