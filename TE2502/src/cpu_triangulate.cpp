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
	#define TERRAIN_GENERATE_NUM_INDICES 200
	#define TERRAIN_GENERATE_NUM_VERTICES 100
	#define TERRAIN_GENERATE_NUM_NODES 4
	#define TERRAIN_GENERATE_GRID_SIDE 3
	#define TRIANGULATE_MAX_NEW_NORMAL_POINTS 1024
	#define TRIANGULATE_MAX_NEW_BORDER_POINTS 50
	#define QUADTREE_LEVELS 1
	#define MAX_BORDER_TRIANGLE_COUNT 50

	#define WORK_GROUP_SIZE 1

	const glm::uvec3 gl_GlobalInvocationID{ 0, 0, 0 };

	uint32_t num_indices;
	uint32_t num_vertices;
	uint32_t num_nodes;
	uint32_t num_new_points;
	uint32_t quadtree_levels;
	uint32_t max_new_border_points;
	uint32_t max_border_triangle_count;

	struct Triangle
	{
		glm::vec2 circumcentre;
		float circumradius;
		float circumradius2;
	};

	struct BufferNodeHeader
	{
		uint32_t vertex_count;
		uint32_t new_points_count;
		uint32_t pad;

		glm::vec2 min;
		glm::vec2 max;

		std::array<uint32_t, 4> new_border_point_count;
		std::array<glm::vec4, 4 * TRIANGULATE_MAX_NEW_BORDER_POINTS> new_border_points;
		std::array<uint32_t, 4> border_count;
		std::array<uint32_t, 4 * MAX_BORDER_TRIANGLE_COUNT> border_triangle_indices;
		std::array<float, 4> border_max;
		std::array<float, 4 * MAX_BORDER_TRIANGLE_COUNT > border_diffs;
	};

	struct TerrainData
	{
		uint32_t index_count;
		uint32_t instance_count;
		uint32_t first_index;
		int  vertex_offset;
		uint32_t first_instance;

		// struct BufferNodeHeader {
		uint32_t vertex_count;
		uint32_t new_points_count;
		uint32_t pad;

		glm::vec2 min;
		glm::vec2 max;

		std::array<uint32_t, 4> new_border_point_count;
		std::array<glm::vec4, 4 * TRIANGULATE_MAX_NEW_BORDER_POINTS> new_border_points;
		std::array<uint32_t, 4> border_count;
		std::array<uint32_t, 4 * MAX_BORDER_TRIANGLE_COUNT> border_triangle_indices;
		std::array<float, 4> border_max;
		std::array<float, 4 * MAX_BORDER_TRIANGLE_COUNT > border_diffs;
		// }

		std::array<uint32_t, TERRAIN_GENERATE_NUM_INDICES> indices;
		std::array<glm::vec4, TERRAIN_GENERATE_NUM_VERTICES> positions;
		std::array<Triangle, TERRAIN_GENERATE_NUM_INDICES / 3> triangles;
		std::array<glm::vec4, TRIANGULATE_MAX_NEW_NORMAL_POINTS> new_points;
	};

	const uint32_t quadtree_data_size = (1 << QUADTREE_LEVELS) * (1 << QUADTREE_LEVELS) + 4;
	const uint32_t pad_size = 16 - (quadtree_data_size % 16);

	struct TerrainBuffer
	{
		std::array<uint32_t, (1 << QUADTREE_LEVELS) * (1 << QUADTREE_LEVELS)> quadtree_index_map;
		glm::vec2 quadtree_min;
		glm::vec2 quadtree_max;
		std::array<uint32_t, pad_size> pad;
		std::array<TerrainData, TERRAIN_GENERATE_NUM_NODES> data;
	};


	struct GenerateInfo
	{
		glm::vec2 min;
		glm::vec2 max;
		uint32_t index;
	};
	struct Quadtree {
		// Number and array of indices to nodes that needs to generate terrain
		uint32_t num_generate_nodes;
		GenerateInfo* generate_nodes;

		// Number and array of indices to nodes that needs to draw terrain
		uint32_t num_draw_nodes;
		uint32_t* draw_nodes;

		float total_side_length;
		uint32_t levels;

		// Max number of active nodes
		uint64_t max_nodes;

		// For chunk i of m_buffer, m_buffer_index_filled[i] is true if that chunk is used by a node
		bool* buffer_index_filled;

		uint32_t* node_index_to_buffer_index;

		uint64_t node_memory_size;
		glm::vec2* quadtree_minmax;
	};
	Quadtree quadtree;

	const uint32_t INVALID = ~0u;

	TerrainBuffer* terrain_buffer;
	uint32_t cpu_index_buffer_size;


#pragma region TERRAINSTUFF
	float dump;
	glm::vec2 addxy = glm::vec2(1.0f, 0.0f);
	glm::vec2 addyy = glm::vec2(0.0f, 0.0f);
	glm::vec2 addxx = glm::vec2(1.0f, 1.0f);
	glm::vec2 addyx = glm::vec2(0.0f, 1.0f);
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

	const glm::mat2 rotate2D = glm::mat2(1.3623f, 1.7531f, -1.7131f, 1.4623f);

	//// Low-res stuff

	float hash12(glm::vec2 p)
	{
		glm::vec3 p3 = fract(glm::vec3(p.x, p.y, p.x) * HASHSCALE1);
		p3 += glm::dot(p3, glm::vec3(p3.y, p3.z, p3.x) + 19.19f);
		return modff((p3.x + p3.y) * p3.z, &dump);
	}

	float noise(glm::vec2 x)
	{
		glm::vec2 p = floor(x);
		glm::vec2 f = fract(x);
		f = f * f * (3.0f - 2.0f * f);

		float res = mix(mix(hash12(p), hash12(p + addxy), f.x),
			mix(hash12(p + addyx), hash12(p + addxx), f.x), f.y);
		return res;
	}

	float terrain(glm::vec2 p)
	{
		glm::vec2 pos = p * 0.05f;
		float w = (noise(pos * 0.25f) * 0.75f + 0.15f);
		w = 66.0f * w * w;
		glm::vec2 dxy = glm::vec2(0.0, 0.0);
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

	float height_to_surface(glm::vec3 p)
	{
		float h = terrain({ p.x, p.z });

		return -p.y - h;
	}

	///////

	//// High-res stuff
#define HASHSCALE3 glm::vec3(.1031f, .1030f, .0973f)
#define HASHSCALE4 glm::vec4(1031f, .1030f, .0973f, .1099f)

	glm::vec2 hash22(glm::vec2 p)
	{
		glm::vec3 p3 = fract(glm::vec3(p.x, p.y, p.x) * HASHSCALE3);
		p3 += glm::dot(p3, glm::vec3(p3.y, p3.z, p3.x) + 19.19f);
		return fract((glm::vec2(p3.x, p3.x) + glm::vec2(p3.y, p3.z)) * glm::vec2(p3.z, p3.y));

	}

	glm::vec2 noise2(glm::vec2 x)
	{
		glm::vec2 p = floor(x);
		glm::vec2 f = fract(x);
		f = f * f * (3.0f - 2.0f * f);
		float n = p.x + p.y * 57.0f;
		glm::vec2 res = mix(mix(hash22(p), hash22(p + addxy), f.x),
			mix(hash22(p + addyx), hash22(p + addxx), f.x), f.y);
		return res;
	}

	//--------------------------------------------------------------------------
	// High def version only used for grabbing normal information.
	float terrain2(glm::vec2 p)
	{
		// There's some real magic numbers in here! 
		// The noise calls add large mountain ranges for more variation over distances...
		glm::vec2 pos = p * 0.05f;
		float w = (noise(pos * 0.25f) * 0.75f + 0.15f);
		w = 66.0f * w * w;
		glm::vec2 dxy = glm::vec2(0.0f, 0.0f);
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

	glm::vec3 sun_light = normalize(glm::vec3(0.4, 0.4, 0.48));
	glm::vec3 sun_color = glm::vec3(1.0, .9, .83);
	float specular = 0.0;
	float ambient;

	// Calculate sun light...
	void do_lighting(glm::vec3& mat, glm::vec3 pos, glm::vec3 normal, glm::vec3 eye_dir, float dis)
	{
		float h = dot(sun_light, normal);
		float c = std::max(h, 0.0f) + ambient;
		mat = mat * sun_color * c;
		// Specular...
		if (h > 0.0)
		{
			glm::vec3 R = reflect(sun_light, normal);
			float spec_amount = pow(std::max(dot(R, normalize(eye_dir)), 0.0f), 3.0f) * specular;
			mat = mix(mat, sun_color, spec_amount);
		}
	}

	// Hack the height, position, and normal data to create the coloured landscape
	glm::vec3 surface_color(glm::vec3 pos, glm::vec3 normal, float dis, glm::vec3 cam_pos)
	{
		glm::vec3 mat;
		specular = 0.0f;
		ambient = 0.1f;
		glm::vec3 dir = normalize(pos - cam_pos);

		glm::vec3 mat_pos = pos * 2.0f;// ... I had change scale halfway though, this lazy multiply allow me to keep the graphic scales I had

		float dis_sqrd = dis * dis;// Squaring it gives better distance scales.

		float f = clamp(noise(glm::vec2(mat_pos.x, mat_pos.z) * 0.05f), 0.0f, 1.0f);
		f += noise(glm::vec2(mat_pos.x, mat_pos.z) * 0.1f + glm::vec2(normal.y, normal.z) * 1.08f) * 0.85f;
		f *= 0.55f;
		glm::vec3 m = mix(glm::vec3(.63*f + .2, .7*f + .1, .7*f + .1), glm::vec3(f*.43 + .1, f*.3 + .2, f*.35 + .1), f * 0.65f);
		mat = m * glm::vec3(f * m.x + .36f, f * m.y + .30f, f * m.z + .28f);
		// Should have used smoothstep to add colours, but left it using 'if' for sanity...
		if (normal.y < .5f)
		{
			float v = normal.y;
			float c = (0.5f - normal.y) * 4.0f;
			c = clamp(c*c, 0.1f, 1.0f);
			f = noise(glm::vec2(mat_pos.x * 0.09f, mat_pos.z * 0.095f) + glm::vec2(mat_pos.y, mat_pos.y) * 0.15f);
			f += noise(glm::vec2(mat_pos.x * 2.233f, mat_pos.z * 2.23f)) * 0.5f;
			mat = mix(mat, glm::vec3(.4f * f), c);
			specular += 0.1f;
		}

		// Grass. Use the normal to decide when to plonk grass down...
		if (normal.y > .65f)
		{

			m = glm::vec3(noise(glm::vec2(mat_pos.x, mat_pos.z) * 0.023f) * 0.5f + 0.15f, noise(glm::vec2(mat_pos.x, mat_pos.z) * 0.03f) * 0.6f + 0.25f, 0.0f);
			m *= (normal.y - 0.65f) * 0.6f;
			mat = mix(mat, m, clamp((normal.y - .65f) * 1.3f * (45.35f - mat_pos.y)* 0.1f, 0.0f, 1.0f));
		}

		// Snow topped mountains...
		if (mat_pos.y > 80.0f && normal.y > .42f)
		{
			float snow = clamp((mat_pos.y - 80.0f - noise(glm::vec2(mat_pos.x, mat_pos.z) * 0.1f) * 28.0f) * 0.035f, 0.0f, 1.0f);
			mat = mix(mat, glm::vec3(.7f, .7f, .8f), snow);
			specular += snow;
			ambient += snow * 0.3f;
		}

		do_lighting(mat, pos, normal, dir, dis_sqrd);

		return mat;
	}

	// Some would say, most of the magic is done in post! :D
	glm::vec3 post_effects(glm::vec3 rgb)
	{
		return (1.0f - exp(-rgb * 6.0f)) * 1.0024f;
	}
#pragma endregion
	   

#pragma region CC

	// Function to find the line given two points
	void line_from_points(glm::vec2 p1, glm::vec2 p2, float& a, float& b, float& c)
	{
		a = p2.y - p1.y;
		b = p1.x - p2.x;
		c = a * p1.x + b * p2.y;
	}

	// Function which converts the input line to its 
	// perpendicular bisector. It also inputs the points 
	// whose mid-point lies on the bisector 
	void perpendicular_bisector_from_line(glm::vec2 p1, glm::vec2 p2, float& a, float& b, float& c)
	{
		glm::vec2 mid_point = glm::vec2((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);

		// c = -bx + ay 
		c = -b * mid_point.x + a * mid_point.y;

		float temp = a;
		a = -b;
		b = temp;
	}

	// Returns the intersection point of two lines 
	glm::vec2 line_line_intersection(float a1, float b1, float c1, float a2, float b2, float c2)
	{
		float determinant = a1 * b2 - a2 * b1;

		float x = (b2 * c1 - b1 * c2) / determinant;
		float y = (a1 * c2 - a2 * c1) / determinant;

		return glm::vec2(x, y);
	}

	glm::vec2 find_circum_center(glm::vec2 P, glm::vec2 Q, glm::vec2 R)
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

	float find_circum_radius_squared(glm::vec2 P, glm::vec2 Q, glm::vec2 R)
	{
		float a = distance(P, Q);
		float b = distance(Q, R);
		float c = distance(R, P);
		return (a * a * b * b * c * c) / ((a + b + c) * (b + c - a) * (c + a - b) * (a + b - c));
	}


#pragma endregion

	bool clip(glm::vec4 p)
	{
		return (abs(p.x) <= p.w &&
			abs(p.y) <= p.w &&
			abs(p.z) <= p.w);
	}

	float curvature(glm::vec3 p, glm::vec3 camera_position)
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
		for (uint32_t i = 0; i < filter_side * filter_side; i++)
		{
			log_filter[i] *= correction;
		}

		float curvature = 0.0f;

		for (int x = -filter_radius; x <= filter_radius; x++)
		{
			for (int y = -filter_radius; y <= filter_radius; y++)
			{
				curvature += terrain(glm::vec2(p.x, p.z) + glm::vec2(sample_step * x, sample_step * y)) * log_filter[(y + filter_radius) * filter_side + (x + filter_radius)];
			}
		}

		// Normalize for height
		curvature -= terrain(glm::vec2(p.x, p.z));

		return abs(curvature);
	}


	void setup(TFile& tfile)
	{
		num_indices = TERRAIN_GENERATE_NUM_INDICES;
		num_vertices = TERRAIN_GENERATE_NUM_VERTICES;
		num_nodes = TERRAIN_GENERATE_NUM_NODES;
		num_new_points = TRIANGULATE_MAX_NEW_NORMAL_POINTS;
		quadtree_levels = QUADTREE_LEVELS;
		max_new_border_points = TRIANGULATE_MAX_NEW_BORDER_POINTS;
		max_border_triangle_count = MAX_BORDER_TRIANGLE_COUNT;

		assert(quadtree_levels > 0);

		quadtree.buffer_index_filled = new bool[num_nodes];
		memset(quadtree.buffer_index_filled, 0, num_nodes * sizeof(bool));

		quadtree.num_generate_nodes = 0;
		quadtree.generate_nodes = new GenerateInfo[num_nodes];

		quadtree.num_draw_nodes = 0;
		quadtree.draw_nodes = new uint32_t[num_nodes];

		quadtree.node_memory_size =
			sizeof(VkDrawIndexedIndirectCommand) +
			sizeof(BufferNodeHeader) +
			num_indices * sizeof(uint32_t) + // Indices
			num_vertices * sizeof(glm::vec4) + // Vertices
			(num_indices / 3) * sizeof(Triangle) + // Circumcentre and circumradius
			num_new_points * sizeof(glm::vec4); // New points

		// Add space for an additional two vec2's to store the quadtree min and max
		cpu_index_buffer_size = (1 << quadtree_levels) * (1 << quadtree_levels) * sizeof(uint32_t) + sizeof(glm::vec2) * 2;
		cpu_index_buffer_size += 64 - (cpu_index_buffer_size % 64);

		terrain_buffer = (TerrainBuffer*) new char[cpu_index_buffer_size + quadtree.node_memory_size * num_nodes + 1000];

		quadtree.node_index_to_buffer_index = (uint32_t*)terrain_buffer;

		// Point to the end of cpu index buffer
		quadtree.quadtree_minmax = (glm::vec2*) (((char*)quadtree.node_index_to_buffer_index) + (1 << quadtree_levels) * (1 << quadtree_levels) * sizeof(uint32_t));

		quadtree.total_side_length = TERRAIN_GENERATE_TOTAL_SIDE_LENGTH;
		float half_length = quadtree.total_side_length * 0.5f;
		quadtree.quadtree_minmax[0] = glm::vec2(-half_length, -half_length);
		quadtree.quadtree_minmax[1] = glm::vec2(half_length, half_length);

		// (1 << levels) is number of nodes per axis
		memset(quadtree.node_index_to_buffer_index, INVALID, (1 << quadtree_levels) * (1 << quadtree_levels) * sizeof(uint32_t));
	}

	void destroy()
	{
		delete[] terrain_buffer;
		delete[] quadtree.draw_nodes;
		delete[] quadtree.generate_nodes;
		delete[] quadtree.buffer_index_filled;
	}

	void run(DebugDrawer& dd, Camera& camera, Window& window)
	{
		Frustum fr = camera.get_frustum();
		intersect(fr, dd);

		draw_terrain(fr, dd, camera);

		static float threshold = 0.01f;
		static float area_mult = 1.0f;
		static float curv_mult = 1.0f;

		ImGui::Begin("cputri");
		if (ImGui::Button("Refine"))
		{
			process_triangles(camera, window, threshold, area_mult, curv_mult);
			triangulate();
		}
		ImGui::End();

	}

	uint32_t find_chunk()
	{
		for (uint32_t i = 0; i < num_nodes; i++)
		{
			if (!quadtree.buffer_index_filled[i])
				return i;
		}

		return INVALID;
	}

	uint32_t get_offset(uint32_t node_x, uint32_t node_z)
	{
		assert(node_x >= 0u && node_x < (1u << quadtree_levels));
		assert(node_z >= 0u && node_z < (1u << quadtree_levels));

		return quadtree.node_index_to_buffer_index[node_x + (1u << quadtree_levels) * node_z];
	}

	uint64_t get_index_offset_of_node(uint32_t i)
	{
		return get_offset_of_node(i) + sizeof(VkDrawIndexedIndirectCommand) + sizeof(BufferNodeHeader);
	}

	uint64_t get_vertex_offset_of_node(uint32_t i)
	{
		return get_index_offset_of_node(i) + num_indices * sizeof(uint32_t);

	}

	uint64_t get_offset_of_node(uint32_t i)
	{
		return cpu_index_buffer_size + i * quadtree.node_memory_size;
	}

	void clear_terrain()
	{
		memset(quadtree.node_index_to_buffer_index, INVALID, (1 << quadtree_levels) * (1 << quadtree_levels) * sizeof(uint32_t));
		memset(quadtree.buffer_index_filled, 0, num_nodes * sizeof(bool));
	}

	void triangulate()
	{
		for (unsigned i = 0; i < quadtree.num_draw_nodes; ++i)
		{
			triangulate_shader(quadtree.draw_nodes[i]);
		}

		for (unsigned i = 0; i < quadtree.num_generate_nodes; ++i)
		{
			triangulate_shader(quadtree.generate_nodes[i].index);
		}
	}

	void process_triangles(Camera& camera, Window& window, float em_threshold, float area_multiplier, float curvature_multiplier)
	{
		// Nonupdated terrain
		for (uint32_t i = 0; i < quadtree.num_draw_nodes; i++)
		{
			triangle_process_shader(
				camera.get_vp(),
				glm::vec4(camera.get_pos(), 0),
				window.get_size(),
				em_threshold,
				area_multiplier,
				curvature_multiplier,
				quadtree.draw_nodes[i]);
		}

		// Newly generated terrain
		for (uint32_t i = 0; i < quadtree.num_generate_nodes; i++)
		{
			triangle_process_shader(
				camera.get_vp(),
				glm::vec4(camera.get_pos(), 0),
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

		for (uint32_t i = 0; i < quadtree.num_generate_nodes; i++)
		{
			generate_shader(quadtree.generate_nodes[i].index, quadtree.generate_nodes[i].min, quadtree.generate_nodes[i].max);
		}
	}

	void intersect(Frustum& frustum, DebugDrawer& dd, AabbXZ aabb, uint32_t level, uint32_t x, uint32_t y)
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
			uint32_t index = (1 << quadtree_levels) * y + x;
			if (quadtree.node_index_to_buffer_index[index] == INVALID)
			{
				// Visible node does not have data

				uint32_t new_index = find_chunk();
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
			glm::vec2 mid = (aabb.m_min + aabb.m_max) * 0.5f;
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
		for (size_t ii = 0; ii < num_nodes; ii++)
		{
			if (quadtree.buffer_index_filled[ii])
			{
				for (uint64_t ind = 0; ind < terrain_buffer->data[ii].index_count; ind += 3)
				{
					const float height = -100.0f;

					glm::vec3 p0 = glm::vec3(terrain_buffer->data[ii].positions[terrain_buffer->data[ii].indices[ind + 0]]) + glm::vec3(0.0f, height, 0.0f);
					glm::vec3 p1 = glm::vec3(terrain_buffer->data[ii].positions[terrain_buffer->data[ii].indices[ind + 1]]) + glm::vec3(0.0f, height, 0.0f);
					glm::vec3 p2 = glm::vec3(terrain_buffer->data[ii].positions[terrain_buffer->data[ii].indices[ind + 2]]) + glm::vec3(0.0f, height, 0.0f);

					glm::vec3 mid = (p0 + p1 + p2) / 3.0f;

					dd.draw_line(p0, p1, { 1, 0, 0 });
					dd.draw_line(p1, p2, { 1, 0, 0 });
					dd.draw_line(p2, p0, { 1, 0, 0 });

					dd.draw_line(mid, p0, { 0, 1, 0 });
					dd.draw_line(mid, p1, { 0, 1, 0 });
					dd.draw_line(mid, p2, { 0, 1, 0 });
				}
			}
		}
	}

	void generate_shader(uint32_t node_index, glm::vec2 min, glm::vec2 max)
	{
		const uint32_t GRID_SIDE = TERRAIN_GENERATE_GRID_SIDE;

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

			for (uint32_t i = 0; i < 4; ++i)
			{
				terrain_buffer->data[node_index].new_border_point_count[i] = 0;
				terrain_buffer->data[node_index].border_max[i] = 10;
				terrain_buffer->data[node_index].border_count[i] = 0;
			}
		}

		// Positions
		uint32_t i = gl_GlobalInvocationID.x;
		while (i < GRID_SIDE * GRID_SIDE)
		{
			float x = min.x + ((i % GRID_SIDE) / float(GRID_SIDE - 1)) * (max.x - min.x);
			float z = min.y + float(i / GRID_SIDE) / float(GRID_SIDE - 1) * (max.y - min.y);

			terrain_buffer->data[node_index].positions[i] = glm::vec4(x, -terrain(glm::vec2(x, z)) - 0.5f, z, 1.0f);


			i += WORK_GROUP_SIZE;
		}

		//barrier();
		//memoryBarrierBuffer();

		// Triangles
		i = gl_GlobalInvocationID.x;
		while (i < (GRID_SIDE - 1) * (GRID_SIDE - 1))
		{
			uint32_t y = i / (GRID_SIDE - 1);
			uint32_t x = i % (GRID_SIDE - 1);
			uint32_t index = y * GRID_SIDE + x;

			// Indices
			uint32_t offset = i * 6;
			terrain_buffer->data[node_index].indices[offset] = index;
			terrain_buffer->data[node_index].indices[offset + 1] = index + GRID_SIDE + 1;
			terrain_buffer->data[node_index].indices[offset + 2] = index + 1;

			terrain_buffer->data[node_index].indices[offset + 3] = index;
			terrain_buffer->data[node_index].indices[offset + 4] = index + GRID_SIDE;
			terrain_buffer->data[node_index].indices[offset + 5] = index + GRID_SIDE + 1;

			// Circumcentres
			offset = i * 2;
			glm::vec4 posP1 = terrain_buffer->data[node_index].positions[index];
			glm::vec4 posQ1 = terrain_buffer->data[node_index].positions[index + GRID_SIDE + 1];
			glm::vec4 posR1 = terrain_buffer->data[node_index].positions[index + 1];

			glm::vec4 posP2 = terrain_buffer->data[node_index].positions[index];
			glm::vec4 posQ2 = terrain_buffer->data[node_index].positions[index + GRID_SIDE];
			glm::vec4 posR2 = terrain_buffer->data[node_index].positions[index + GRID_SIDE + 1];

			glm::vec2 P1 = { posP1.x, posP1.z };
			glm::vec2 Q1 = { posQ1.x, posQ1.z };
			glm::vec2 R1 = { posR1.x, posR1.z };
			terrain_buffer->data[node_index].triangles[offset].circumcentre = find_circum_center(P1, Q1, R1);

			glm::vec2 P2 = { posP2.x, posP2.z };
			glm::vec2 Q2 = { posQ2.x, posQ2.z };
			glm::vec2 R2 = { posR2.x, posR2.z };
			terrain_buffer->data[node_index].triangles[offset + 1].circumcentre = find_circum_center(P2, Q2, R2);

			// Circumradii
			const float radius12 = find_circum_radius_squared(P1, Q1, R1);
			const float radius22 = find_circum_radius_squared(P2, Q2, R2);
			terrain_buffer->data[node_index].triangles[offset].circumradius2 = radius12;
			terrain_buffer->data[node_index].triangles[offset].circumradius = sqrt(radius12);
			terrain_buffer->data[node_index].triangles[offset + 1].circumradius2 = radius22;
			terrain_buffer->data[node_index].triangles[offset + 1].circumradius = sqrt(radius22);

			i += WORK_GROUP_SIZE;
		}
	}

#pragma region TRIANGLE_PROCESS
	const uint32_t max_new_normal_points = TRIANGULATE_MAX_NEW_NORMAL_POINTS / WORK_GROUP_SIZE;
	static /*shared*/ std::array<uint32_t, WORK_GROUP_SIZE> s_counts;
	static /*shared*/ uint32_t s_total;
		
	static /*shared*/ std::array<float, 4> s_border_max;


	void triangle_process_shader(glm::mat4 vp, glm::vec4 camera_position, glm::vec2 screen_size, float threshold, float area_multiplier, float curvature_multiplier, uint32_t node_index)
	{
		std::array<glm::vec4, max_new_normal_points + 1> new_points;

		const uint32_t thid = gl_GlobalInvocationID.x;

		const glm::vec2 node_min = terrain_buffer->data[node_index].min;
		const glm::vec2 node_max = terrain_buffer->data[node_index].max;
		const float side = node_max.x - node_min.x;

		const int neighbur_indexing_x[4] = { 0, 1, 0, -1 };
		const int neighbur_indexing_y[4] = { 1, 0, -1, 0 };

		const int cx = int((node_min.x - terrain_buffer->quadtree_min.x + 1) / side);  // current node x
		const int cy = int((node_min.y - terrain_buffer->quadtree_min.y + 1) / side);  // current node z/y

		const int nodes_per_side = 1 << quadtree_levels;

		if (thid == 0)  // TODO: Optimize to multiple threads
		{
			for (int bb = 0; bb < 4; ++bb)  // TODO: Go through corner neighbour nodes as well
			{
				int nx = cx + neighbur_indexing_x[bb];
				int ny = cy + neighbur_indexing_y[bb];

				// Check if valid neighbour
				if (ny >= 0 && ny < nodes_per_side && nx >= 0 && nx < nodes_per_side)
				{
					uint32_t neighbour_index = terrain_buffer->quadtree_index_map[ny * nodes_per_side + nx];
					int neighbour_border = (bb + 2) % 4;

					s_border_max[bb] = terrain_buffer->data[neighbour_index].border_max[neighbour_border];
				}
				else
				{
					s_border_max[bb] = 0;
				}
			}
		}

		//barrier();
		//memoryBarrierShared();

		const uint32_t index_count = terrain_buffer->data[node_index].index_count;

		uint32_t new_point_count = 0;

		// For every triangle
		for (uint32_t i = thid * 3; i + 3 <= index_count && new_point_count < max_new_normal_points; i += WORK_GROUP_SIZE * 3)
		{
			// Get vertices
			glm::vec4 v0 = terrain_buffer->data[node_index].positions[terrain_buffer->data[node_index].indices[i]];
			glm::vec4 v1 = terrain_buffer->data[node_index].positions[terrain_buffer->data[node_index].indices[i + 1]];
			glm::vec4 v2 = terrain_buffer->data[node_index].positions[terrain_buffer->data[node_index].indices[i + 2]];

			// Get clipspace coordinates
			glm::vec4 c0 = vp * v0;
			glm::vec4 c1 = vp * v1;
			glm::vec4 c2 = vp * v2;

			// Check if any vertex is visible (shitty clipping)
			if (clip(c0) || clip(c1) || clip(c2))
			{
				// Calculate screen space area

				c0 /= c0.w;
				c1 /= c1.w;
				c2 /= c2.w;

				// a, b, c is triangle side lengths
				float a = distance(glm::vec2(c0.x, c0.y), glm::vec2(c1.x, c1.y));
				float b = distance(glm::vec2(c0.x, c0.y), glm::vec2(c2.x, c2.y));
				float c = distance(glm::vec2(c1.x, c1.y), glm::vec2(c2.x, c2.y));

				// s is semiperimeter
				float s = (a + b + c) * 0.5f;

				float area = pow(s * (s - a) * (s - b) * (s - c), area_multiplier);

				glm::vec3 mid = (glm::vec3(v0) + glm::vec3(v1)+ glm::vec3(v2)) / 3.0f;
				float curv = pow(curvature(mid, camera_position), curvature_multiplier);

				// A new point should be added
				if (curv * area >= threshold)
				{
					// Check if the point could be a border point
					const glm::vec4 point = glm::vec4(mid.x, -terrain(glm::vec2(mid.x, mid.z)) - 0.5f, mid.z, 1.0f);
					// TODO: Check against corner neighbour nodes as well
					bool border = false;

					// Left
					if (point.x < node_min.x + s_border_max[3])
					{
						//uint32_t count = atomicAdd(terrain_buffer->data[node_index].new_border_point_count[3], 1);	// TODO: Don't increment if number of points exceed max border points 
						uint32_t count = terrain_buffer->data[node_index].new_border_point_count[3];
						++terrain_buffer->data[node_index].new_border_point_count[3];
						if (count < max_new_border_points)
						{
							terrain_buffer->data[node_index].new_border_points[3 * max_new_border_points + count] = point;
							border = true;
						}
					}
					// Right
					else if (point.x > node_max.x - s_border_max[1])
					{
						//uint32_t count = atomicAdd(terrain_buffer->data[node_index].new_border_point_count[1], 1);	// TODO: Don't increment if number of points exceed max border points 
						uint32_t count = terrain_buffer->data[node_index].new_border_point_count[1];
						++terrain_buffer->data[node_index].new_border_point_count[1];

						if (count < max_new_border_points)
						{
							terrain_buffer->data[node_index].new_border_points[1 * max_new_border_points + count] = point;
							border = true;
						}
					}
					// Top
					else if (point.z > node_max.y - s_border_max[0])
					{
						//uint32_t count = atomicAdd(terrain_buffer->data[node_index].new_border_point_count[0], 1);	// TODO: Don't increment if number of points exceed max border points 
						uint32_t count = terrain_buffer->data[node_index].new_border_point_count[0];
						++terrain_buffer->data[node_index].new_border_point_count[0];

						if (count < max_new_border_points)
						{
							terrain_buffer->data[node_index].new_border_points[0 * max_new_border_points + count] = point;
							border = true;
						}
					}
					// Bottom
					else if (point.z < node_min.y + s_border_max[2])
					{
						//uint32_t count = atomicAdd(terrain_buffer->data[node_index].new_border_point_count[2], 1);	// TODO: Don't increment if number of points exceed max border points 
						uint32_t count = terrain_buffer->data[node_index].new_border_point_count[2];
						++terrain_buffer->data[node_index].new_border_point_count[2];

						if (count < max_new_border_points)
						{
							terrain_buffer->data[node_index].new_border_points[2 * max_new_border_points + count] = point;
							border = true;
						}
					}

					// Not a border point
					if (!border)
					{
						new_points[new_point_count] = point;
						++new_point_count;
					}
				}
			}
		}




		////// PREFIX SUM

		const uint32_t n = WORK_GROUP_SIZE;

		// Load into shared memory
		s_counts[thid] = new_point_count;

		//barrier();
		//memoryBarrierShared();

		if (thid == 0)
			s_total = s_counts[n - 1];

		//barrier();
		//memoryBarrierShared();

		int offset = 1;
		for (uint32_t d = n >> 1; d > 0; d >>= 1) // Build sum in place up the tree
		{
			//barrier();
			//memoryBarrierShared();
			if (thid < d)
			{
				uint32_t ai = offset * (2 * thid + 1) - 1;
				uint32_t bi = offset * (2 * thid + 2) - 1;
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
				uint32_t ai = offset * (2 * thid + 1) - 1;
				uint32_t bi = offset * (2 * thid + 2) - 1;

				uint32_t t = s_counts[ai];
				s_counts[ai] = s_counts[bi];
				s_counts[bi] += t;
			}
		}
		//barrier();
		//memoryBarrierShared();

		uint32_t prev_count = terrain_buffer->data[node_index].new_points_count;


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
		const uint32_t base_offset = prev_count + s_counts[thid];
		for (uint32_t i = 0; i < new_point_count && base_offset + i < num_new_points; ++i)
		{
			terrain_buffer->data[node_index].new_points[base_offset + i] = new_points[i];
		}
	}
#pragma endregion


#pragma region TRIANGULATE
	struct BorderEdge
	{
		glm::vec4 p1;
		glm::vec4 p2;
		uint32_t p1_index;
		uint32_t p2_index;
		uint32_t node_index;
		uint32_t pad;
	};

	const uint32_t max_border_edges = 100;
	/*shared*/ std::array<BorderEdge, max_border_edges * 3> s_border_edges;


	struct Edge
	{
		uint32_t p1;
		uint32_t p2;
	};

	const uint32_t max_triangles_to_remove = 50;
	/*shared*/ std::array<Edge, max_triangles_to_remove * 3> s_edges;

	/*shared*/ std::array<uint32_t, max_triangles_to_remove> s_triangles_to_remove;
	/*shared*/ uint32_t s_triangles_removed;
	/*shared*/ uint32_t s_new_triangle_count;

	/*shared*/ uint32_t s_index_count;
	/*shared*/ uint32_t s_triangle_count;
	/*shared*/ uint32_t s_vertex_count;

#define INVALID 9999999
#define EPSILON 1.0f - 0.0001f

	static /*shared*/ bool test;

	void remove_old_triangles(uint32_t node_index)
	{
		// Remove old triangles
		uint32_t last_valid_triangle = s_triangle_count - 1;
		for (int j = int(s_triangles_removed) - 1; j >= 0; --j)
		{
			const uint32_t index = s_triangles_to_remove[j];
			// Check border triangles
			// TODO: Check if the cc for the triangle goes over a border, THEN find the correct index
			for (uint32_t bb = 0; bb < 4; ++bb)
			{
				uint32_t count = terrain_buffer->data[node_index].border_count[bb];
				for (uint32_t tt = 0; tt < count; ++tt)
				{
					const uint32_t triangle_index = bb * max_border_triangle_count + tt;
					if (terrain_buffer->data[node_index].border_triangle_indices[triangle_index] == index)
					{
						--count;
						// Replace the found index with data from the back of the array
						terrain_buffer->data[node_index].border_triangle_indices[triangle_index]
							= terrain_buffer->data[node_index].border_triangle_indices[bb * max_border_triangle_count + count];

						terrain_buffer->data[node_index].border_diffs[triangle_index]
							= terrain_buffer->data[node_index].border_diffs[bb * max_border_triangle_count + count];

						// Find the new max border diff
						float biggest = terrain_buffer->data[node_index].border_diffs[bb * max_border_triangle_count + 0];
						for (uint32_t dd = 1; dd < count; ++dd)
						{
							const float temp = terrain_buffer->data[node_index].border_diffs[bb * max_border_triangle_count + dd];
							if (temp > biggest)
							{
								biggest = temp;
							}
						}
						terrain_buffer->data[node_index].border_max[bb] = biggest;
						--terrain_buffer->data[node_index].border_count[bb];
						break; // TODO: Add one more early exit for the other/outer for-loop?
					}
				}
			}

			// Remove triangle
			if (index < last_valid_triangle)
			{
				terrain_buffer->data[node_index].indices[index * 3 + 0] = terrain_buffer->data[node_index].indices[last_valid_triangle * 3 + 0];
				terrain_buffer->data[node_index].indices[index * 3 + 1] = terrain_buffer->data[node_index].indices[last_valid_triangle * 3 + 1];
				terrain_buffer->data[node_index].indices[index * 3 + 2] = terrain_buffer->data[node_index].indices[last_valid_triangle * 3 + 2];
				terrain_buffer->data[node_index].triangles[index].circumcentre = terrain_buffer->data[node_index].triangles[last_valid_triangle].circumcentre;
				terrain_buffer->data[node_index].triangles[index].circumradius = terrain_buffer->data[node_index].triangles[last_valid_triangle].circumradius;
				terrain_buffer->data[node_index].triangles[index].circumradius2 = terrain_buffer->data[node_index].triangles[last_valid_triangle].circumradius2;
			}

			--last_valid_triangle;
		}

		s_triangle_count -= s_triangles_removed;
		s_index_count = s_triangle_count * 3;
	}

	void add_triangle_to_border(uint32_t node_index, uint32_t border_index, uint32_t count, float diff)
	{
		terrain_buffer->data[node_index].border_triangle_indices[border_index * max_border_triangle_count + count] = s_triangle_count;
		terrain_buffer->data[node_index].border_diffs[border_index * max_border_triangle_count + count] = diff;
		terrain_buffer->data[node_index].border_max[border_index] = std:: max(terrain_buffer->data[node_index].border_max[border_index], diff);
		++terrain_buffer->data[node_index].border_count[border_index];
	}

	void triangulate_shader(uint32_t node_index)
	{
		const uint32_t thid = gl_GlobalInvocationID.x;

		const glm::vec2 node_min = terrain_buffer->data[node_index].min;
		const glm::vec2 node_max = terrain_buffer->data[node_index].max;
		const float side = node_max.x - node_min.x;

		////////////////////////////////////////
		// BORDER TRIANGULATION
		////////////////////////////////////////

		// Set shared variables
		if (thid == 0)
		{
			s_index_count = terrain_buffer->data[node_index].index_count;
			s_triangle_count = s_index_count / 3;
			s_triangles_removed = 0;
			s_vertex_count = terrain_buffer->data[node_index].vertex_count;
		}

		//barrier();
		//memoryBarrierShared();

		for (uint32_t bb = 0; bb < 4; ++bb)
		{
			const uint32_t new_points_count = terrain_buffer->data[node_index].new_border_point_count[bb];
			for (uint32_t n = 0; n < new_points_count && n < 1 && bb * max_new_border_points + n < 4 * max_new_border_points; ++n)
			{
				const glm::vec4 current_point = terrain_buffer->data[node_index].new_border_points[bb * max_new_border_points + n];

				const uint32_t triangle_count = terrain_buffer->data[node_index].border_count[bb];

				uint32_t i = thid;
				while (i < triangle_count)
				{
					const uint32_t triangle_index = terrain_buffer->data[node_index].border_triangle_indices[bb * max_border_triangle_count + i];
					const glm::vec2 circumcentre = terrain_buffer->data[node_index].triangles[triangle_index].circumcentre;
					const float circumradius2 = terrain_buffer->data[node_index].triangles[triangle_index].circumradius2;

					const float dx = current_point.x - circumcentre.x;
					const float dy = current_point.z - circumcentre.y;

					if (dx * dx + dy * dy < circumradius2)
					{
						// Add triangle edges to edge buffer
						const uint32_t index0 = terrain_buffer->data[node_index].indices[triangle_index * 3 + 0];
						const uint32_t index1 = terrain_buffer->data[node_index].indices[triangle_index * 3 + 1];
						const uint32_t index2 = terrain_buffer->data[node_index].indices[triangle_index * 3 + 2];
						const glm::vec4 p0 = terrain_buffer->data[node_index].positions[index0];
						const glm::vec4 p1 = terrain_buffer->data[node_index].positions[index1];
						const glm::vec4 p2 = terrain_buffer->data[node_index].positions[index2];

						// Store edges to be removed
						//uint32_t tr = atomicAdd(s_triangles_removed, 1);
						uint32_t tr = s_triangles_removed;
						++s_triangles_removed;

						if (tr >= max_border_edges || tr >= max_triangles_to_remove)
							break;

						uint32_t ec = tr * 3;
						// Edge 0
						bool biggest_point = p0.y < p1.y;
						s_border_edges[ec + 0].p1 = biggest_point ? p0 : p1;
						s_border_edges[ec + 0].p2 = !biggest_point ? p0 : p1;
						s_border_edges[ec + 0].p1_index = biggest_point ? index0 : index1;
						s_border_edges[ec + 0].p2_index = !biggest_point ? index0 : index1;
						s_border_edges[ec + 0].node_index = node_index;
						// Edge 1
						biggest_point = p1.y < p2.y;
						s_border_edges[ec + 1].p1 = biggest_point ? p1 : p2;
						s_border_edges[ec + 1].p2 = !biggest_point ? p1 : p2;
						s_border_edges[ec + 1].p1_index = biggest_point ? index1 : index2;
						s_border_edges[ec + 1].p2_index = !biggest_point ? index1 : index2;
						s_border_edges[ec + 1].node_index = node_index;
						// Edge 2
						biggest_point = p2.y < p0.y;
						s_border_edges[ec + 2].p1 = biggest_point ? p2 : p0;
						s_border_edges[ec + 2].p2 = !biggest_point ? p2 : p0;
						s_border_edges[ec + 2].p1_index = biggest_point ? index2 : index0;
						s_border_edges[ec + 2].p2_index = !biggest_point ? index2 : index0;
						s_border_edges[ec + 2].node_index = node_index;

						// Mark the triangle to be removed later
						s_triangles_to_remove[tr] = triangle_index;
					}

					i += WORK_GROUP_SIZE;
				}
				//barrier();
				//memoryBarrierShared();

				if (thid == 0)
				{
					test = false;
					// If the points was not within any border triangle then add it as a normal point to the node instead
					if (s_triangles_removed == 0)
					{
						test = true;
						if (terrain_buffer->data[node_index].new_points_count < num_new_points)
							terrain_buffer->data[node_index].new_points[terrain_buffer->data[node_index].new_points_count++] = glm::vec4(6);
						//continue;  // TODO: Fix this
					}
				}

				//barrier();
				//memoryBarrierShared();

				if (!test)
				{
					// Delete all doubly specified edges from edge buffer (this leaves the edges of the enclosing polygon only)
					const uint32_t edge_count = s_triangles_removed * 3;
					i = thid;
					while (i < edge_count)
					{
						bool found = false;
						for (uint32_t j = 0; j < edge_count; ++j)
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

						for (uint32_t j = 0; j < edge_count && j < max_triangles_to_remove * 3; ++j)
						{
							if (s_border_edges[j].p1.w > -0.5)
							{
								++s_new_triangle_count;
							}
						}
					}

					//barrier();
					//memoryBarrierShared();

					// If new triangles will not fit in index buffer, quit
					if (s_index_count + (s_new_triangle_count * 3) >= num_indices)
					{
						//finish = true;
						break;
					}

					if (thid == 0)
					{
						// Add to the triangle list all triangles formed between the point and the edges of the enclosing polygon
						for (uint32_t i = 0; i < edge_count; ++i)
						{
							if (s_border_edges[i].p1.w > -0.5)
							{
								glm::vec3 P = glm::vec3(s_border_edges[i].p1);
								glm::vec3 Q = glm::vec3(s_border_edges[i].p2);
								glm::vec3 R = glm::vec3(current_point);

								glm::vec2 PQ = normalize(glm::vec2(Q.x, Q.z) - glm::vec2(P.x, P.z));
								glm::vec2 PR = normalize(glm::vec2(R.x, R.z) - glm::vec2(P.x, P.z));
								glm::vec2 RQ = normalize(glm::vec2(Q.x, Q.z) - glm::vec2(R.x, R.z));

								float d1 = abs(dot(PQ, PR));
								float d2 = abs(dot(PR, RQ));
								float d3 = abs(dot(RQ, PQ));

								// Skip this triangle because it is too narrow (should only happen at borders)
								if (d1 > EPSILON || d2 > EPSILON || d3 > EPSILON)
								{
									continue;
								}

								// Make sure winding order is correct
								glm::vec3 n = cross(R - P, Q - P);
								if (n.y > 0)
								{
									glm::vec4 temp = s_border_edges[i].p1;
									s_border_edges[i].p1 = s_border_edges[i].p2;
									s_border_edges[i].p2 = temp;
									uint32_t temp2 = s_border_edges[i].p1_index;
									s_border_edges[i].p1_index = s_border_edges[i].p2_index;
									s_border_edges[i].p2_index = temp2;
								}

								// Set indices for the new triangle
								terrain_buffer->data[s_border_edges[i].node_index].indices[s_index_count + 0] = s_border_edges[i].p1_index;
								terrain_buffer->data[s_border_edges[i].node_index].indices[s_index_count + 1] = s_border_edges[i].p2_index;
								terrain_buffer->data[s_border_edges[i].node_index].indices[s_index_count + 2] = s_vertex_count;

								// Set circumcircles for the new triangle
								float a = distance(glm::vec2(P.x, P.z), glm::vec2(Q.x, Q.z));
								float b = distance(glm::vec2(P.x, P.z), glm::vec2(R.x, R.z));
								float c = distance(glm::vec2(R.x, R.z), glm::vec2(Q.x, Q.z));

								const glm::vec2 cc_center = find_circum_center(glm::vec2(P.x, P.z), glm::vec2(Q.x, Q.z), glm::vec2(R.x, R.z));
								const float cc_radius2 = find_circum_radius_squared(a, b, c);
								const float cc_radius = sqrt(cc_radius2);

								terrain_buffer->data[s_border_edges[i].node_index].triangles[s_triangle_count].circumcentre = cc_center;
								terrain_buffer->data[s_border_edges[i].node_index].triangles[s_triangle_count].circumradius = cc_radius;
								terrain_buffer->data[s_border_edges[i].node_index].triangles[s_triangle_count].circumradius2 = cc_radius2;

								// Check if the triangle is a border triangle
								// Left
								uint32_t count = terrain_buffer->data[s_border_edges[i].node_index].border_count[3];
								float diff = node_min.x + cc_radius - cc_center.x;
								if (diff > 0 && count < max_border_triangle_count)
								{
									add_triangle_to_border(s_border_edges[i].node_index, 3, count, diff);
								}
								// Right
								count = terrain_buffer->data[s_border_edges[i].node_index].border_count[1];
								diff = cc_center.x + cc_radius - node_max.x;
								if (diff > 0 && count < max_border_triangle_count)
								{
									add_triangle_to_border(s_border_edges[i].node_index, 1, count, diff);
								}
								// Top
								count = terrain_buffer->data[s_border_edges[i].node_index].border_count[0];
								diff = cc_center.y + cc_radius - node_max.y;
								if (diff > 0 && count < max_border_triangle_count)
								{
									add_triangle_to_border(s_border_edges[i].node_index, 0, count, diff);
								}
								// Bottom
								count = terrain_buffer->data[s_border_edges[i].node_index].border_count[2];
								diff = node_min.y + cc_radius - cc_center.y;
								if (diff > 0 && count < max_border_triangle_count)
								{
									add_triangle_to_border(s_border_edges[i].node_index, 2, count, diff);
								}

								s_index_count += 3;
								++s_triangle_count;
							}
						}

						remove_old_triangles(node_index);

						// Insert new point
						terrain_buffer->data[s_border_edges[i].node_index].positions[s_vertex_count] = current_point;
						++s_vertex_count;

						s_triangles_removed = 0;
					}
				}

				//barrier();
				//memoryBarrierShared();
				//memoryBarrierBuffer();
			}
			terrain_buffer->data[node_index].new_border_point_count[bb] = 0;
		}

























		if (terrain_buffer->data[node_index].new_points_count == 0)
		{
			terrain_buffer->data[node_index].vertex_count = s_vertex_count;
			terrain_buffer->data[node_index].index_count = s_index_count;
			return;
		}
		////////////////////////////////////////
		// NORMAL TRIANGULATION
		////////////////////////////////////////

		//barrier();
		//memoryBarrierShared();
		//memoryBarrierBuffer();

		// Set shared variables
		if (thid == 0)
		{
			//s_index_count = terrain_buffer->data[node_index].index_count;
			//s_triangle_count = s_index_count / 3;
			//s_triangles_removed = 0;
			//s_vertex_count = terrain_buffer->data[node_index].vertex_count;
		}

		//barrier();
		//memoryBarrierShared();

		bool finish = false;

		uint32_t new_points_count = terrain_buffer->data[node_index].new_points_count;
		for (uint32_t n = 0; n < new_points_count && s_vertex_count < num_vertices && !finish; ++n)
		{
			glm::vec4 current_point = terrain_buffer->data[node_index].new_points[n];

			// Check distance from circumcircles to new point
			uint32_t i = thid;
			while (i < s_triangle_count)
			{
				const glm::vec2 circumcentre = terrain_buffer->data[node_index].triangles[i].circumcentre;
				const float circumradius2 = terrain_buffer->data[node_index].triangles[i].circumradius2;

				const float dx = current_point.x - circumcentre.x;
				const float dy = current_point.z - circumcentre.y;
				if (dx * dx + dy * dy < circumradius2)
				{
					// Add triangle edges to edge buffer
					//uint32_t tr = atomicAdd(s_triangles_removed, 1);
					uint32_t tr = s_triangles_removed;
					++s_triangles_removed;

					if (tr >= max_triangles_to_remove)
						break;

					uint32_t ec = tr * 3; //atomicAdd(s_edge_count, 3);
					uint32_t index_offset = i * 3;
					uint32_t index0 = terrain_buffer->data[node_index].indices[index_offset + 0];
					uint32_t index1 = terrain_buffer->data[node_index].indices[index_offset + 1];
					uint32_t index2 = terrain_buffer->data[node_index].indices[index_offset + 2];


					// Edge 1
					s_edges[ec + 0].p1 = std::min(index0, index1);
					s_edges[ec + 0].p2 = std::max(index0, index1);
					// Edge 2
					s_edges[ec + 1].p1 = std::min(index1, index2);
					s_edges[ec + 1].p2 = std::max(index1, index2);
					// Edge 3
					s_edges[ec + 2].p1 = std::min(index2, index0);
					s_edges[ec + 2].p2 = std::max(index2, index0);

					// Mark the triangle to be removed later
					s_triangles_to_remove[tr] = i;
				}

				i += WORK_GROUP_SIZE;
			}
			//barrier();
			//memoryBarrierShared();

			// Delete all doubly specified edges from edge buffer (this leaves the edges of the enclosing polygon only)
			const uint32_t edge_count = std::min(s_triangles_removed, max_triangles_to_remove) * 3;
			i = thid;
			while (i < edge_count)
			{
				bool found = false;
				for (uint32_t j = 0; j < edge_count; ++j)
				{
					if (i != j &&
						s_edges[i].p1 == s_edges[j].p1 &&
						s_edges[i].p2 == s_edges[j].p2)
					{
						// Mark as invalid
						s_edges[j].p1 = INVALID;
						found = true;
					}
				}
				if (found)
					s_edges[i].p1 = INVALID;
				i += WORK_GROUP_SIZE;
			}

			//barrier();
			//memoryBarrierShared();

			// Count the number of new triangles to create
			if (thid == 0)
			{
				s_new_triangle_count = 0;

				for (uint32_t j = 0; j < edge_count && j < max_triangles_to_remove * 3; ++j)
				{
					if (s_edges[j].p1 != INVALID)
					{
						++s_new_triangle_count;
					}
				}
			}

			//barrier();
			//memoryBarrierShared();

			// If new triangles will not fit in index buffer, quit
			if (s_index_count + (s_new_triangle_count * 3) >= num_indices)
			{
				finish = true;
				break;
			}

			if (thid == 0)
			{
				// Add to the triangle list all triangles formed between the point and the edges of the enclosing polygon
				for (uint32_t i = 0; i < edge_count; ++i)
				{
					if (s_edges[i].p1 != INVALID)
					{
						glm::vec3 P = glm::vec3(terrain_buffer->data[node_index].positions[s_edges[i].p1]);
						glm::vec3 Q = glm::vec3(terrain_buffer->data[node_index].positions[s_edges[i].p2]);
						glm::vec3 R = glm::vec3(current_point);

						glm::vec2 PQ = normalize(glm::vec2(Q.x, Q.z) - glm::vec2(P.x, P.z));
						glm::vec2 PR = normalize(glm::vec2(R.x, R.z) - glm::vec2(P.x, P.z));
						glm::vec2 RQ = normalize(glm::vec2(Q.x, Q.z) - glm::vec2(R.x, R.z));

						float d1 = abs(dot(PQ, PR));
						float d2 = abs(dot(PR, RQ));
						float d3 = abs(dot(RQ, PQ));

						// Skip this triangle because it is too narrow (should only happen at borders)
						if (d1 > EPSILON || d2 > EPSILON || d3 > EPSILON)
						{
							continue;
						}

						// Make sure winding order is correct
						glm::vec3 n = cross(R - P, Q - P);
						if (n.y > 0)
						{
							uint32_t temp = s_edges[i].p1;
							s_edges[i].p1 = s_edges[i].p2;
							s_edges[i].p2 = temp;
						}

						// Set indices for the new triangle
						terrain_buffer->data[node_index].indices[s_index_count + 0] = s_edges[i].p1;
						terrain_buffer->data[node_index].indices[s_index_count + 1] = s_edges[i].p2;
						terrain_buffer->data[node_index].indices[s_index_count + 2] = s_vertex_count;

						// Set circumcircles for the new triangle
						float a = distance(glm::vec2(P.x, P.z), glm::vec2(Q.x, Q.z));
						float b = distance(glm::vec2(P.x, P.z), glm::vec2(R.x, R.z));
						float c = distance(glm::vec2(R.x, R.z), glm::vec2(Q.x, Q.z));

						const glm::vec2 cc_center = find_circum_center(glm::vec2(P.x, P.z), glm::vec2(Q.x, Q.z), glm::vec2(R.x, R.z));
						const float cc_radius2 = find_circum_radius_squared(a, b, c);
						const float cc_radius = sqrt(cc_radius2);

						terrain_buffer->data[node_index].triangles[s_triangle_count].circumcentre = cc_center;
						terrain_buffer->data[node_index].triangles[s_triangle_count].circumradius = cc_radius;
						terrain_buffer->data[node_index].triangles[s_triangle_count].circumradius2 = cc_radius2;

						// Check if the triangle is a border triangle
						// Left
						uint32_t count = terrain_buffer->data[node_index].border_count[3];
						float diff = node_min.x + cc_radius - cc_center.x;
						if (diff > 0 && count < max_border_triangle_count)
						{
							add_triangle_to_border(node_index, 3, count, diff);
						}
						// Right
						count = terrain_buffer->data[node_index].border_count[1];
						diff = cc_center.x + cc_radius - node_max.x;
						if (diff > 0 && count < max_border_triangle_count)
						{
							add_triangle_to_border(node_index, 1, count, diff);
						}
						// Top
						count = terrain_buffer->data[node_index].border_count[0];
						diff = cc_center.y + cc_radius - node_max.y;
						if (diff > 0 && count < max_border_triangle_count)
						{
							add_triangle_to_border(node_index, 0, count, diff);
						}
						// Bottom
						count = terrain_buffer->data[node_index].border_count[2];
						diff = node_min.y + cc_radius - cc_center.y;
						if (diff > 0 && count < max_border_triangle_count)
						{
							add_triangle_to_border(node_index, 2, count, diff);
						}

						s_index_count += 3;
						++s_triangle_count;
					}
				}

				remove_old_triangles(node_index);

				// Insert new point
				terrain_buffer->data[node_index].positions[s_vertex_count] = current_point;
				++s_vertex_count;

				s_triangles_removed = 0;
			}

			//barrier();
			//memoryBarrierShared();
			//memoryBarrierBuffer();
		}

		// Write new buffer lenghts to buffer
		if (thid == 0)
		{
			terrain_buffer->data[node_index].vertex_count = s_vertex_count;
			terrain_buffer->data[node_index].index_count = s_index_count;

			terrain_buffer->data[node_index].new_points_count = 0;
		}
	}

#pragma endregion
}