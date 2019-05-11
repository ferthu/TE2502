#pragma once
#include <cstdint>

const uint32_t max_terrain_textures = 4;
const uint32_t pixel_size = sizeof(uint8_t) * 4;
struct TerrainTexture
{
	int x;
	int y;
	uint8_t* texture_data;
};

// Terrain textures
extern TerrainTexture* storage_tex0;
extern TerrainTexture* storage_tex1;
extern TerrainTexture* storage_tex2;
extern TerrainTexture* storage_tex3;