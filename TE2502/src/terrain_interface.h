
#ifdef __cplusplus
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
using namespace glm;
#else
#define inline 
#define static 
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

const float max_view_dist = 550.f;

//#include "../src/terrain_mountains.h"
#include "../src/terrain_rainforest.h"
