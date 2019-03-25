#include <algorithm>
#include <cmath>
#include <array>

#include "vulkan/vulkan.h"
#include "glm/glm.hpp"
#include "cpu_triangulate.hpp"
#include "graphics/window.hpp"

#include "imgui/imgui.h"

namespace cputri
{
	#define TERRAIN_GENERATE_TOTAL_SIDE_LENGTH 1000
	#define TERRAIN_GENERATE_NUM_INDICES 8000
	#define TERRAIN_GENERATE_NUM_VERTICES 2000
	#define TERRAIN_GENERATE_NUM_NODES 16
	#define TERRAIN_GENERATE_GRID_SIDE 3
	#define TRIANGULATE_MAX_NEW_POINTS 1024
	#define QUADTREE_LEVELS 2
	#define MAX_BORDER_TRIANGLE_COUNT 500
	
	#define WORK_GROUP_SIZE 1

	const uvec3 gl_GlobalInvocationID{ 0, 0, 0 };

	typedef uint32_t uint;
	using namespace glm;

	uint num_indices;
	uint num_vertices;
	uint num_nodes;
	uint num_new_points;
	uint quadtree_levels;
	uint max_border_triangle_count;

	int max_points_per_refine = 9999999;
	int vistris_start = 0;
	int vistris_end = 99999999;
	bool show_cc = false;
	bool show = false;

	struct Triangle
	{
		vec2 circumcentre;
		float circumradius;  // TODO: Still needed?
		float circumradius2;
	};

	struct BufferNodeHeader
	{
		uint vertex_count;
		uint new_points_count;
		uint pad;

		vec2 min;
		vec2 max;

		uint border_count;
		std::array<uint, MAX_BORDER_TRIANGLE_COUNT> border_triangle_indices;
	};

	struct TerrainData
	{
		uint index_count;
		uint instance_count;
		uint first_index;
		int  vertex_offset;
		uint first_instance;

		// struct BufferNodeHeader {
		uint vertex_count;
		uint new_points_count;
		uint pad;

		vec2 min;
		vec2 max;

		uint temp_new_triangles_count;

		uint border_count;
		std::array<uint, MAX_BORDER_TRIANGLE_COUNT> border_triangle_indices;
		// }

		std::array<uint, TERRAIN_GENERATE_NUM_INDICES> indices;
		std::array<vec4, TERRAIN_GENERATE_NUM_VERTICES> positions;
		std::array<Triangle, TERRAIN_GENERATE_NUM_INDICES / 3> triangles;
		std::array<uint, TERRAIN_GENERATE_NUM_INDICES> triangle_connections;
		std::array<vec4, TRIANGULATE_MAX_NEW_POINTS> new_points;
		std::array<uint, TRIANGULATE_MAX_NEW_POINTS> new_points_triangles;
	};

	const uint quadtree_data_size = (1 << QUADTREE_LEVELS) * (1 << QUADTREE_LEVELS) + 4;
	const uint pad_size = 16 - (quadtree_data_size % 16);

	struct TerrainBuffer
	{
		std::array<uint, (1 << QUADTREE_LEVELS) * (1 << QUADTREE_LEVELS)> quadtree_index_map;
		vec2 quadtree_min;
		vec2 quadtree_max;
		std::array<uint, pad_size> pad;
		std::array<TerrainData, TERRAIN_GENERATE_NUM_NODES> data;
	};


	struct GenerateInfo
	{
		vec2 min;
		vec2 max;
		uint index;
	};
	struct Quadtree {
		// Number and array of indices to nodes that needs to generate terrain
		uint num_generate_nodes;
		GenerateInfo* generate_nodes;

		// Number and array of indices to nodes that needs to draw terrain
		uint num_draw_nodes;
		uint* draw_nodes;

		float total_side_length;
		uint levels;

		// Max number of active nodes
		uint64_t max_nodes;

		// For chunk i of m_buffer, m_buffer_index_filled[i] is true if that chunk is used by a node
		bool* buffer_index_filled;

		uint* node_index_to_buffer_index;

		uint64_t node_memory_size;
		vec2* quadtree_minmax;
	};
	Quadtree quadtree;

	const uint INVALID = ~0u;

	TerrainBuffer* terrain_buffer;
	uint cpu_index_buffer_size;


#pragma region TERRAINSTUFF
	float dump;
	vec2 addxy = vec2(1.0f, 0.0f);
	vec2 addyy = vec2(0.0f, 0.0f);
	vec2 addxx = vec2(1.0f, 1.0f);
	vec2 addyx = vec2(0.0f, 1.0f);
#define HASHSCALE1 .1031f

	float mix(float a, float b, float i)
	{
		return a * (1.0f - i) + b * i;
	}

	float clamp(float a, float min, float max)
	{
		if (a < min)
			a = min;
		if (a > max)
			a = max;

		return a;
	}

	const mat2 rotate2D = mat2(1.3623f, 1.7531f, -1.7131f, 1.4623f);

	//// Low-res stuff

	float hash12(vec2 p)
	{
		vec3 p3 = fract(vec3(p.x, p.y, p.x) * HASHSCALE1);
		p3 += dot(p3, vec3(p3.y, p3.z, p3.x) + 19.19f);
		return modff((p3.x + p3.y) * p3.z, &dump);
	}

	float noise(vec2 x)
	{
		vec2 p = floor(x);
		vec2 f = fract(x);
		f = f * f * (3.0f - 2.0f * f);

		float res = mix(mix(hash12(p), hash12(p + addxy), f.x),
			mix(hash12(p + addyx), hash12(p + addxx), f.x), f.y);
		return res;
	}

	float terrain(vec2 p)
	{
		vec2 pos = p * 0.05f;
		float w = (noise(pos * 0.25f) * 0.75f + 0.15f);
		w = 66.0f * w * w;
		vec2 dxy = vec2(0.0, 0.0);
		float f = .0;
		for (int i = 0; i < 5; i++)
		{
			f += w * noise(pos);
			w = -w * 0.4f;
			pos = rotate2D * pos;
		}
		float ff = noise(pos * 0.002f);

		f += pow(abs(ff), 5.0f) * 275.0f - 5.0f;
		return f;
	}

	float height_to_surface(vec3 p)
	{
		float h = terrain({ p.x, p.z });

		return -p.y - h;
	}

	///////

	//// High-res stuff
#define HASHSCALE3 vec3(.1031f, .1030f, .0973f)
#define HASHSCALE4 vec4(1031f, .1030f, .0973f, .1099f)

	vec2 hash22(vec2 p)
	{
		vec3 p3 = fract(vec3(p.x, p.y, p.x) * HASHSCALE3);
		p3 += dot(p3, vec3(p3.y, p3.z, p3.x) + 19.19f);
		return fract((vec2(p3.x, p3.x) + vec2(p3.y, p3.z)) * vec2(p3.z, p3.y));

	}

	vec2 noise2(vec2 x)
	{
		vec2 p = floor(x);
		vec2 f = fract(x);
		f = f * f * (3.0f - 2.0f * f);
		float n = p.x + p.y * 57.0f;
		vec2 res = mix(mix(hash22(p), hash22(p + addxy), f.x),
			mix(hash22(p + addyx), hash22(p + addxx), f.x), f.y);
		return res;
	}

	//--------------------------------------------------------------------------
	// High def version only used for grabbing normal information.
	float terrain2(vec2 p)
	{
		// There's some real magic numbers in here! 
		// The noise calls add large mountain ranges for more variation over distances...
		vec2 pos = p * 0.05f;
		float w = (noise(pos * 0.25f) * 0.75f + 0.15f);
		w = 66.0f * w * w;
		vec2 dxy = vec2(0.0f, 0.0f);
		float f = 0.0f;
		for (int i = 0; i < 5; i++)
		{
			f += w * noise(pos);
			w = -w * 0.4f;	//...Flip negative and positive for varition	   
			pos = rotate2D * pos;
		}
		float ff = noise(pos * 0.002f);
		f += powf(abs(ff), 5.0f) * 275.0f - 5.0f;


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

	vec3 sun_light = normalize(vec3(0.4, 0.4, 0.48));
	vec3 sun_color = vec3(1.0, .9, .83);
	float specular = 0.0;
	float ambient;

	// Calculate sun light...
	void do_lighting(vec3& mat, vec3 pos, vec3 normal, vec3 eye_dir, float dis)
	{
		float h = dot(sun_light, normal);
		float c = std::max(h, 0.0f) + ambient;
		mat = mat * sun_color * c;
		// Specular...
		if (h > 0.0)
		{
			vec3 R = reflect(sun_light, normal);
			float spec_amount = pow(std::max(dot(R, normalize(eye_dir)), 0.0f), 3.0f) * specular;
			mat = mix(mat, sun_color, spec_amount);
		}
	}

	// Hack the height, position, and normal data to create the coloured landscape
	vec3 surface_color(vec3 pos, vec3 normal, float dis, vec3 cam_pos)
	{
		vec3 mat;
		specular = 0.0f;
		ambient = 0.1f;
		vec3 dir = normalize(pos - cam_pos);

		vec3 mat_pos = pos * 2.0f;// ... I had change scale halfway though, this lazy multiply allow me to keep the graphic scales I had

		float dis_sqrd = dis * dis;// Squaring it gives better distance scales.

		float f = clamp(noise(vec2(mat_pos.x, mat_pos.z) * 0.05f), 0.0f, 1.0f);
		f += noise(vec2(mat_pos.x, mat_pos.z) * 0.1f + vec2(normal.y, normal.z) * 1.08f) * 0.85f;
		f *= 0.55f;
		vec3 m = mix(vec3(.63*f + .2, .7*f + .1, .7*f + .1), vec3(f*.43 + .1, f*.3 + .2, f*.35 + .1), f * 0.65f);
		mat = m * vec3(f * m.x + .36f, f * m.y + .30f, f * m.z + .28f);
		// Should have used smoothstep to add colours, but left it using 'if' for sanity...
		if (normal.y < .5f)
		{
			float v = normal.y;
			float c = (0.5f - normal.y) * 4.0f;
			c = clamp(c*c, 0.1f, 1.0f);
			f = noise(vec2(mat_pos.x * 0.09f, mat_pos.z * 0.095f) + vec2(mat_pos.y, mat_pos.y) * 0.15f);
			f += noise(vec2(mat_pos.x * 2.233f, mat_pos.z * 2.23f)) * 0.5f;
			mat = mix(mat, vec3(.4f * f), c);
			specular += 0.1f;
		}

		// Grass. Use the normal to decide when to plonk grass down...
		if (normal.y > .65f)
		{

			m = vec3(noise(vec2(mat_pos.x, mat_pos.z) * 0.023f) * 0.5f + 0.15f, noise(vec2(mat_pos.x, mat_pos.z) * 0.03f) * 0.6f + 0.25f, 0.0f);
			m *= (normal.y - 0.65f) * 0.6f;
			mat = mix(mat, m, clamp((normal.y - .65f) * 1.3f * (45.35f - mat_pos.y)* 0.1f, 0.0f, 1.0f));
		}

		// Snow topped mountains...
		if (mat_pos.y > 80.0f && normal.y > .42f)
		{
			float snow = clamp((mat_pos.y - 80.0f - noise(vec2(mat_pos.x, mat_pos.z) * 0.1f) * 28.0f) * 0.035f, 0.0f, 1.0f);
			mat = mix(mat, vec3(.7f, .7f, .8f), snow);
			specular += snow;
			ambient += snow * 0.3f;
		}

		do_lighting(mat, pos, normal, dir, dis_sqrd);

		return mat;
	}

	// Some would say, most of the magic is done in post! :D
	vec3 post_effects(vec3 rgb)
	{
		return (1.0f - exp(-rgb * 6.0f)) * 1.0024f;
	}
#pragma endregion
	   

#pragma region CC

	// Function to find the line given two points
	void line_from_points(vec2 p1, vec2 p2, float& a, float& b, float& c)
	{
		a = p2.y - p1.y;
		b = p1.x - p2.x;
		c = a * p1.x + b * p2.y;
	}

	// Function which converts the input line to its 
	// perpendicular bisector. It also inputs the points 
	// whose mid-point lies on the bisector 
	void perpendicular_bisector_from_line(vec2 p1, vec2 p2, float& a, float& b, float& c)
	{
		vec2 mid_point = vec2((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);

		// c = -bx + ay 
		c = -b * mid_point.x + a * mid_point.y;

		float temp = a;
		a = -b;
		b = temp;
	}

	// Returns the intersection point of two lines 
	vec2 line_line_intersection(float a1, float b1, float c1, float a2, float b2, float c2)
	{
		float determinant = a1 * b2 - a2 * b1;

		float x = (b2 * c1 - b1 * c2) / determinant;
		float y = (a1 * c2 - a2 * c1) / determinant;

		return vec2(x, y);
	}

	vec2 find_circum_center(vec2 P, vec2 Q, vec2 R)
	{
		// Line PQ is represented as ax + by = c 
		float a, b, c;
		line_from_points(P, Q, a, b, c);

		// Line QR is represented as ex + fy = g 
		float e, f, g;
		line_from_points(Q, R, e, f, g);

		// Converting lines PQ and QR to perpendicular 
		// vbisectors. After this, L = ax + by = c 
		// M = ex + fy = g 
		perpendicular_bisector_from_line(P, Q, a, b, c);
		perpendicular_bisector_from_line(Q, R, e, f, g);

		// The point of intersection of L and M gives 
		// the circumcenter 
		return line_line_intersection(a, b, c, e, f, g);
	}

	float find_circum_radius_squared(float a, float b, float c)
	{
		return (a * a * b * b * c * c) / ((a + b + c) * (b + c - a) * (c + a - b) * (a + b - c));
	}

	float find_circum_radius_squared(vec2 P, vec2 Q, vec2 R)
	{
		float a = distance(P, Q);
		float b = distance(Q, R);
		float c = distance(R, P);
		return (a * a * b * b * c * c) / ((a + b + c) * (b + c - a) * (c + a - b) * (a + b - c));
	}


#pragma endregion

	bool clip(vec4 p)
	{
		return (abs(p.x) <= p.w &&
			abs(p.y) <= p.w &&
			abs(p.z) <= p.w);
	}

	float curvature(vec3 p, vec3 camera_position)
	{
		const float pi = 3.1415f;

		float camera_distance = distance(p, camera_position);

		float sample_step = 5.5f + pow(camera_distance * 0.5f, 0.6f);
		const float gaussian_width = 1.0f;
		const int filter_radius = 2;	// Side length of grid is filter_radius * 2 + 1
		const int filter_side = filter_radius * 2 + 1;
		float log_filter[filter_side * filter_side];

		/////////////////////////////////////////
		float sum = 0.0f;

		for (int x = -filter_radius; x <= filter_radius; x++)
		{
			for (int y = -filter_radius; y <= filter_radius; y++)
			{
				// https://homepages.inf.ed.ac.uk/rbf/HIPR2/log.htm
				float t = -((x * x + y * y) / (2.0f * gaussian_width * gaussian_width));
				float log = -(1.0f / (pi * powf(gaussian_width, 4.0f))) * (1.0f + t) * exp(t);

				log_filter[(y + filter_radius) * filter_side + (x + filter_radius)] = log;
				sum += log;
			}
		}

		// Normalize filter
		float correction = 1.0f / sum;
		for (uint i = 0; i < filter_side * filter_side; i++)
		{
			log_filter[i] *= correction;
		}

		float curvature = 0.0f;

		for (int x = -filter_radius; x <= filter_radius; x++)
		{
			for (int y = -filter_radius; y <= filter_radius; y++)
			{
				curvature += terrain(vec2(p.x, p.z) + vec2(sample_step * x, sample_step * y)) * log_filter[(y + filter_radius) * filter_side + (x + filter_radius)];
			}
		}

		// Normalize for height
		curvature -= terrain(vec2(p.x, p.z));

		return abs(curvature);
	}


	void setup(TFile& tfile)
	{
		num_indices = TERRAIN_GENERATE_NUM_INDICES;
		num_vertices = TERRAIN_GENERATE_NUM_VERTICES;
		num_nodes = TERRAIN_GENERATE_NUM_NODES;
		num_new_points = TRIANGULATE_MAX_NEW_POINTS;
		quadtree_levels = QUADTREE_LEVELS;
		max_border_triangle_count = MAX_BORDER_TRIANGLE_COUNT;

		assert(quadtree_levels > 0);

		quadtree.buffer_index_filled = new bool[num_nodes];
		memset(quadtree.buffer_index_filled, 0, num_nodes * sizeof(bool));

		quadtree.num_generate_nodes = 0;
		quadtree.generate_nodes = new GenerateInfo[num_nodes];

		quadtree.num_draw_nodes = 0;
		quadtree.draw_nodes = new uint[num_nodes];

		quadtree.node_memory_size =
			sizeof(VkDrawIndexedIndirectCommand) +
			sizeof(BufferNodeHeader) +
			num_indices * sizeof(uint) + // Indices
			num_vertices * sizeof(vec4) + // Vertices
			(num_indices / 3) * sizeof(Triangle) + // Circumcentre and circumradius
			num_indices * sizeof(uint) + // Triangle connectivity
			num_new_points * sizeof(vec4) + // New points
			num_new_points * sizeof(uint); // New point triangle index

		// Add space for an additional two vec2's to store the quadtree min and max
		cpu_index_buffer_size = (1 << quadtree_levels) * (1 << quadtree_levels) * sizeof(uint) + sizeof(vec2) * 2;
		cpu_index_buffer_size += 64 - (cpu_index_buffer_size % 64);

		terrain_buffer = (TerrainBuffer*) new char[cpu_index_buffer_size + quadtree.node_memory_size * num_nodes + 1000];

		quadtree.node_index_to_buffer_index = (uint*)terrain_buffer;

		// Point to the end of cpu index buffer
		quadtree.quadtree_minmax = (vec2*) (((char*)quadtree.node_index_to_buffer_index) + (1 << quadtree_levels) * (1 << quadtree_levels) * sizeof(uint));

		quadtree.total_side_length = TERRAIN_GENERATE_TOTAL_SIDE_LENGTH;
		float half_length = quadtree.total_side_length * 0.5f;
		quadtree.quadtree_minmax[0] = vec2(-half_length, -half_length);
		quadtree.quadtree_minmax[1] = vec2(half_length, half_length);

		// (1 << levels) is number of nodes per axis
		memset(quadtree.node_index_to_buffer_index, INVALID, (1 << quadtree_levels) * (1 << quadtree_levels) * sizeof(uint));
	}

	void destroy()
	{
		delete[] terrain_buffer;
		delete[] quadtree.draw_nodes;
		delete[] quadtree.generate_nodes;
		delete[] quadtree.buffer_index_filled;
	}

	int lolz = 0;
	int temp = 0;
	int vertices_per_refine = 1;
	int border = -1;
	int show_connections = -1;

	void run(DebugDrawer& dd, Camera& camera, Window& window)
	{
		Frustum fr = camera.get_frustum();
		intersect(fr, dd);

		draw_terrain(fr, dd, camera);

		static float threshold = 0.0f;
		static float area_mult = 1.0f;
		static float curv_mult = 1.0f;

		ImGui::Begin("Lol");
		ImGui::SliderInt("Lolz", &lolz, 0, 3);
		ImGui::SliderInt("Index", &temp, -1, 15);
		ImGui::SliderInt("Vertices per refine", &vertices_per_refine, 1, 10);
		ImGui::SliderInt("Border", &border, -1, 3);
		ImGui::SliderInt("Show Connections", &show_connections, -1, num_indices / 3);
		ImGui::End();

		ImGui::Begin("cputri");
		if (ImGui::Button("Refine"))
		{
			process_triangles(camera, window, threshold, area_mult, curv_mult);
			triangulate();
		}
		if (ImGui::Button("Clear Terrain"))
		{
			clear_terrain();
		}

		ImGui::DragFloat("Area mult", &area_mult, 0.01f, 0.0f, 50.0f);
		ImGui::DragFloat("Curv mult", &curv_mult, 0.01f, 0.0f, 50.0f);
		ImGui::DragFloat("Threshold", &threshold, 0.01f, 0.0f, 50.0f);

		ImGui::Checkbox("Show", &show);
		ImGui::Checkbox("Show CC", &show_cc);

		ImGui::DragInt("Max Points", &max_points_per_refine);
		ImGui::DragInt("Vistris Start", &vistris_start, 0.1f);
		ImGui::DragInt("Vistris End", &vistris_end, 0.1f);
		ImGui::End();

	}

	uint find_chunk()
	{
		for (uint i = 0; i < num_nodes; i++)
		{
			if (!quadtree.buffer_index_filled[i])
				return i;
		}

		return INVALID;
	}

	uint get_offset(uint node_x, uint node_z)
	{
		assert(node_x >= 0u && node_x < (1u << quadtree_levels));
		assert(node_z >= 0u && node_z < (1u << quadtree_levels));

		return quadtree.node_index_to_buffer_index[node_x + (1u << quadtree_levels) * node_z];
	}

	uint64_t get_index_offset_of_node(uint i)
	{
		return get_offset_of_node(i) + sizeof(VkDrawIndexedIndirectCommand) + sizeof(BufferNodeHeader);
	}

	uint64_t get_vertex_offset_of_node(uint i)
	{
		return get_index_offset_of_node(i) + num_indices * sizeof(uint);

	}

	uint64_t get_offset_of_node(uint i)
	{
		return cpu_index_buffer_size + i * quadtree.node_memory_size;
	}

	void clear_terrain()
	{
		memset(quadtree.node_index_to_buffer_index, INVALID, (1 << quadtree_levels) * (1 << quadtree_levels) * sizeof(uint));
		memset(quadtree.buffer_index_filled, 0, num_nodes * sizeof(bool));
	}

	void triangulate()
	{
		const int nodes_per_side = 1 << quadtree_levels;
		for (int yy = 0; yy < 3; ++yy)
		{
			for (int xx = 0; xx < 3; ++xx)
			{
				for (int ty = yy; ty < nodes_per_side; ty += 3)
				{
					for (int tx = xx; tx < nodes_per_side; tx += 3)
					{
						triangulate_shader(quadtree.node_index_to_buffer_index[ty * nodes_per_side + tx]);
					}
				}
			}
		}
	}

	void process_triangles(Camera& camera, Window& window, float em_threshold, float area_multiplier, float curvature_multiplier)
	{
		// Nonupdated terrain
		for (uint i = 0; i < quadtree.num_draw_nodes; i++)
		{
			//if (quadtree.draw_nodes[i] == lolz)
			triangle_process_shader(
				camera.get_vp(),
				vec4(camera.get_pos(), 0),
				window.get_size(),
				em_threshold,
				area_multiplier,
				curvature_multiplier,
				quadtree.draw_nodes[i]);
		}

		// Newly generated terrain
		for (uint i = 0; i < quadtree.num_generate_nodes; i++)
		{
			triangle_process_shader(
				camera.get_vp(),
				vec4(camera.get_pos(), 0),
				window.get_size(),
				em_threshold,
				area_multiplier,
				curvature_multiplier,
				quadtree.generate_nodes[i].index);
		}
	}

	void intersect(Frustum& frustum, DebugDrawer& dd)
	{
		quadtree.num_generate_nodes = 0;
		quadtree.num_draw_nodes = 0;

		float half_length = quadtree.total_side_length * 0.5f;

		// Gather status of nodes
		intersect(frustum, dd, { {-half_length, -half_length},
			{half_length, half_length} }, 0, 0, 0);

		for (uint i = 0; i < quadtree.num_generate_nodes; i++)
		{
			generate_shader(quadtree.generate_nodes[i].index, quadtree.generate_nodes[i].min, quadtree.generate_nodes[i].max);
		}
	}

	void intersect(Frustum& frustum, DebugDrawer& dd, AabbXZ aabb, uint level, uint x, uint y)
	{
		if (level == quadtree_levels)
		{
			//float minx = aabb.m_min.x + (aabb.m_max.x - aabb.m_min.x) * 0.05f;
			//float maxx = aabb.m_min.x + (aabb.m_max.x - aabb.m_min.x) * 0.95f;

			//float minz = aabb.m_min.y + (aabb.m_max.y - aabb.m_min.y) * 0.05f;
			//float maxz = aabb.m_min.y + (aabb.m_max.y - aabb.m_min.y) * 0.95f;

			//dd.draw_line({ minx, 0, minz }, { minx, 0, maxz }, { 1, 1, 0 });
			//dd.draw_line({ minx, 0, minz }, { maxx, 0, minz }, { 1, 1, 0 });

			//dd.draw_line({ maxx, 0, maxz }, { maxx, 0, minz }, { 1, 1, 0 });
			//dd.draw_line({ maxx, 0, maxz }, { minx, 0, maxz }, { 1, 1, 0 });

			// Index into m_node_index_to_buffer_index
			uint index = (1 << quadtree_levels) * y + x;
			if (quadtree.node_index_to_buffer_index[index] == INVALID)
			{
				// Visible node does not have data

				uint new_index = find_chunk();
				if (new_index != INVALID)
				{
					quadtree.buffer_index_filled[new_index] = true;
					quadtree.node_index_to_buffer_index[index] = new_index;

					// m_buffer[new_index] needs to be filled with data
					quadtree.generate_nodes[quadtree.num_generate_nodes].index = new_index;
					quadtree.generate_nodes[quadtree.num_generate_nodes].min = aabb.m_min;
					quadtree.generate_nodes[quadtree.num_generate_nodes].max = aabb.m_max;
					quadtree.num_generate_nodes++;
				}
				else
				{
					// No space left! Ignore for now...
				}
			}
			else
			{
				// Visible node has data, draw it

				// m_buffer[m_node_index_to_buffer_index[index]] needs to be drawn
				quadtree.draw_nodes[quadtree.num_draw_nodes] = quadtree.node_index_to_buffer_index[index];
				quadtree.num_draw_nodes++;
			}

			return;
		}

		// This node is visible, check children
		if (frustum_aabbxz_intersection(frustum, aabb))
		{
			vec2 mid = (aabb.m_min + aabb.m_max) * 0.5f;
			float mid_x = (aabb.m_min.x + aabb.m_max.x) * 0.5f;
			float mid_z = (aabb.m_min.y + aabb.m_max.y) * 0.5f;

			intersect(frustum, dd, { {aabb.m_min.x, aabb.m_min.y}, {mid.x, mid.y} }, level + 1, (x << 1), (y << 1));
			intersect(frustum, dd, { {aabb.m_min.x, mid_z}, {mid.x, aabb.m_max.y} }, level + 1, (x << 1), (y << 1) + 1);
			intersect(frustum, dd, { {mid_x, aabb.m_min.y}, {aabb.m_max.x, mid_z} }, level + 1, (x << 1) + 1, (y << 1));
			intersect(frustum, dd, { {mid.x, mid.y}, {aabb.m_max.x, aabb.m_max.y} }, level + 1, (x << 1) + 1, (y << 1) + 1);
		}
	}

	void draw_terrain(Frustum& frustum, DebugDrawer& dd, Camera& camera)
	{
		if (show)
		{
			for (size_t ii = 0; ii < num_nodes; ii++)
			{
				if (quadtree.buffer_index_filled[ii] && (ii == temp || temp == -1))
				{
					for (uint ind = vistris_start * 3; ind < terrain_buffer->data[ii].index_count && ind < (uint)vistris_end * 3; ind += 3)
					{
						const float height = -100.0f;

						vec3 p0 = vec3(terrain_buffer->data[ii].positions[terrain_buffer->data[ii].indices[ind + 0]]) + vec3(0.0f, height, 0.0f);
						vec3 p1 = vec3(terrain_buffer->data[ii].positions[terrain_buffer->data[ii].indices[ind + 1]]) + vec3(0.0f, height, 0.0f);
						vec3 p2 = vec3(terrain_buffer->data[ii].positions[terrain_buffer->data[ii].indices[ind + 2]]) + vec3(0.0f, height, 0.0f);

						vec3 mid = (p0 + p1 + p2) / 3.0f;

						dd.draw_line(p0, p1, { 1, 0, 0 });
						dd.draw_line(p1, p2, { 1, 0, 0 });
						dd.draw_line(p2, p0, { 1, 0, 0 });

						//dd.draw_line(mid, p0, { 0, 1, 0 });
						//dd.draw_line(mid, p1, { 0, 1, 0 });
						//dd.draw_line(mid, p2, { 0, 1, 0 });

						if (show_cc)
						{
							const uint tri_index = ind / 3;
							const uint steps = 20;
							const float angle = 3.14159265f * 2.0f / steps;
							for (uint jj = 0; jj < steps + 1; ++jj)
							{
								float cc_radius = terrain_buffer->data[ii].triangles[tri_index].circumradius;
								vec3 cc_mid = { terrain_buffer->data[ii].triangles[tri_index].circumcentre.x, mid.y, terrain_buffer->data[ii].triangles[tri_index].circumcentre.y };

								dd.draw_line(cc_mid + vec3(sinf(angle * jj) * cc_radius, 0.0f, cosf(angle * jj) * cc_radius),
									cc_mid + vec3(sinf(angle * (jj + 1)) * cc_radius, 0.0f, cosf(angle * (jj + 1)) * cc_radius),
									{ 0, 0, 1 });
							}
						}

						if (show_connections == ind / 3)
						{
							glm::vec3 h = { 0, -20, 0 };

							dd.draw_line(p0 + h, p1 + h, { 1, 0, 0 });
							dd.draw_line(p1 + h, p2 + h, { 0, 1, 0 });
							dd.draw_line(p2 + h, p0 + h, { 0, 0, 1 });

							glm::vec3 n0 = mid + h;
							glm::vec3 n1 = mid + h;
							glm::vec3 n2 = mid + h;

							if (terrain_buffer->data[ii].triangle_connections[ind + 0] != INVALID)
							{
								uint neighbour_ind = terrain_buffer->data[ii].triangle_connections[ind + 0];
								n0 = (terrain_buffer->data[ii].positions[terrain_buffer->data[ii].indices[neighbour_ind * 3 + 0]] + 
									  terrain_buffer->data[ii].positions[terrain_buffer->data[ii].indices[neighbour_ind * 3 + 1]] + 
									  terrain_buffer->data[ii].positions[terrain_buffer->data[ii].indices[neighbour_ind * 3 + 2]]) / 3.0f;
								n0 += glm::vec3(0, height, 0) + h;
							}
							if (terrain_buffer->data[ii].triangle_connections[ind + 1] != INVALID)
							{
								uint neighbour_ind = terrain_buffer->data[ii].triangle_connections[ind + 1];
								n1 = (terrain_buffer->data[ii].positions[terrain_buffer->data[ii].indices[neighbour_ind * 3 + 0]] +
									terrain_buffer->data[ii].positions[terrain_buffer->data[ii].indices[neighbour_ind * 3 + 1]] +
									terrain_buffer->data[ii].positions[terrain_buffer->data[ii].indices[neighbour_ind * 3 + 2]]) / 3.0f;
								n1 += glm::vec3(0, height, 0) + h;
							}
							if (terrain_buffer->data[ii].triangle_connections[ind + 2] != INVALID)
							{
								uint neighbour_ind = terrain_buffer->data[ii].triangle_connections[ind + 2];
								n2 = (terrain_buffer->data[ii].positions[terrain_buffer->data[ii].indices[neighbour_ind * 3 + 0]] +
									terrain_buffer->data[ii].positions[terrain_buffer->data[ii].indices[neighbour_ind * 3 + 1]] +
									terrain_buffer->data[ii].positions[terrain_buffer->data[ii].indices[neighbour_ind * 3 + 2]]) / 3.0f;
								n2 += glm::vec3(0, height, 0) + h;
							}


							dd.draw_line(mid + h, n0, { 1, 0, 0 });
							dd.draw_line(mid + h, n1, { 0, 1, 0 });
							dd.draw_line(mid + h, n2, { 0, 0, 1 });
						}
					}
					//for (int tt = 0; tt < terrain_buffer->data[ii].vertex_count; ++tt)
					//{
					//	vec3 p = terrain_buffer->data[ii].positions[tt] + vec4(0, -100, 0, 0);
					//	dd.draw_line(p, p + vec3(0, -50, 0), vec3(1, 0, 1));
					//}

					//vec2 min = terrain_buffer->data[ii].min;
					//vec2 max = terrain_buffer->data[ii].max;
					//dd.draw_line({ min.x - terrain_buffer->data[ii].border_max[3], -150, min.y }, { min.x - terrain_buffer->data[ii].border_max[3], -150, max.y }, { 1, 1, 1 });
					//dd.draw_line({ max.x + terrain_buffer->data[ii].border_max[1], -150, min.y }, { max.x + terrain_buffer->data[ii].border_max[1], -150, max.y }, { 1, 1, 1 });
					//dd.draw_line({ min.x, -150, max.y + terrain_buffer->data[ii].border_max[0] }, { max.x, -150, max.y + terrain_buffer->data[ii].border_max[0] }, { 1, 1, 1 });
					//dd.draw_line({ min.x, -150, min.y - terrain_buffer->data[ii].border_max[2] }, { max.x, -150, min.y - terrain_buffer->data[ii].border_max[2] }, { 1, 1, 1 });

					//for (uint bb = 0; bb < 4; ++bb)
					//{
					//	if (bb == border || border == -1)
					//		for (uint bt = 0; bt < terrain_buffer->data[ii].border_count[bb]; ++bt)
					//		{
					//			const float height = -120.0f;
					//			uint ind = terrain_buffer->data[ii].border_triangle_indices[bb * max_border_triangle_count + bt] * 3;
					//			vec3 p0 = vec3(terrain_buffer->data[ii].positions[terrain_buffer->data[ii].indices[ind + 0]]) + vec3(0.0f, height, 0.0f);
					//			vec3 p1 = vec3(terrain_buffer->data[ii].positions[terrain_buffer->data[ii].indices[ind + 1]]) + vec3(0.0f, height, 0.0f);
					//			vec3 p2 = vec3(terrain_buffer->data[ii].positions[terrain_buffer->data[ii].indices[ind + 2]]) + vec3(0.0f, height, 0.0f);

					//			dd.draw_line(p0, p1, { 0, 0, 1 });
					//			dd.draw_line(p1, p2, { 0, 0, 1 });
					//			dd.draw_line(p2, p0, { 0, 0, 1 });
					//		}
					//}
				}
			}
		}
	}


	void generate_shader(uint node_index, vec2 min, vec2 max)
	{
		const uint GRID_SIDE = TERRAIN_GENERATE_GRID_SIDE;

		if (gl_GlobalInvocationID.x == 0)
		{
			terrain_buffer->data[node_index].index_count = 6 * (GRID_SIDE - 1) * (GRID_SIDE - 1);
			terrain_buffer->data[node_index].instance_count = 1;
			terrain_buffer->data[node_index].first_index = 0;
			terrain_buffer->data[node_index].vertex_offset = 0;
			terrain_buffer->data[node_index].first_instance = 0;

			terrain_buffer->data[node_index].vertex_count = GRID_SIDE * GRID_SIDE;
			terrain_buffer->data[node_index].new_points_count = 0;

			terrain_buffer->data[node_index].min = min;
			terrain_buffer->data[node_index].max = max;

			terrain_buffer->data[node_index].border_count = 0;
		}

		//barrier();
		//memoryBarrierBuffer();

		const vec2 node_min = terrain_buffer->data[node_index].min;
		const vec2 node_max = terrain_buffer->data[node_index].max;
		const float side = node_max.x - node_min.x;

		// Positions
		uint i = gl_GlobalInvocationID.x;
		while (i < GRID_SIDE * GRID_SIDE)
		{
			float x = min.x + ((i % GRID_SIDE) / float(GRID_SIDE - 1)) * (max.x - min.x);
			float z = min.y + float(i / GRID_SIDE) / float(GRID_SIDE - 1) * (max.y - min.y);

			//terrain_buffer->data[node_index].positions[i] = vec4(x, -terrain(vec2(x, z)) - 0.5f, z, 1.0f);
			terrain_buffer->data[node_index].positions[i] = vec4(x + ((max.x - min.x) / GRID_SIDE) * 0.5 * ((i / GRID_SIDE) % 2), -terrain(vec2(x, z)) - 0.5, z, 1.0);

			i += WORK_GROUP_SIZE;
		}

		//barrier();
		//memoryBarrierBuffer();

		// Triangles
		i = gl_GlobalInvocationID.x;
		while (i < (GRID_SIDE - 1) * (GRID_SIDE - 1))
		{
			const uint y = i / (GRID_SIDE - 1);
			const uint x = i % (GRID_SIDE - 1);
			const uint index = y * GRID_SIDE + x;

			uint indices[6];

			const uint offset = i * 6;
			const uint triangle_index = i * 2;
			const uint triangles_per_side = (GRID_SIDE - 1) * 2;
			if ((y % 2) == 0)
			{
				// Indices
				indices[0] = index;
				indices[1] = index + GRID_SIDE;
				indices[2] = index + 1;

				indices[3] = index + 1;
				indices[4] = index + GRID_SIDE;
				indices[5] = index + GRID_SIDE + 1;

				// Triangle connections
				terrain_buffer->data[node_index].triangle_connections[offset + 0] = x > 0 ? triangle_index - 1 : INVALID;
				terrain_buffer->data[node_index].triangle_connections[offset + 1] = triangle_index + 1;
				terrain_buffer->data[node_index].triangle_connections[offset + 2] = y > 0 ? triangle_index - triangles_per_side + 1 : INVALID;

				terrain_buffer->data[node_index].triangle_connections[offset + 3] = triangle_index;
				terrain_buffer->data[node_index].triangle_connections[offset + 4] = y < GRID_SIDE - 2 ? triangle_index + triangles_per_side : INVALID;
				terrain_buffer->data[node_index].triangle_connections[offset + 5] = x < GRID_SIDE - 2 ? triangle_index + 2 : INVALID;
			}
			else
			{
				// Indices
				indices[0] = index;
				indices[1] = index + GRID_SIDE + 1;
				indices[2] = index + 1;

				indices[3] = index;
				indices[4] = index + GRID_SIDE;
				indices[5] = index + GRID_SIDE + 1;

				// Triangle connections
				terrain_buffer->data[node_index].triangle_connections[offset + 0] = triangle_index + 1;
				terrain_buffer->data[node_index].triangle_connections[offset + 1] = x < GRID_SIDE - 2 ? triangle_index + 3 : INVALID;
				terrain_buffer->data[node_index].triangle_connections[offset + 2] = triangle_index - triangles_per_side + 1;

				terrain_buffer->data[node_index].triangle_connections[offset + 3] = x > 0 ? triangle_index - 2 : INVALID;
				terrain_buffer->data[node_index].triangle_connections[offset + 4] = y < GRID_SIDE - 2 ? triangle_index + triangles_per_side : INVALID;
				terrain_buffer->data[node_index].triangle_connections[offset + 5] = triangle_index;
			}

			// Indices
			terrain_buffer->data[node_index].indices[offset + 0] = indices[0];
			terrain_buffer->data[node_index].indices[offset + 1] = indices[1];
			terrain_buffer->data[node_index].indices[offset + 2] = indices[2];

			terrain_buffer->data[node_index].indices[offset + 3] = indices[3];
			terrain_buffer->data[node_index].indices[offset + 4] = indices[4];
			terrain_buffer->data[node_index].indices[offset + 5] = indices[5];

			// Positions
			vec2 P1 = vec2(terrain_buffer->data[node_index].positions[indices[0]].x, terrain_buffer->data[node_index].positions[indices[0]].z);
			vec2 Q1 = vec2(terrain_buffer->data[node_index].positions[indices[1]].x, terrain_buffer->data[node_index].positions[indices[1]].z);
			vec2 R1 = vec2(terrain_buffer->data[node_index].positions[indices[2]].x, terrain_buffer->data[node_index].positions[indices[2]].z);
										 
			vec2 P2 = vec2(terrain_buffer->data[node_index].positions[indices[3]].x, terrain_buffer->data[node_index].positions[indices[3]].z);
			vec2 Q2 = vec2(terrain_buffer->data[node_index].positions[indices[4]].x, terrain_buffer->data[node_index].positions[indices[4]].z);
			vec2 R2 = vec2(terrain_buffer->data[node_index].positions[indices[5]].x, terrain_buffer->data[node_index].positions[indices[5]].z);

			// Circumcentres
			terrain_buffer->data[node_index].triangles[triangle_index + 0].circumcentre = find_circum_center(P1, Q1, R1);
			terrain_buffer->data[node_index].triangles[triangle_index + 1].circumcentre = find_circum_center(P2, Q2, R2);

			// Circumradii
			const float radius12 = find_circum_radius_squared(P1, Q1, R1);
			const float radius22 = find_circum_radius_squared(P2, Q2, R2);
			terrain_buffer->data[node_index].triangles[triangle_index + 0].circumradius2 = radius12;
			terrain_buffer->data[node_index].triangles[triangle_index + 0].circumradius = sqrt(radius12);
			terrain_buffer->data[node_index].triangles[triangle_index + 1].circumradius2 = radius22;
			terrain_buffer->data[node_index].triangles[triangle_index + 1].circumradius = sqrt(radius22);

			i += WORK_GROUP_SIZE;
		}
	}

#pragma region TRIANGLE_PROCESS
	const uint max_new_normal_points = TRIANGULATE_MAX_NEW_POINTS / WORK_GROUP_SIZE;
	static /*shared*/ std::array<uint, WORK_GROUP_SIZE> s_counts;
	static /*shared*/ uint s_total;

	void triangle_process_shader(mat4 vp, vec4 camera_position, vec2 screen_size, float threshold, float area_multiplier, float curvature_multiplier, uint node_index)
	{
		if (terrain_buffer->data[node_index].new_points_count != 0)
			return;
		terrain_buffer->data[node_index].new_points_count = 0;
		std::array<vec4, max_new_normal_points + 1> new_points;
		std::array<uint, max_new_normal_points + 1> triangle_indices;

		const uint thid = gl_GlobalInvocationID.x;

		const vec2 node_min = terrain_buffer->data[node_index].min;
		const vec2 node_max = terrain_buffer->data[node_index].max;
		const float side = node_max.x - node_min.x;

		const int neighbur_indexing_x[4] = { 0, 1, 0, -1 };
		const int neighbur_indexing_y[4] = { 1, 0, -1, 0 };

		const int cx = int((node_min.x - terrain_buffer->quadtree_min.x + 1) / side);  // current node x
		const int cy = int((node_min.y - terrain_buffer->quadtree_min.y + 1) / side);  // current node z/y

		const int nodes_per_side = 1 << quadtree_levels;

		const uint index_count = terrain_buffer->data[node_index].index_count;

		uint new_point_count = 0;

		// For every triangle
		for (uint i = thid * 3; i + 3 <= index_count && new_point_count < max_new_normal_points; i += WORK_GROUP_SIZE * 3)
		{
			// Get vertices
			vec4 v0 = terrain_buffer->data[node_index].positions[terrain_buffer->data[node_index].indices[i]];
			vec4 v1 = terrain_buffer->data[node_index].positions[terrain_buffer->data[node_index].indices[i + 1]];
			vec4 v2 = terrain_buffer->data[node_index].positions[terrain_buffer->data[node_index].indices[i + 2]];

			// Get clipspace coordinates
			vec4 c0 = vp * v0;
			vec4 c1 = vp * v1;
			vec4 c2 = vp * v2;

			// Check if any vertex is visible (shitty clipping)
			if (clip(c0) || clip(c1) || clip(c2))
			{
				// Calculate screen space area

				c0 /= c0.w;
				c1 /= c1.w;
				c2 /= c2.w;

				// a, b, c is triangle side lengths
				float a = distance(vec2(c0.x, c0.y), vec2(c1.x, c1.y));
				float b = distance(vec2(c0.x, c0.y), vec2(c2.x, c2.y));
				float c = distance(vec2(c1.x, c1.y), vec2(c2.x, c2.y));

				// s is semiperimeter
				float s = (a + b + c) * 0.5f;

				float area = pow(s * (s - a) * (s - b) * (s - c), area_multiplier);

				vec3 mid = (vec3(v0) + vec3(v1)+ vec3(v2)) / 3.0f;
				float curv = pow(curvature(mid, camera_position), curvature_multiplier);

				// A new point should be added
				if (curv * area >= threshold)
				{
					const vec4 point = vec4(mid.x, -terrain(vec2(mid.x, mid.z)) - 0.5f, mid.z, 1.0f);
					new_points[new_point_count] = point;
					triangle_indices[new_point_count] = i / 3;
					++new_point_count;
				}
			}
		}




		////// PREFIX SUM

		const uint n = WORK_GROUP_SIZE;

		// Load into shared memory
		s_counts[thid] = new_point_count;

		//barrier();
		//memoryBarrierShared();

		if (thid == 0)
			s_total = s_counts[n - 1];

		//barrier();
		//memoryBarrierShared();

		int offset = 1;
		for (uint d = n >> 1; d > 0; d >>= 1) // Build sum in place up the tree
		{
			//barrier();
			//memoryBarrierShared();
			if (thid < d)
			{
				uint ai = offset * (2 * thid + 1) - 1;
				uint bi = offset * (2 * thid + 2) - 1;
				s_counts[bi] += s_counts[ai];
			}
			offset *= 2;
		}
		if (thid == 0) { s_counts[n - 1] = 0; } // Clear the last element
		for (int d = 1; d < n; d *= 2) // Traverse down tree & build scan
		{
			offset >>= 1;
			//barrier();
			//memoryBarrierShared();
			if (static_cast<int>(thid) < d)
			{
				uint ai = offset * (2 * thid + 1) - 1;
				uint bi = offset * (2 * thid + 2) - 1;

				uint t = s_counts[ai];
				s_counts[ai] = s_counts[bi];
				s_counts[bi] += t;
			}
		}
		//barrier();
		//memoryBarrierShared();

		uint prev_count = terrain_buffer->data[node_index].new_points_count;


		//barrier();
		//memoryBarrierShared();
		//memoryBarrierBuffer();

		// Make sure the total is saved as well
		if (thid == 0)
		{
			s_total += s_counts[n - 1];
			terrain_buffer->data[node_index].new_points_count += s_total;
			terrain_buffer->data[node_index].new_points_count = std::min(terrain_buffer->data[node_index].new_points_count, num_new_points);
		}

		// Write points to output storage buffer
		const uint base_offset = prev_count + s_counts[thid];
		for (uint i = 0; i < new_point_count && base_offset + i < num_new_points; ++i)
		{
			terrain_buffer->data[node_index].new_points[base_offset + i] = new_points[i];
			terrain_buffer->data[node_index].new_points_triangles[base_offset + i] = triangle_indices[i];
		}
	}
#pragma endregion

#pragma region TRIANGULATE_BORDERS

	struct BorderEdge
	{
		vec4 p1;
		vec4 p2;
		uint p1_index;
		uint p2_index;
		uint node_index;
		uint connection;
		uint future_index;
		uint pad[3];
	};
	const uint max_border_edges = 100;
	/*shared*/ std::array<BorderEdge, max_border_edges * 3> s_border_edges;



	const uint max_triangles_to_remove = 50;

	/*shared*/ std::array<uint, max_triangles_to_remove> s_owning_node;
	/*shared*/ std::array<uint, max_triangles_to_remove> s_triangles_to_remove;
	/*shared*/ uint s_triangles_removed;
	/*shared*/ uint s_new_triangle_count;

	/*shared*/ uint s_index_count;
	/*shared*/ uint s_triangle_count;
	/*shared*/ uint s_vertex_count;

	/*shared*/ uint s_valid_indices[max_triangles_to_remove];

#define INVALID 9999999
#define EPSILON 1.0f - 0.0001f

	void remove_old_triangles()
	{
		// Remove old triangles
		for (int j = int(s_triangles_removed) - 1; j >= 0; --j)
		{
			const uint index = s_triangles_to_remove[j];
			const uint node_index = s_owning_node[j];

			const uint last_triangle = terrain_buffer->data[node_index].index_count / 3 - 1;

			// Remove triangle
			if (index < last_triangle)
			{
				terrain_buffer->data[node_index].indices[index * 3 + 0] = terrain_buffer->data[node_index].indices[last_triangle * 3 + 0];
				terrain_buffer->data[node_index].indices[index * 3 + 1] = terrain_buffer->data[node_index].indices[last_triangle * 3 + 1];
				terrain_buffer->data[node_index].indices[index * 3 + 2] = terrain_buffer->data[node_index].indices[last_triangle * 3 + 2];
				terrain_buffer->data[node_index].triangles[index].circumcentre = terrain_buffer->data[node_index].triangles[last_triangle].circumcentre;
				terrain_buffer->data[node_index].triangles[index].circumradius = terrain_buffer->data[node_index].triangles[last_triangle].circumradius;
				terrain_buffer->data[node_index].triangles[index].circumradius2 = terrain_buffer->data[node_index].triangles[last_triangle].circumradius2;
				terrain_buffer->data[node_index].triangle_connections[index * 3 + 0] = terrain_buffer->data[node_index].triangle_connections[last_triangle * 3 + 0];
				terrain_buffer->data[node_index].triangle_connections[index * 3 + 1] = terrain_buffer->data[node_index].triangle_connections[last_triangle * 3 + 1];
				terrain_buffer->data[node_index].triangle_connections[index * 3 + 2] = terrain_buffer->data[node_index].triangle_connections[last_triangle * 3 + 2];
			}
			terrain_buffer->data[node_index].index_count -= 3;
		}
	}




	void triangulate_shader(uint node_index)
	{
		const uint thid = gl_GlobalInvocationID.x;

		const vec2 node_min = terrain_buffer->data[node_index].min;
		const vec2 node_max = terrain_buffer->data[node_index].max;
		const float side = node_max.x - node_min.x;

		const int nodes_per_side = 1 << quadtree_levels;

		const int neighbur_indexing_x[4] = { 0, 1, 0, -1 };
		const int neighbur_indexing_y[4] = { 1, 0, -1, 0 };

		const int cx = int((node_min.x - terrain_buffer->quadtree_min.x + 1) / side);  // current node x
		const int cy = int((node_min.y - terrain_buffer->quadtree_min.y + 1) / side);  // current node z/y

		// Set shared variables
		if (thid == 0)
		{
			s_triangles_removed = 0;
		}

		//barrier();
		//memoryBarrierShared();

		const uint new_points_count = terrain_buffer->data[node_index].new_points_count;
		for (uint n = 0; n < new_points_count && n < (uint)max_points_per_refine; ++n)
		{
			const vec4 current_point = terrain_buffer->data[node_index].new_points[n];

			uint seen_triangles[50];
			uint seen_triangle_count = 4;
			uint triangles_to_test[50];
			uint test_count = 3;

			const uint start_index = terrain_buffer->data[node_index].new_points_triangles[n];
			seen_triangles[0] = start_index;
			seen_triangles[1] = terrain_buffer->data[node_index].triangle_connections[start_index * 3 + 0];
			seen_triangles[2] = terrain_buffer->data[node_index].triangle_connections[start_index * 3 + 1];
			seen_triangles[3] = terrain_buffer->data[node_index].triangle_connections[start_index * 3 + 2];
			triangles_to_test[0] = seen_triangles[1];
			triangles_to_test[1] = seen_triangles[2];
			triangles_to_test[2] = seen_triangles[3];

			while (test_count != 0)
			{
				const uint triangle_index = triangles_to_test[--test_count];
				const vec2 circumcentre = terrain_buffer->data[node_index].triangles[triangle_index].circumcentre;
				const float circumradius2 = terrain_buffer->data[node_index].triangles[triangle_index].circumradius2;

				const float dx = current_point.x - circumcentre.x;
				const float dy = current_point.z - circumcentre.y;

				if (dx * dx + dy * dy < circumradius2)
				{
					// Add triangle edges to edge buffer
					const uint index0 = terrain_buffer->data[node_index].indices[triangle_index * 3 + 0];
					const uint index1 = terrain_buffer->data[node_index].indices[triangle_index * 3 + 1];
					const uint index2 = terrain_buffer->data[node_index].indices[triangle_index * 3 + 2];
					const vec4 p0 = terrain_buffer->data[node_index].positions[index0];
					const vec4 p1 = terrain_buffer->data[node_index].positions[index1];
					const vec4 p2 = terrain_buffer->data[node_index].positions[index2];

					// Store edges to be removed
					//uint tr = atomicAdd(s_triangles_removed, 1);
					uint tr = s_triangles_removed;
					++s_triangles_removed;

					if (tr >= max_border_edges || tr >= max_triangles_to_remove)
						break;

					uint ec = tr * 3;
					// Edge 0
					bool biggest_point = p0.y < p1.y;
					s_border_edges[ec + 0].p1 = biggest_point ? p0 : p1;
					s_border_edges[ec + 0].p2 = !biggest_point ? p0 : p1;
					s_border_edges[ec + 0].p1_index = biggest_point ? index0 : index1;
					s_border_edges[ec + 0].p2_index = !biggest_point ? index0 : index1;
					s_border_edges[ec + 0].node_index = node_index;
					s_border_edges[ec + 0].connection = terrain_buffer->data[node_index].triangle_connections[triangle_index * 3 + 0];
					// Edge 1
					biggest_point = p1.y < p2.y;
					s_border_edges[ec + 1].p1 = biggest_point ? p1 : p2;
					s_border_edges[ec + 1].p2 = !biggest_point ? p1 : p2;
					s_border_edges[ec + 1].p1_index = biggest_point ? index1 : index2;
					s_border_edges[ec + 1].p2_index = !biggest_point ? index1 : index2;
					s_border_edges[ec + 1].node_index = node_index;
					s_border_edges[ec + 1].connection = terrain_buffer->data[node_index].triangle_connections[triangle_index * 3 + 1];
					// Edge 2
					biggest_point = p2.y < p0.y;
					s_border_edges[ec + 2].p1 = biggest_point ? p2 : p0;
					s_border_edges[ec + 2].p2 = !biggest_point ? p2 : p0;
					s_border_edges[ec + 2].p1_index = biggest_point ? index2 : index0;
					s_border_edges[ec + 2].p2_index = !biggest_point ? index2 : index0;
					s_border_edges[ec + 2].node_index = node_index;
					s_border_edges[ec + 2].connection = terrain_buffer->data[node_index].triangle_connections[triangle_index * 3 + 2];

					// Mark the triangle to be removed later
					s_triangles_to_remove[tr] = triangle_index;
					s_owning_node[tr] = node_index;

					// Add neighbour triangles to be tested
					for (uint tt = 0; tt < 3; ++tt)
					{
						const uint index = terrain_buffer->data[node_index].triangle_connections[triangle_index * 3 + tt];
						if (index == INVALID)
							continue;
						// Check if it has already been seen
						bool found = false;
						for (uint ii = 0; ii < seen_triangle_count; ++ii)
						{
							if (index == seen_triangles[ii])
							{
								found = true;
								break;
							}
						}
						if (!found)
						{
							seen_triangles[seen_triangle_count++] = index;
							triangles_to_test[test_count++] = index;
						}
					}
					// TODO: Borders
				}
			}



			//barrier();
			//memoryBarrierShared();

			// Delete all doubly specified edges from edge buffer (this leaves the edges of the enclosing polygon only)
			const uint edge_count = s_triangles_removed * 3;
			uint i = thid;
			while (i < edge_count)
			{
				bool found = false;
				for (uint j = 0; j < edge_count; ++j)
				{
					if (i != j &&
						s_border_edges[i].p1 == s_border_edges[j].p1 &&
						s_border_edges[i].p2 == s_border_edges[j].p2)
					{
						// Mark as invalid
						s_border_edges[j].p1.w = -1;
						found = true;
					}
				}
				if (found)
					s_border_edges[i].p1.w = -1;
				i += WORK_GROUP_SIZE;
			}

			//barrier();
			//memoryBarrierShared();

			// Count the number of new triangles to create
			if (thid == 0)
			{
				s_new_triangle_count = 0;

				for (uint j = 0; j < edge_count && j < max_triangles_to_remove * 3; ++j)
				{
					if (s_border_edges[j].p1.w > -0.5)
					{
						s_valid_indices[s_new_triangle_count++] = j;
						s_border_edges[j].future_index = terrain_buffer->data[s_border_edges[j].node_index].temp_new_triangles_count++;  // TODO: Reset
					}
				}
			}

			//barrier();
			//memoryBarrierShared();

			// If new triangles will not fit in index buffer, quit
			//if (s_index_count + (s_new_triangle_count * 3) >= num_indices)
			//{
			//	//finish = true;
			//	break;
			//}

			if (thid == 0)
			{
				std::array<uint, 9> participating_nodes;
				uint participation_count = 0;

				// Add to the triangle list all triangles formed between the point and the edges of the enclosing polygon
				for (uint ii = 0; ii < s_new_triangle_count; ++ii)
				{
					uint i = s_valid_indices[ii];
					vec3 P = vec3(s_border_edges[i].p1);
					vec3 Q = vec3(s_border_edges[i].p2);
					vec3 R = vec3(current_point);

					vec2 PQ = normalize(vec2(Q.x, Q.z) - vec2(P.x, P.z));
					vec2 PR = normalize(vec2(R.x, R.z) - vec2(P.x, P.z));
					vec2 RQ = normalize(vec2(Q.x, Q.z) - vec2(R.x, R.z));

					float d1 = abs(dot(PQ, PR));
					float d2 = abs(dot(PR, RQ));
					float d3 = abs(dot(RQ, PQ));

					// Skip this triangle because it is too narrow (should only happen at borders)
					if (d1 > EPSILON || d2 > EPSILON || d3 > EPSILON)
					{
						continue;  // TODO: Still needed?
					}

					// Make sure winding order is correct
					const vec3 nor = cross(R - P, Q - P);
					if (nor.y > 0)
					{
						vec4 temp = s_border_edges[i].p1;
						s_border_edges[i].p1 = s_border_edges[i].p2;
						s_border_edges[i].p2 = temp;
						uint temp2 = s_border_edges[i].p1_index;
						s_border_edges[i].p1_index = s_border_edges[i].p2_index;
						s_border_edges[i].p2_index = temp2;
					}

					// Set indices for the new triangle
					const uint index_count = terrain_buffer->data[s_border_edges[i].node_index].index_count;
					terrain_buffer->data[s_border_edges[i].node_index].indices[index_count + 0] = s_border_edges[i].p1_index;
					terrain_buffer->data[s_border_edges[i].node_index].indices[index_count + 1] = s_border_edges[i].p2_index;
					terrain_buffer->data[s_border_edges[i].node_index].indices[index_count + 2] = terrain_buffer->data[s_border_edges[i].node_index].vertex_count;

					// Set circumcircles for the new triangle
					float a = distance(vec2(P.x, P.z), vec2(Q.x, Q.z));
					float b = distance(vec2(P.x, P.z), vec2(R.x, R.z));
					float c = distance(vec2(R.x, R.z), vec2(Q.x, Q.z));

					const vec2 cc_center = find_circum_center(vec2(P.x, P.z), vec2(Q.x, Q.z), vec2(R.x, R.z));
					const float cc_radius2 = find_circum_radius_squared(a, b, c);
					const float cc_radius = sqrt(cc_radius2);

					const uint triangle_count = index_count / 3;
					terrain_buffer->data[s_border_edges[i].node_index].triangles[triangle_count].circumcentre = cc_center;
					terrain_buffer->data[s_border_edges[i].node_index].triangles[triangle_count].circumradius = cc_radius;
					terrain_buffer->data[s_border_edges[i].node_index].triangles[triangle_count].circumradius2 = cc_radius2;

					// Connections
					terrain_buffer->data[s_border_edges[i].node_index].triangle_connections[index_count + 0] = s_border_edges[i].connection;
					const vec4 edges[4] = { s_border_edges[i].p2, current_point, current_point, s_border_edges[i].p1 };
					for (uint ss = 0; ss < 4; ss += 2)  // The two other sides
					{
						// TODO: Add early exit?
						//if (terrain_buffer->data[s_border_edges[i].node_index].triangle_connections[index_count + 1 + ss / 2] == )
						//	continue;

						bool found = false;
						// Search through all other new triangles that have been added to find possible neighbours/connections
						for (uint ee = 0; ee < s_new_triangle_count && !found; ++ee)
						{
							if (ee == i || s_border_edges[i].node_index != s_border_edges[ee].node_index)
								continue;
							// Check each pair of points in the triangle if they match
							if (edges[ss] == s_border_edges[ee].p2 && edges[ss + 1] == s_border_edges[ee].p1)
							{
								terrain_buffer->data[s_border_edges[i].node_index].triangle_connections[index_count + 1 + ss / 2] = s_border_edges[ee].future_index;
								found = true;
							}
							else if (edges[ss] == current_point && edges[ss + 1] == s_border_edges[ee].p2)
							{
								terrain_buffer->data[s_border_edges[i].node_index].triangle_connections[index_count + 1 + ss / 2] = s_border_edges[ee].future_index;
								found = true;
							}
							else if (edges[ss] == s_border_edges[ee].p1 && edges[ss + 1] == current_point)
							{
								terrain_buffer->data[s_border_edges[i].node_index].triangle_connections[index_count + 1 + ss / 2] = s_border_edges[ee].future_index;
								found = true;
							}
						}
						if (!found)
						{
							terrain_buffer->data[s_border_edges[i].node_index].triangle_connections[index_count + 1 + ss / 2] = INVALID;
						}
					}

					terrain_buffer->data[s_border_edges[i].node_index].index_count += 3;

					bool found = false;
					for (uint jj = 0; jj < participation_count; ++jj)
					{
						if (s_border_edges[i].node_index == participating_nodes[jj])
						{
							found = true;
							break;
						}
					}
					if (!found)
						participating_nodes[participation_count++] = s_border_edges[i].node_index;
				}

				remove_old_triangles();

				// Insert new point
				for (uint jj = 0; jj < participation_count; ++jj)
				{
					terrain_buffer->data[participating_nodes[jj]].positions[terrain_buffer->data[participating_nodes[jj]].vertex_count++] = current_point;
				}

				s_triangles_removed = 0;
			}

			//barrier();
			//memoryBarrierShared();
			//memoryBarrierBuffer();
		}

		//if (thid == 0)
		//{
			//terrain_buffer->data[node_index].vertex_count = s_vertex_count;
			//terrain_buffer->data[node_index].index_count = s_index_count;
		terrain_buffer->data[node_index].new_points_count = 0;
		//}
	}

#pragma endregion
}
