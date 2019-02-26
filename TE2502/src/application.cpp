#include "application.hpp"
#include "graphics/graphics_queue.hpp"
#include "graphics/compute_queue.hpp"
#include "graphics/transfer_queue.hpp"
#include "graphics/gpu_memory.hpp"
#include "graphics/gpu_image.hpp"
#include "graphics/gpu_buffer.hpp"
#include "graphics/pipeline_layout.hpp"
#include "quadtree.hpp"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

#include <string>
#include <glm/gtc/matrix_transform.hpp>
#include <array>

//#define RAY_MARCH_WINDOW

/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////

int num_indices = 759;
int num_vertices = 700;
const uint32_t num_nodes = 4;
const uint32_t num_new_points = 4096;
const uint32_t max_new_points = 4096;
const uint32_t quadtree_levels = 1;

const uint32_t WORK_GROUP_SIZE = 1;

struct Triangle
{
	glm::vec2 circumcentre;
	float circumradius;
	uint32_t pad;
};

struct terrain_data_t
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
	// }

	std::vector<uint32_t> indices;
	std::vector<glm::vec4> positions;
	std::vector<Triangle> triangles;
	std::vector<glm::vec4> new_points;
};

struct terrain_buffer_t
{
	uint32_t quadtree_index_map[4];
	glm::vec2 quadtree_min;
	glm::vec2 quadtree_max;
	terrain_data_t data;
};

std::array<glm::vec2, max_new_points> new_points;
//uint32_t s_counts[WORK_GROUP_SIZE];
std::array<uint32_t, 2> s_counts;
uint32_t s_total;

terrain_buffer_t terrain_buffer;

struct Edge
{
	uint32_t p1;
	uint32_t p2;
};

std::array<Edge, 600> s_edges;

#define INDICES_TO_STORE 300
#define TRIANGLES_TO_STORE 100

std::array<uint32_t, TRIANGLES_TO_STORE> s_triangles_to_remove;

uint32_t s_triangles_removed;

uint32_t s_index_count;
uint32_t s_triangle_count;
uint32_t s_vertex_count;

#define INVALID 999999
#define EPSILON 1.0f - 0.0001f

void line_from_points(glm::vec2 p1, glm::vec2 p2, float& a, float& b, float& c)
{
	a = p2.y - p1.y;
	b = p1.x - p2.x;
	c = a * p1.x + b * p2.y;
}
void perpendicular_bisector_from_line(glm::vec2 p1, glm::vec2 p2, float& a, float& b, float& c)
{
	glm::vec2 mid_point = glm::vec2((p1.x + p2.x) * 0.5, (p1.y + p2.y) * 0.5);

	// c = -bx + ay 
	c = -b * mid_point.x + a * mid_point.y;

	float temp = a;
	a = -b;
	b = temp;
}
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
float find_circum_radius_squared(glm::vec2 P, glm::vec2 Q, glm::vec2 R)
{
	float a = distance(P, Q);
	float b = distance(P, R);
	float c = distance(R, Q);

	return (a * a * b * b * c * c) / ((a + b + c) * (b + c - a) * (c + a - b) * (a + b - c));
}
float find_circum_radius_squared(float a, float b, float c)
{
	return (a * a * b * b * c * c) / ((a + b + c) * (b + c - a) * (c + a - b) * (a + b - c));
}
bool clip(glm::vec4 p)
{
	return (abs(p.x) <= p.w &&
		abs(p.y) <= p.w &&
		abs(p.z) <= p.w);
}

void gen_terrain()
{
	uint32_t GRID_SIDE = 5;
	struct f_data { glm::vec2 min; glm::vec2 max; };
	f_data frame_data{ {-250, 0}, {0, 250} };

	{
		terrain_buffer.data.index_count = 6 * (GRID_SIDE - 1) * (GRID_SIDE - 1);
		terrain_buffer.data.instance_count = 1;
		terrain_buffer.data.first_index = 0;
		terrain_buffer.data.vertex_offset = 0;
		terrain_buffer.data.first_instance = 0;

		terrain_buffer.data.vertex_count = GRID_SIDE * GRID_SIDE;
		terrain_buffer.data.new_points_count = 0;

		terrain_buffer.data.min = frame_data.min;
		terrain_buffer.data.max = frame_data.max;
	}

	// Positions
	uint32_t i = 0;
	while (i < GRID_SIDE * GRID_SIDE)
	{
		float x = frame_data.min.x + ((i % GRID_SIDE) / float(GRID_SIDE - 1)) * (frame_data.max.x - frame_data.min.x);
		float z = frame_data.min.y + float(i / GRID_SIDE) / float(GRID_SIDE - 1) * (frame_data.max.y - frame_data.min.y);

		terrain_buffer.data.positions[i] = glm::vec4(x, 0.0f, z, 1.0);

		i++;
	}

	// Triangles
	i = 0;
	while (i < (GRID_SIDE - 1) * (GRID_SIDE - 1))
	{
		uint32_t y = i / (GRID_SIDE - 1);
		uint32_t x = i % (GRID_SIDE - 1);
		uint32_t index = y * GRID_SIDE + x;

		// Indices
		uint32_t offset = i * 6;
		terrain_buffer.data.indices[offset] = index;
		terrain_buffer.data.indices[offset + 1] = index + GRID_SIDE + 1;
		terrain_buffer.data.indices[offset + 2] = index + 1;

		terrain_buffer.data.indices[offset + 3] = index;
		terrain_buffer.data.indices[offset + 4] = index + GRID_SIDE;
		terrain_buffer.data.indices[offset + 5] = index + GRID_SIDE + 1;

		// Circumcentres
		offset = i * 2;
		glm::vec2 P1 = glm::vec2(terrain_buffer.data.positions[index].x, terrain_buffer.data.positions[index].z);
		glm::vec2 Q1 = glm::vec2(terrain_buffer.data.positions[index + GRID_SIDE + 1].x, terrain_buffer.data.positions[index + GRID_SIDE + 1].z);
		glm::vec2 R1 = glm::vec2(terrain_buffer.data.positions[index + 1].x, terrain_buffer.data.positions[index + 1].z);
		terrain_buffer.data.triangles[offset].circumcentre = find_circum_center(P1, Q1, R1);

		glm::vec2 P2 = glm::vec2(terrain_buffer.data.positions[index].x, terrain_buffer.data.positions[index].z);
		glm::vec2 Q2 = glm::vec2(terrain_buffer.data.positions[index + GRID_SIDE].x, terrain_buffer.data.positions[index + GRID_SIDE].z);
		glm::vec2 R2 = glm::vec2(terrain_buffer.data.positions[index + GRID_SIDE + 1].x, terrain_buffer.data.positions[index + GRID_SIDE + 1].z);
		terrain_buffer.data.triangles[offset + 1].circumcentre = find_circum_center(P2, Q2, R2);

		// Circumradii
		terrain_buffer.data.triangles[offset].circumradius = find_circum_radius_squared(P1, Q1, R1);
		terrain_buffer.data.triangles[offset + 1].circumradius = find_circum_radius_squared(P2, Q2, R2);

		i += WORK_GROUP_SIZE;
	}
}

void triangle_process(const glm::mat4& vp)
{
	struct f_d { glm::mat4 vp; };
	f_d frame_data{vp};

	const uint32_t thid = 0;
	const uint32_t index_count = terrain_buffer.data.index_count;

	uint32_t new_point_count = 0;

	if (true)
	{
		// For every triangle
		for (uint32_t i = thid * 3; i + 3 < index_count && new_point_count < max_new_points; i += WORK_GROUP_SIZE * 3)
		{
			// Get vertices
			glm::vec4 v0 = terrain_buffer.data.positions[terrain_buffer.data.indices[i]];
			glm::vec4 v1 = terrain_buffer.data.positions[terrain_buffer.data.indices[i + 1]];
			glm::vec4 v2 = terrain_buffer.data.positions[terrain_buffer.data.indices[i + 2]];

			// Get clipspace coordinates
			glm::vec4 c0 = frame_data.vp * v0;
			glm::vec4 c1 = frame_data.vp * v1;
			glm::vec4 c2 = frame_data.vp * v2;

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
				float s = (a + b + c) * 0.5;

				float area = pow(/*sqrt*/(s * (s - a) * (s - b) * (s - c)), 0.0);

				glm::vec3 mid = (glm::vec3(v0) + glm::vec3(v1) + glm::vec3(v2)) / 3.0f;
				float curv = pow(rand() / RAND_MAX, 0.0f);

				//if (rand() / RAND_MAX > 0.94f)
				{
					new_points[new_point_count] = glm::vec2(mid.x, mid.z);
					++new_point_count;
				}
			}
		}
	}




	////// PREFIX SUM

	const uint32_t n = WORK_GROUP_SIZE;

	// Load into shared memory
	s_counts[thid] = new_point_count;

	if (thid == 0)
		s_total = s_counts[n - 1];

	int offset = 1;
	for (uint32_t d = n >> 1; d > 0; d >>= 1) // Build sun in place up the tree
	{
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
		if (thid < d)
		{
			uint32_t ai = offset * (2 * thid + 1) - 1;
			uint32_t bi = offset * (2 * thid + 2) - 1;

			uint32_t t = s_counts[ai];
			s_counts[ai] = s_counts[bi];
			s_counts[bi] += t;
		}
	}

	// Make sure the total is saved as well
	if (thid == 0)
	{
		s_total += s_counts[n - 1];
		terrain_buffer.data.new_points_count = std::min(s_total, num_new_points);
	}

	// Write points to output storage buffer
	const uint32_t base_offset = s_counts[thid];
	for (uint32_t i = 0; i < new_point_count && base_offset + i < num_new_points; ++i)
	{
		//output_data.points[base_offset + i] = new_points[i];
		terrain_buffer.data.new_points[base_offset + i] = glm::vec4(new_points[i].x, 0, new_points[i].y, 1.0);
	}
}

void triangulate()
{
	if (terrain_buffer.data.new_points_count == 0)
		return;

	const uint32_t thid = 0;
	bool finish = false;

	// Set shared variables
	if (thid == 0)
	{
		s_index_count = terrain_buffer.data.index_count;
		s_triangle_count = s_index_count / 3;
		s_triangles_removed = 0;
		s_vertex_count = terrain_buffer.data.vertex_count;
	}

	const uint32_t new_points_count = terrain_buffer.data.new_points_count;
	for (uint32_t n = 0; n < new_points_count && s_vertex_count < num_vertices && !finish; ++n)
	{
		glm::vec4 current_point = terrain_buffer.data.new_points[n];

		// Check distance from circumcircles to new point
		uint32_t i = thid;
		while (i < s_triangle_count)
		{
			glm::vec2 circumcentre = terrain_buffer.data.triangles[i].circumcentre;
			float circumradius = terrain_buffer.data.triangles[i].circumradius;

			float dx = current_point.x - circumcentre.x;
			float dy = current_point.z - circumcentre.y;
			if (dx * dx + dy * dy < circumradius)
			{
				// Add triangle edges to edge buffer
				uint32_t tr = s_triangles_removed;
				s_triangles_removed++;
				uint32_t ec = tr * 3; //atomicAdd(s_edge_count, 3);
				uint32_t index_offset = i * 3;
				uint32_t index0 = terrain_buffer.data.indices[index_offset + 0];
				uint32_t index1 = terrain_buffer.data.indices[index_offset + 1];
				uint32_t index2 = terrain_buffer.data.indices[index_offset + 2];
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

		if (s_index_count + (s_triangles_removed + 2) * 9 >= num_indices)
		{
			finish = true;
			break;
		}

		// Delete all doubly specified edges from edge buffer (this leaves the edges of the enclosing polygon only)
		const uint32_t edge_count = s_triangles_removed * 3;
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

		if (thid == 0)
		{
			uint32_t old_index_count = s_index_count;
			uint32_t old_triangle_count = s_triangle_count;
			bool all_valid = true;

			// Add to the triangle list all triangles formed between the point and the edges of the enclosing polygon
			for (uint32_t i = 0; i < edge_count; ++i)
			{
				if (s_edges[i].p1 != INVALID)
				{
					glm::vec3 P = glm::vec3(terrain_buffer.data.positions[s_edges[i].p1]);
					glm::vec3 Q = glm::vec3(terrain_buffer.data.positions[s_edges[i].p2]);
					glm::vec3 R = glm::vec3(current_point);

					// Make sure winding order is correct
					glm::vec3 n = cross(R - P, Q - P);
					if (n.y > 0)
					{
						uint32_t temp = s_edges[i].p1;
						s_edges[i].p1 = s_edges[i].p2;
						s_edges[i].p2 = temp;
					}

					// Set indices for the new triangle
					assert(s_edges[i].p1 < s_vertex_count);
					assert(s_edges[i].p2 < s_vertex_count);
					assert(s_vertex_count <= num_vertices);
					terrain_buffer.data.indices[s_index_count + 0] = s_edges[i].p1;
					terrain_buffer.data.indices[s_index_count + 1] = s_edges[i].p2;
					terrain_buffer.data.indices[s_index_count + 2] = s_vertex_count;
					// Set circumcircles for the new triangle
					float a = distance(glm::vec2(P.x, P.z), glm::vec2(Q.x, Q.z));
					float b = distance(glm::vec2(P.x, P.z), glm::vec2(R.x, R.z));
					float c = distance(glm::vec2(R.x, R.z), glm::vec2(Q.x, Q.z));

					glm::vec2 PQ = glm::normalize(glm::vec2(Q.x, Q.z) - glm::vec2(P.x, P.z));
					glm::vec2 PR = glm::normalize(glm::vec2(R.x, R.z) - glm::vec2(P.x, P.z));
					glm::vec2 RQ = glm::normalize(glm::vec2(Q.x, Q.z) - glm::vec2(R.x, R.z));
					float d1 = abs(dot(PQ, PR));
					float d2 = abs(dot(PR, RQ));
					float d3 = abs(dot(RQ, PQ));

					if (d1 > EPSILON || d2 > EPSILON || d3 > EPSILON)
					{
						all_valid = false;
						break;
					}
					terrain_buffer.data.triangles[s_triangle_count].circumcentre = find_circum_center(glm::vec2(P.x, P.z), glm::vec2(Q.x, Q.z), glm::vec2(R.x, R.z));
					terrain_buffer.data.triangles[s_triangle_count].circumradius = find_circum_radius_squared(a, b, c);

					s_index_count += 3;
					++s_triangle_count;
				}
			}
			// Remove old triangles
			if (all_valid)
			{
				uint32_t last_valid_triangle = s_triangle_count - 1;
				for (int j = int(s_triangles_removed) - 1; j >= 0; --j)
				{
					uint32_t index = s_triangles_to_remove[j];
					if (index < last_valid_triangle)
					{
						assert(terrain_buffer.data.indices[last_valid_triangle * 3 + 0] <= s_vertex_count);
						assert(terrain_buffer.data.indices[last_valid_triangle * 3 + 1] <= s_vertex_count);
						assert(terrain_buffer.data.indices[last_valid_triangle * 3 + 2] <= s_vertex_count);

						terrain_buffer.data.indices[index * 3 + 0] = terrain_buffer.data.indices[last_valid_triangle * 3 + 0];
						terrain_buffer.data.indices[index * 3 + 1] = terrain_buffer.data.indices[last_valid_triangle * 3 + 1];
						terrain_buffer.data.indices[index * 3 + 2] = terrain_buffer.data.indices[last_valid_triangle * 3 + 2];
						terrain_buffer.data.triangles[index].circumcentre = terrain_buffer.data.triangles[last_valid_triangle].circumcentre;
						terrain_buffer.data.triangles[index].circumradius = terrain_buffer.data.triangles[last_valid_triangle].circumradius;
					}
					--last_valid_triangle;
				}
				s_triangle_count -= s_triangles_removed;
				s_index_count = s_triangle_count * 3;

				// Insert new point
				terrain_buffer.data.positions[s_vertex_count] = current_point;
				++s_vertex_count;
			}
			// Exclude the new point and revert
			else
			{
				s_index_count = old_index_count;
				s_triangle_count = old_triangle_count;
			}

			s_triangles_removed = 0;
		}
	}

	// Write new buffer lengths to buffer
	if (thid == 0)
	{
		terrain_buffer.data.vertex_count = s_vertex_count;
		assert(terrain_buffer.data.positions[s_vertex_count - 1].w > 0.5f);

		terrain_buffer.data.index_count = s_index_count;

		terrain_buffer.data.new_points_count = 0;
	}
}

void cpu_triangulate_test(const glm::mat4& vp, DebugDrawer& dd, Camera& camera)
{
	static int vistris_begin = 0;
	static int vistris_end = num_indices / 3;

	ImGui::Begin("Triangle Debug Shit");

	if (ImGui::Button("EXPAND!! XD"))
	{
		num_indices += 3;
	}

	if (ImGui::Button("retract :("))
		num_indices -= 3;

	ImGui::DragInt("Num Indices", &num_indices, 1.0f, 100, 4700);
	ImGui::DragInt("Num Vertices", &num_vertices, 1.0f, 100, 4700);

	ImGui::Text("Indices: %u", num_indices);


	if (ImGui::Button("SET CAMERAAAA"))
	{
		camera.set_pos({ -60.324883f, 217.934616f, 71.968697f });
		camera.set_yaw_pitch(1.72681379f, 1.07079637);
	}

	ImGui::DragInt("Vistris B", &vistris_begin, 0.2f, 0, num_indices / 3);
	ImGui::DragInt("Vistris E", &vistris_end, 0.2f, 0, num_indices / 3);


	ImGui::End();

	terrain_buffer.data.indices.resize(num_indices);
	terrain_buffer.data.positions.resize(num_vertices);
	terrain_buffer.data.triangles.resize(num_indices / 3);
	terrain_buffer.data.new_points.resize(num_new_points);

	terrain_buffer.data.new_points_count = 0;
	terrain_buffer.data.vertex_count = 0;
	terrain_buffer.data.index_count = 0;

	gen_terrain();
	
	for (size_t i = 0; i < 6; i++)
	{
		triangle_process(vp);
		triangulate();
	}

	for (size_t i = vistris_begin * 3; i < terrain_buffer.data.index_count && i < vistris_end * 3; i += 3)
	{
		assert(terrain_buffer.data.positions[terrain_buffer.data.indices[i    ]].w > 0.5f);
		assert(terrain_buffer.data.positions[terrain_buffer.data.indices[i + 1]].w > 0.5f);
		assert(terrain_buffer.data.positions[terrain_buffer.data.indices[i + 2]].w > 0.5f);

		dd.draw_line(glm::vec3(terrain_buffer.data.positions[terrain_buffer.data.indices[i]]), glm::vec3(terrain_buffer.data.positions[terrain_buffer.data.indices[i + 1]]), glm::vec3(1,0,0));
		dd.draw_line(glm::vec3(terrain_buffer.data.positions[terrain_buffer.data.indices[i + 1]]), glm::vec3(terrain_buffer.data.positions[terrain_buffer.data.indices[i + 2]]), glm::vec3(1,0,0));
		dd.draw_line(glm::vec3(terrain_buffer.data.positions[terrain_buffer.data.indices[i + 2]]), glm::vec3(terrain_buffer.data.positions[terrain_buffer.data.indices[i]]), glm::vec3(1, 0, 0));

		glm::vec3 mid = (terrain_buffer.data.positions[terrain_buffer.data.indices[i]] + terrain_buffer.data.positions[terrain_buffer.data.indices[i + 1]] + terrain_buffer.data.positions[terrain_buffer.data.indices[i + 2]]) / 3.0f;

		dd.draw_line(mid, glm::vec3(terrain_buffer.data.positions[terrain_buffer.data.indices[i]]), glm::vec3(0, 1, 0));
		dd.draw_line(mid, glm::vec3(terrain_buffer.data.positions[terrain_buffer.data.indices[i + 1]]), glm::vec3(0, 1, 0));
		dd.draw_line(mid, glm::vec3(terrain_buffer.data.positions[terrain_buffer.data.indices[i + 2]]), glm::vec3(0, 1, 0));

		size_t tri_index = i / 3;
		glm::vec3 c = { terrain_buffer.data.triangles[tri_index].circumcentre.x, 0, terrain_buffer.data.triangles[tri_index].circumcentre.y };
		float r = terrain_buffer.data.triangles[tri_index].circumradius;

		glm::vec3 dir = glm::normalize(glm::vec3(1, 0, -1));
		glm::vec3 dir2 = glm::normalize(glm::vec3(-1, 0, -1));

		dd.draw_line(c, c + glm::vec3(0, 0, 1) * sqrtf(r), { 0, 0, 1 });
		dd.draw_line(c, c + dir				   * sqrtf(r), { 0, 0, 1 });
		dd.draw_line(c, c + dir2			   * sqrtf(r), { 0, 0, 1 });
	}
}

/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////

void error_callback(int error, const char* description)
{
	fprintf(stderr, "Error: %s\n", description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GLFW_TRUE);
}

Application::Application() : m_tfile("shaders/vars.txt", "shaders/")
{
	m_tfile.compile_shaders();

	glfwSetErrorCallback(error_callback);

	int err = glfwInit();
	assert(err == GLFW_TRUE);

#ifdef RAY_MARCH_WINDOW
	m_ray_march_window = new Window(1080, 720, "TE2502 - Ray March", m_vulkan_context, false);
#endif
	m_window = new Window(1080, 720, "TE2502 - Main", m_vulkan_context, true);
	m_main_camera = new Camera(m_window->get_glfw_window());
	m_debug_camera = new Camera(m_window->get_glfw_window());
	m_current_camera = m_main_camera;

	glfwSetWindowPos(m_window->get_glfw_window(), 840, 100);

#ifdef RAY_MARCH_WINDOW
	glfwSetWindowPos(m_ray_march_window->get_glfw_window(), 0, 100);


	// Ray marching
	m_ray_march_set_layout = DescriptorSetLayout(m_vulkan_context);
	m_ray_march_set_layout.add_storage_image(VK_SHADER_STAGE_COMPUTE_BIT);
	m_ray_march_set_layout.create();

	m_ray_march_image_descriptor_set = DescriptorSet(m_vulkan_context, m_ray_march_set_layout);

	m_ray_march_pipeline_layout = PipelineLayout(m_vulkan_context);
	m_ray_march_pipeline_layout.add_descriptor_set_layout(m_ray_march_set_layout);
	// Set up push constant range for frame data
	{
		VkPushConstantRange push_range;
		push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		push_range.offset = 0;
		push_range.size = sizeof(RayMarchFrameData);

		m_ray_march_pipeline_layout.create(&push_range);
	}

	m_ray_march_compute_queue = m_vulkan_context.create_compute_queue();
	// !Ray marching
#endif

	// Point generation
	m_em_group_size = m_tfile.get_u32("ERROR_METRIC_GROUP_SIZE");

	m_main_queue = m_vulkan_context.create_graphics_queue();

	glfwSetKeyCallback(m_window->get_glfw_window(), key_callback);

	imgui_setup();


	// Set up terrain generation/drawing
	VkDeviceSize num_indices = m_tfile.get_u64("TERRAIN_GENERATE_NUM_INDICES");
	VkDeviceSize num_vertices = m_tfile.get_u64("TERRAIN_GENERATE_NUM_VERTICES");
	VkDeviceSize num_nodes = m_tfile.get_u64("TERRAIN_GENERATE_NUM_NODES");
	VkDeviceSize num_new_points = m_tfile.get_u64("TRIANGULATE_MAX_NEW_POINTS");
	uint32_t num_levels = m_tfile.get_u32("QUADTREE_LEVELS");
	m_quadtree = Quadtree(m_vulkan_context, 50000.0f, num_levels, num_nodes, num_indices, num_vertices, num_new_points, *m_window, m_main_queue);
#ifdef RAY_MARCH_WINDOW
	m_ray_march_window_states.swapchain_framebuffers.resize(m_ray_march_window->get_swapchain_size());
	for (uint32_t i = 0; i < m_ray_march_window->get_swapchain_size(); i++)
	{
		m_ray_march_window_states.swapchain_framebuffers[i] = Framebuffer(m_vulkan_context);
		m_ray_march_window_states.swapchain_framebuffers[i].add_attachment(m_ray_march_window->get_swapchain_image_view(i));
		m_ray_march_window_states.swapchain_framebuffers[i].create(m_imgui_vulkan_state.render_pass, m_ray_march_window->get_size().x, m_ray_march_window->get_size().y);
	}
#endif

	m_window_states.depth_memory = m_vulkan_context.allocate_device_memory(m_window->get_size().x * m_window->get_size().y * 2 * m_window->get_swapchain_size() * 4 + 1024);
	m_window_states.swapchain_framebuffers.resize(m_window->get_swapchain_size());
	m_imgui_vulkan_state.swapchain_framebuffers.resize(m_window->get_swapchain_size());
	for (uint32_t i = 0; i < m_window->get_swapchain_size(); i++)
	{
		VkExtent3D depth_size;
		depth_size.width = m_window->get_size().x;
		depth_size.height = m_window->get_size().y;
		depth_size.depth = 1;
		m_window_states.depth_images.push_back(GPUImage(m_vulkan_context, depth_size, 
			VK_FORMAT_D32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, 
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
			m_window_states.depth_memory));

		m_window_states.depth_image_views.push_back(ImageView(m_vulkan_context, m_window_states.depth_images[i], VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT));

		m_window_states.swapchain_framebuffers[i] = Framebuffer(m_vulkan_context);
		m_window_states.swapchain_framebuffers[i].add_attachment(m_window->get_swapchain_image_view(i));
		m_window_states.swapchain_framebuffers[i].add_attachment(m_window_states.depth_image_views[i]);
		m_window_states.swapchain_framebuffers[i].create(m_quadtree.get_render_pass().get_render_pass(), m_window->get_size().x, m_window->get_size().y);

		m_imgui_vulkan_state.swapchain_framebuffers[i] = Framebuffer(m_vulkan_context);
		m_imgui_vulkan_state.swapchain_framebuffers[i].add_attachment(m_window->get_swapchain_image_view(i));
		m_imgui_vulkan_state.swapchain_framebuffers[i].create(m_imgui_vulkan_state.render_pass, m_window->get_size().x, m_window->get_size().y);
	}

	m_imgui_vulkan_state.done_drawing_semaphores.resize(m_window->get_swapchain_size());
	for (uint32_t i = 0; i < m_window->get_swapchain_size(); i++)
	{
		VkSemaphoreCreateInfo create_info;
		create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		create_info.pNext = nullptr;
		create_info.flags = 0;
		VK_CHECK(vkCreateSemaphore(m_vulkan_context.get_device(), &create_info, m_vulkan_context.get_allocation_callbacks(), 
			&m_imgui_vulkan_state.done_drawing_semaphores[i]), "Semaphore creation failed!")
	}

	// Set up debug drawing
	m_debug_pipeline_layout = PipelineLayout(m_vulkan_context);
	{
		// Set up push constant range for frame data
		VkPushConstantRange push_range;
		push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		push_range.offset = 0;
		push_range.size = sizeof(DebugDrawingFrameData);

		m_debug_pipeline_layout.create(&push_range);
	}

	m_debug_render_pass = RenderPass(
		m_vulkan_context, 
		VK_FORMAT_B8G8R8A8_UNORM, 
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		false, true, false, 
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	m_debug_drawer = DebugDrawer(m_vulkan_context, 11000);

	create_pipelines();

#ifdef RAY_MARCH_WINDOW
	// Start ray march thread
	m_ray_march_thread = std::thread(&Application::draw_ray_march, this);
#endif
}

Application::~Application()
{
	imgui_shutdown();

	for (uint32_t i = 0; i < m_window->get_swapchain_size(); i++)
	{
		vkDestroySemaphore(m_vulkan_context.get_device(), m_imgui_vulkan_state.done_drawing_semaphores[i], 
			m_vulkan_context.get_allocation_callbacks());
	}

	delete m_debug_camera;
	delete m_main_camera;

#ifdef RAY_MARCH_WINDOW
	delete m_ray_march_window;
#endif

	delete m_window;

	glfwTerminate();
}

void Application::run()
{
	bool right_mouse_clicked = false;
	bool f_pressed = false;
	bool demo_window = true;
	bool camera_switch_pressed = false;
	bool f5_pressed = false;
	bool q_pressed = false;

	while (!glfwWindowShouldClose(m_window->get_glfw_window()))
	{
		auto stop_time = m_timer;
		m_timer = std::chrono::high_resolution_clock::now();
		std::chrono::duration<float> delta_time = m_timer - stop_time;

		glfwPollEvents();

		// Toggle camera controls
		if (!right_mouse_clicked && glfwGetMouseButton(m_window->get_glfw_window(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS
			&& !ImGui::GetIO().WantCaptureMouse)
		{
			m_window->set_mouse_locked(!m_window->get_mouse_locked());
			right_mouse_clicked = true;
		}
		else if (m_window && glfwGetMouseButton(m_window->get_glfw_window(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_RELEASE
			&& !ImGui::GetIO().WantCaptureMouse)
			right_mouse_clicked = false;

		// Switch camera
		if (!camera_switch_pressed && glfwGetKey(m_window->get_glfw_window(), GLFW_KEY_F1) == GLFW_PRESS
			&& !ImGui::GetIO().WantCaptureMouse)
		{
			if (m_current_camera == m_main_camera)
				m_current_camera = m_debug_camera;
			else
				m_current_camera = m_main_camera;
			camera_switch_pressed = true;
		}
		else if (m_window && glfwGetKey(m_window->get_glfw_window(), GLFW_KEY_F1) == GLFW_RELEASE
			&& !ImGui::GetIO().WantCaptureMouse)
			camera_switch_pressed = false;

		// Toggle imgui
		if (!f_pressed && glfwGetKey(m_window->get_glfw_window(), GLFW_KEY_F) == GLFW_PRESS)
		{
			f_pressed = true;
			m_show_imgui = !m_show_imgui;
		}
		else if (f_pressed && glfwGetKey(m_window->get_glfw_window(), GLFW_KEY_F) == GLFW_RELEASE)
			f_pressed = false;

		// Reload shaders
		if (!f5_pressed && glfwGetKey(m_window->get_glfw_window(), GLFW_KEY_F5) == GLFW_PRESS)
		{
			f5_pressed = true;
			m_tfile.compile_shaders();
			create_pipelines();
		}
		else if (f5_pressed && glfwGetKey(m_window->get_glfw_window(), GLFW_KEY_F5) == GLFW_RELEASE)
			f5_pressed = false;

		// Clear terrain
		if (!q_pressed && glfwGetKey(m_window->get_glfw_window(), GLFW_KEY_Q) == GLFW_PRESS)
		{
			q_pressed = true;
			m_quadtree.clear_terrain();
		}
		else if (q_pressed && glfwGetKey(m_window->get_glfw_window(), GLFW_KEY_Q) == GLFW_RELEASE)
			q_pressed = false;

		// Refinement button
		m_triangulate_button_held = glfwGetKey(m_window->get_glfw_window(), GLFW_KEY_R) == GLFW_PRESS;

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame(!m_window->get_mouse_locked() && m_show_imgui);
		ImGui_ImplGlfw_SetHandleCallbacks(!m_window->get_mouse_locked() && m_show_imgui);
		ImGui::NewFrame();

		// Reset debug drawer
		m_debug_drawer.new_frame();

		/////////////////////////////
		/////////////////////////////
		/////////////////////////////
		/////////////////////////////
		cpu_triangulate_test(m_main_camera->get_vp(), m_debug_drawer, *m_main_camera);
		/////////////////////////////
		/////////////////////////////
		/////////////////////////////
		/////////////////////////////
			   		 	  	  
		update(delta_time.count());

		draw();
	}

	m_quit = true; 
#ifdef RAY_MARCH_WINDOW
	m_cv.notify_all();
	m_ray_march_thread.join();
#endif
}


void Application::update(const float dt)
{
	m_current_camera->update(dt, m_window->get_mouse_locked(), m_debug_drawer);

#ifdef RAY_MARCH_WINDOW
	m_ray_march_frame_data.view = m_current_camera->get_ray_march_view();
	m_ray_march_frame_data.screen_size = m_ray_march_window->get_size();
	m_ray_march_frame_data.position = glm::vec4(m_current_camera->get_pos(), 0);
#endif

	m_debug_draw_frame_data.vp = m_current_camera->get_vp();

	if (m_show_imgui)
	{
		ImGui::Begin("Info");

		static const uint32_t num_frames = 200;
		static float values[num_frames] = { 0 };
		static int values_offset = 0;
		values[values_offset] = dt;
		values_offset = (values_offset + 1) % num_frames;
		ImGui::PlotLines("Frame Time", values, num_frames, values_offset, nullptr, 0.0f, 0.02f, ImVec2(150, 30));
		

		std::string text = "Frame info: " + std::to_string(int(1.f / dt)) + "fps  "
			+ std::to_string(dt) + "s  " + std::to_string(int(100.f * dt / 0.016f)) + "%%";
		ImGui::Text(text.c_str());
		text = "Position: " + std::to_string(m_main_camera->get_pos().x) + ", " + std::to_string(m_main_camera->get_pos().y) + ", " + std::to_string(m_main_camera->get_pos().z);
		ImGui::Text(text.c_str());
		text = "Debug Position: " + std::to_string(m_debug_camera->get_pos().x) + ", " + std::to_string(m_debug_camera->get_pos().y) + ", " + std::to_string(m_debug_camera->get_pos().z);
		ImGui::Text(text.c_str());
		ImGui::Checkbox("Draw Ray Marched View", &m_draw_ray_march);
		ImGui::Checkbox("Wireframe", &m_draw_wireframe);
		ImGui::Checkbox("Refine", &m_triangulate);
		ImGui::DragFloat("Area Multiplier", &m_em_area_multiplier, 0.01f, 0.0f, 3.0f);
		ImGui::DragFloat("Curvature Multiplier", &m_em_curvature_multiplier, 0.01f, 0.0f, 3.0f);
		ImGui::DragFloat("Threshold", &m_em_threshold, 0.01f, 0.1f, 10.0f);
		if (ImGui::Button("Clear Terrain"))
			m_quadtree.clear_terrain();
		ImGui::End();
	}
}

void Application::draw()
{
#ifdef RAY_MARCH_WINDOW
	// Start ray march thread
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_ray_march_new_frame = true;
	}
	m_cv.notify_all();
#endif

	draw_main();

#ifdef RAY_MARCH_WINDOW
	// Wait for ray march thread
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_cv.wait(lock, [this] { return m_ray_march_done; });
		m_ray_march_done = false;
	}
#endif
}

void Application::draw_main()
{
	const uint32_t index = m_window->get_next_image();
	VkImage image = m_window->get_swapchain_image(index);


	// RENDER-------------------

	m_main_queue.start_recording();

	// Transfer images to layouts for rendering targets
	m_main_queue.cmd_image_barrier(
		image,
		VK_ACCESS_MEMORY_READ_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

	m_main_queue.cmd_image_barrier(
		m_window_states.depth_images[index].get_image(),
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);


	// Perform terrain generation/drawing
	Frustum fr = m_main_camera->get_frustum();
	m_quadtree.intersect(m_main_queue, fr, m_debug_drawer);
	m_quadtree.draw_terrain(m_main_queue, fr, m_debug_drawer, m_window_states.swapchain_framebuffers[index], *m_current_camera, m_draw_wireframe);

	m_main_queue.cmd_image_barrier(
		image,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	m_main_queue.cmd_image_barrier(
		m_window_states.depth_images[index].get_image(),
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

	ImGui::Begin("Triangulate");

	// Fritjof stuff
	if (ImGui::Button("Set") || m_triangulate || m_triangulate_button_held)
	{
		m_quadtree.process_triangles(m_main_queue, *m_main_camera, *m_window, m_em_threshold, m_em_area_multiplier, m_em_curvature_multiplier);

		m_quadtree.triangulate(m_main_queue, m_main_camera->get_pos());

		m_main_queue.cmd_buffer_barrier(m_quadtree.get_buffer().get_buffer(),
			VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
			VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);
	}
	ImGui::End();

	// Do debug drawing
	{
		m_debug_drawer.draw_line({ 0, 0, 0 }, { 1, 0, 0 }, { 1, 0, 0 });
		m_debug_drawer.draw_line({ 0, 0, 0 }, { 0, 1, 0 }, { 0, 1, 0 });
		m_debug_drawer.draw_line({ 0, 0, 0 }, { 0, 0, 1 }, { 0, 0, 1 });

		if (m_current_camera != m_main_camera)
		{
			// Draw frustum
			m_debug_drawer.draw_frustum(m_main_camera->get_vp(), {1, 0, 1});

			//glm::mat4 inv_vp = glm::inverse(m_main_camera->get_vp());
			//glm::vec4 left_pos = inv_vp * glm::vec4(-1, 0, 0.99f, 1); left_pos /= left_pos.w;
			//glm::vec4 right_pos = inv_vp * glm::vec4(1, 0, 0.99f, 1); right_pos /= right_pos.w;
			//glm::vec4 top_pos = inv_vp * glm::vec4(0, -1, 0.99f, 1); top_pos /= top_pos.w;
			//glm::vec4 bottom_pos = inv_vp * glm::vec4(0, 1, 0.99f, 1); bottom_pos /= bottom_pos.w;
			//glm::vec4 near_pos = inv_vp * glm::vec4(0, 0, 0, 1); near_pos /= near_pos.w;
			//glm::vec4 far_pos = inv_vp * glm::vec4(0, 0, 1, 1); far_pos /= far_pos.w;

			//Frustum frustum = m_main_camera->get_frustum();
			//m_debug_drawer.draw_plane(frustum.m_left, left_pos, 1.0f, { 1,1,1 }, { 1,0,0 });
			//m_debug_drawer.draw_plane(frustum.m_right, right_pos, 1.0f, { 1,1,1 }, { 1,0,0 });
			//m_debug_drawer.draw_plane(frustum.m_top, top_pos, 1.0f, { 1,1,1 }, { 1,0,0 });
			//m_debug_drawer.draw_plane(frustum.m_bottom, bottom_pos, 1.0f, { 1,1,1 }, { 1,0,0 });
			//m_debug_drawer.draw_plane(frustum.m_near, near_pos, 1.0f, { 1,1,1 }, { 1,0,0 });
			//m_debug_drawer.draw_plane(frustum.m_far, far_pos, 1.0f, { 1,1,1 }, { 1,0,0 });
		}

		// Copy lines specified on CPU to GPU buffer
		m_main_queue.cmd_copy_buffer(m_debug_drawer.get_cpu_buffer().get_buffer(),
			m_debug_drawer.get_gpu_buffer().get_buffer(),
			m_debug_drawer.get_active_buffer_size());
	
		// Memory barrier for GPU buffer
		m_main_queue.cmd_buffer_barrier(m_debug_drawer.get_gpu_buffer().get_buffer(),
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);

		m_main_queue.cmd_begin_render_pass(m_debug_render_pass, m_window_states.swapchain_framebuffers[index]);

		m_main_queue.cmd_bind_graphics_pipeline(m_debug_pipeline->m_pipeline);
		m_main_queue.cmd_bind_vertex_buffer(m_debug_drawer.get_gpu_buffer().get_buffer(), 0);
		m_main_queue.cmd_push_constants(m_debug_pipeline_layout.get_pipeline_layout(), VK_SHADER_STAGE_VERTEX_BIT, sizeof(DebugDrawingFrameData), &m_debug_draw_frame_data);
		m_main_queue.cmd_draw(m_debug_drawer.get_num_lines() * 2);

		m_main_queue.cmd_end_render_pass();

		m_main_queue.cmd_image_barrier(
			image,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	}


	m_main_queue.end_recording();
	m_main_queue.submit();
	m_main_queue.wait();

	imgui_draw(m_imgui_vulkan_state.swapchain_framebuffers[index], m_imgui_vulkan_state.done_drawing_semaphores[index]);

	{
		std::scoped_lock lock(m_present_lock);
		present(m_window, m_main_queue.get_queue(), index, m_imgui_vulkan_state.done_drawing_semaphores[index]);
	}
}

void Application::draw_ray_march()
{
	while (!m_quit)
	{
		// Wait until main thread signals new frame
		std::unique_lock<std::mutex> lock(m_mutex);
		m_cv.wait(lock, [this] { return m_ray_march_new_frame || m_quit; });
		m_ray_march_new_frame = false;

		if (m_draw_ray_march)
		{
			const uint32_t index = m_ray_march_window->get_next_image();
			VkImage image = m_ray_march_window->get_swapchain_image(index);

			m_ray_march_image_descriptor_set.clear();
			m_ray_march_image_descriptor_set.add_storage_image(m_ray_march_window->get_swapchain_image_view(index), VK_IMAGE_LAYOUT_GENERAL);
			m_ray_march_image_descriptor_set.bind();

			m_ray_march_compute_queue.start_recording();

			// RENDER-------------------
			// Bind pipeline
			m_ray_march_compute_queue.cmd_bind_compute_pipeline(m_ray_march_compute_pipeline->m_pipeline);

			// Bind descriptor set
			m_ray_march_compute_queue.cmd_bind_descriptor_set_compute(m_ray_march_compute_pipeline->m_pipeline_layout.get_pipeline_layout(), 0, m_ray_march_image_descriptor_set.get_descriptor_set());

			// Transfer image to shader write layout
			m_ray_march_compute_queue.cmd_image_barrier(image,
				VK_ACCESS_MEMORY_READ_BIT,
				VK_ACCESS_SHADER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_GENERAL,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

			// Push frame data
			m_ray_march_compute_queue.cmd_push_constants(m_ray_march_pipeline_layout.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, sizeof(RayMarchFrameData), &m_ray_march_frame_data);

			// Dispatch
			const uint32_t group_size = 32;
			m_ray_march_compute_queue.cmd_dispatch(m_ray_march_window->get_size().x / group_size + 1, m_ray_march_window->get_size().y / group_size + 1, 1);

			// end of RENDER------------------

			m_ray_march_compute_queue.cmd_image_barrier(
				image,
				VK_ACCESS_SHADER_WRITE_BIT,
				VK_ACCESS_MEMORY_READ_BIT,
				VK_IMAGE_LAYOUT_GENERAL,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

			m_ray_march_compute_queue.end_recording();
			m_ray_march_compute_queue.submit();
			m_ray_march_compute_queue.wait();

			{
				std::scoped_lock lock(m_present_lock);
				present(m_ray_march_window, m_ray_march_compute_queue.get_queue(), index, VK_NULL_HANDLE);
			}
		}

		m_ray_march_done = true;
		lock.unlock();
		m_cv.notify_all();
	}
}

void Application::present(Window* window, VkQueue queue, const uint32_t index, VkSemaphore wait_for) const
{
	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = nullptr;
	if (wait_for == VK_NULL_HANDLE)
	{
		present_info.waitSemaphoreCount = 0;
		present_info.pWaitSemaphores = nullptr;
	}
	else
	{
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = &wait_for;
	}
	present_info.swapchainCount = 1;
	present_info.pSwapchains = window->get_swapchain();
	present_info.pImageIndices = &index;
	VkResult result;
	present_info.pResults = &result;

	if ((result = vkQueuePresentKHR(queue, &present_info)) != VK_SUCCESS)
	{
#ifdef _DEBUG
		__debugbreak();
#else
		println("Failed to present image");
		exit(1);
#endif
	}
}

void Application::imgui_setup()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(m_window->get_glfw_window(), true);

	m_imgui_vulkan_state.queue = m_vulkan_context.create_graphics_queue();

	// Create the Render Pass
    {
        VkAttachmentDescription attachment = {};
        attachment.format = m_window->get_format();
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkAttachmentReference color_attachment = {};
        color_attachment.attachment = 0;
        color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment;
        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 1;
        info.pAttachments = &attachment;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dependency;
        VK_CHECK(vkCreateRenderPass(m_vulkan_context.get_device(), &info, 
				m_vulkan_context.get_allocation_callbacks(), 
				&m_imgui_vulkan_state.render_pass), 
			"imgui setup failed to create render pass!");
    }

	ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_vulkan_context.get_instance();
    init_info.PhysicalDevice = m_vulkan_context.get_physical_device();
    init_info.Device = m_vulkan_context.get_device();
    init_info.QueueFamily = m_vulkan_context.get_graphics_queue_index();
    init_info.Queue = m_imgui_vulkan_state.queue.get_queue();
    init_info.DescriptorPool = m_vulkan_context.get_descriptor_pool();
    init_info.Allocator = m_vulkan_context.get_allocation_callbacks();
    ImGui_ImplVulkan_Init(&init_info, m_imgui_vulkan_state.render_pass);

	 // Upload Fonts
    {
		// Create command pool
		VkCommandPoolCreateInfo command_pool_info;
		command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		command_pool_info.pNext = nullptr;
		command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		command_pool_info.queueFamilyIndex = m_vulkan_context.get_graphics_queue_index();

		VkResult result = vkCreateCommandPool(m_vulkan_context.get_device(), 
			&command_pool_info, m_vulkan_context.get_allocation_callbacks(), 
			&m_imgui_vulkan_state.command_pool);
		assert(result == VK_SUCCESS);

		// Create command buffer
		VkCommandBufferAllocateInfo alloc_info;
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.pNext = nullptr;
		alloc_info.commandPool = m_imgui_vulkan_state.command_pool;
		alloc_info.level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;

		result = vkAllocateCommandBuffers(m_vulkan_context.get_device(), &alloc_info, &m_imgui_vulkan_state.command_buffer);
		assert(result == VK_SUCCESS);

        VK_CHECK(vkResetCommandPool(init_info.Device, m_imgui_vulkan_state.command_pool, 0), "imgui setup failed to reset command pool!");

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(m_imgui_vulkan_state.command_buffer, &begin_info), "imgui setup failed to reset command pool!");

        ImGui_ImplVulkan_CreateFontsTexture(m_imgui_vulkan_state.command_buffer);

        VkSubmitInfo end_info = {};
        end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        end_info.commandBufferCount = 1;
        end_info.pCommandBuffers = &m_imgui_vulkan_state.command_buffer;
        VK_CHECK(vkEndCommandBuffer(m_imgui_vulkan_state.command_buffer), "imgui setup failed to end command buffer!");
        VK_CHECK(vkQueueSubmit(init_info.Queue, 1, &end_info, VK_NULL_HANDLE), "imgui setup failed to submit command buffer!");

        VK_CHECK(vkDeviceWaitIdle(init_info.Device), "imgui setup failed when waiting for device!");
        
        ImGui_ImplVulkan_InvalidateFontUploadObjects();
    }

	VkFenceCreateInfo fence_info;
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.pNext = nullptr;
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	VK_CHECK(vkCreateFence(m_vulkan_context.get_device(), &fence_info, m_vulkan_context.get_allocation_callbacks(), 
		&m_imgui_vulkan_state.command_buffer_idle), "Fence creation failed!");
}

void Application::imgui_shutdown()
{
	vkDeviceWaitIdle(m_vulkan_context.get_device());
	vkDestroyFence(m_vulkan_context.get_device(), m_imgui_vulkan_state.command_buffer_idle,
		m_vulkan_context.get_allocation_callbacks());

	vkDestroyRenderPass(m_vulkan_context.get_device(), m_imgui_vulkan_state.render_pass, 
				m_vulkan_context.get_allocation_callbacks());

	vkFreeCommandBuffers(m_vulkan_context.get_device(), m_imgui_vulkan_state.command_pool, 1, &m_imgui_vulkan_state.command_buffer);

	vkDestroyCommandPool(m_vulkan_context.get_device(), m_imgui_vulkan_state.command_pool, 
				m_vulkan_context.get_allocation_callbacks());


	ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void Application::imgui_draw(Framebuffer& framebuffer, VkSemaphore imgui_draw_complete_semaphore)
{
	ImGui::Render();

	{
		vkWaitForFences(m_vulkan_context.get_device(), 1, &m_imgui_vulkan_state.command_buffer_idle, VK_FALSE, ~0ull);
		vkResetFences(m_vulkan_context.get_device(), 1, &m_imgui_vulkan_state.command_buffer_idle);
		VK_CHECK(vkResetCommandPool(m_vulkan_context.get_device(), m_imgui_vulkan_state.command_pool, 0), "imgui failed to reset command pool!");
		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VK_CHECK(vkBeginCommandBuffer(m_imgui_vulkan_state.command_buffer, &info), "imgui failed to begin command buffer!");
	}
	{
		VkRenderPassBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass = m_imgui_vulkan_state.render_pass;
		info.framebuffer = framebuffer.get_framebuffer();
		info.renderArea.extent.width = m_window->get_size().x;
		info.renderArea.extent.height = m_window->get_size().y;
		info.clearValueCount = 1;
		VkClearValue clear_value;
		clear_value.color.float32[0] = 0.0f;
		clear_value.color.float32[1] = 0.0f;
		clear_value.color.float32[2] = 0.0f;
		clear_value.color.float32[3] = 0.0f;
		clear_value.depthStencil.depth = 0.0f;
		clear_value.depthStencil.stencil = 0;
		info.pClearValues = &clear_value;
		vkCmdBeginRenderPass(m_imgui_vulkan_state.command_buffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	}

	// Record Imgui Draw Data and draw funcs into command buffer
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_imgui_vulkan_state.command_buffer);

	// Submit command buffer
	vkCmdEndRenderPass(m_imgui_vulkan_state.command_buffer);
	{
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.waitSemaphoreCount = 0;
		info.pWaitSemaphores = nullptr;
		info.pWaitDstStageMask = &wait_stage;
		info.commandBufferCount = 1;
		info.pCommandBuffers = &m_imgui_vulkan_state.command_buffer;
		info.signalSemaphoreCount = 1;
		info.pSignalSemaphores = &imgui_draw_complete_semaphore;

		VK_CHECK(vkEndCommandBuffer(m_imgui_vulkan_state.command_buffer), "imgui ending command buffer failed!");
		VK_CHECK(vkQueueSubmit(m_imgui_vulkan_state.queue.get_queue(), 1, &info, m_imgui_vulkan_state.command_buffer_idle), "imgui submitting queue failed!");
	}
}

void Application::create_pipelines()
{
#ifdef RAY_MARCH_WINDOW
	m_ray_march_compute_pipeline = m_vulkan_context.create_compute_pipeline("terrain", m_ray_march_pipeline_layout, nullptr);
#endif

	VertexAttributes debug_attributes;
	debug_attributes.add_buffer();
	debug_attributes.add_attribute(3);
	debug_attributes.add_attribute(3);
	m_debug_pipeline = m_vulkan_context.create_graphics_pipeline("debug", m_window->get_size(), m_debug_pipeline_layout, debug_attributes, m_debug_render_pass, true, false, nullptr, nullptr, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);

	m_quadtree.create_pipelines(*m_window);
}
