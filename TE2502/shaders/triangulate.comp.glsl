#version 450 core

#define WORK_GROUP_SIZE 32

layout(local_size_x = WORK_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

const uint num_indices = TERRAIN_GENERATE_NUM_INDICES;
const uint num_vertices = TERRAIN_GENERATE_NUM_VERTICES;
const uint num_nodes = TERRAIN_GENERATE_NUM_NODES;
const uint num_new_points = TRIANGULATE_MAX_NEW_POINTS;
const uint quadtree_levels = QUADTREE_LEVELS;

layout(push_constant) uniform frame_data_t
{
	uint node_index;
} frame_data;

struct Triangle
{
	vec2 circumcentre;
	float circumradius;
	uint pad;
};

struct terrain_data_t
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
	// }

	uint indices[num_indices];
	vec4 positions[num_vertices];
	Triangle triangles[num_indices / 3];
	vec4 new_points[num_new_points];
};

const uint num_quadtree_nodes = (1 << quadtree_levels) * (1 << quadtree_levels);
const uint aligned_quadtree_index_num = (num_quadtree_nodes + 4) + (16 - ((num_quadtree_nodes + 4) % 16));

layout(set = 0, binding = 0) buffer terrain_buffer_t
{
	uint quadtree_index_map[aligned_quadtree_index_num - 4];
	vec2 quadtree_min;
	vec2 quadtree_max;
	terrain_data_t data[num_nodes];
} terrain_buffer;



///// CIRCUMCIRCLE

// Function to find the line given two points
void line_from_points(vec2 p1, vec2 p2, out float a, out float b, out float c)
{
	a = p2.y - p1.y;
	b = p1.x - p2.x;
	c = a * p1.x + b * p2.y;
}

// Function which converts the input line to its 
// perpendicular bisector. It also inputs the points 
// whose mid-point lies on the bisector 
void perpendicular_bisector_from_line(vec2 p1, vec2 p2, inout float a, inout float b, inout float c)
{
	vec2 mid_point = vec2((p1.x + p2.x) * 0.5, (p1.y + p2.y) * 0.5);

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
	return (a * b * c) / sqrt((a + b + c) * (b + c - a) * (c + a - b) * (a + b - c));
}

///////////////////


struct Edge
{
	uint p1;
	uint p2;
};

shared uint s_edge_count;
shared Edge s_edges[200 * 3];

#define INDICES_TO_STORE 150
#define TRIANGLES_TO_STORE 50

shared uint s_triangles_to_remove[TRIANGLES_TO_STORE];

//shared uint s_last_indices[INDICES_TO_STORE];
//shared vec2 s_last_circumcentres[TRIANGLES_TO_STORE];
//shared float s_last_circumradii[TRIANGLES_TO_STORE];
shared uint s_triangles_removed;

shared uint s_index_count;
shared uint s_triangle_count;
shared uint s_vertex_count;

#define INVALID 999999

void main(void)
{
	const uint node_index = frame_data.node_index;

	if (terrain_buffer.data[node_index].new_points_count == 0)
		return;

	const uint thid = gl_GlobalInvocationID.x;

	// Set shared variables
	if (thid == 0)
	{
		s_edge_count = 0;
	  s_index_count = terrain_buffer.data[node_index].index_count;
		s_triangle_count = s_index_count / 3;
		s_triangles_removed = 0;
		s_vertex_count = terrain_buffer.data[node_index].vertex_count;
	}
	barrier();
	memoryBarrierShared();

	const uint new_points_count = terrain_buffer.data[node_index].new_points_count;
	for (uint n = 0; n < new_points_count && s_index_count + 60 < num_indices; ++n)
	{
		vec4 current_point = terrain_buffer.data[node_index].new_points[n];


		// Load last few indices from buffer to shared memory
		uint i = thid;
		//while (i < INDICES_TO_STORE)  // TODO: Replace with if-statement instead of while-loop
		//{
		//	s_last_indices[i] = terrain_buffer.data[node_index].indices[s_index_count - INDICES_TO_STORE + i];
		//	i += WORK_GROUP_SIZE;
		//}

		//// Load last few circumcircles from buffer to shared memory
		//i = thid;
		//while (i < TRIANGLES_TO_STORE)  // TODO: Replace with if-statement instead of while-loop
		//{
		//	s_last_circumcentres[i] = terrain_buffer.data[node_index].triangles[s_triangle_count - TRIANGLES_TO_STORE + i].circumcentre;
		//	s_last_circumradii[i] = terrain_buffer.data[node_index].triangles[s_triangle_count - TRIANGLES_TO_STORE + i].circumradius;
		//	i += WORK_GROUP_SIZE;
		//}

		//barrier();
		//memoryBarrierShared();

		// Check distance from circumcircles to new point
		i = thid;
		while (i < s_triangle_count)
		{
			vec2 circumcentre = terrain_buffer.data[node_index].triangles[i].circumcentre;
			float circumradius = terrain_buffer.data[node_index].triangles[i].circumradius;

			float dx = current_point.x - circumcentre.x;
			float dy = current_point.z - circumcentre.y;
			if (sqrt(dx * dx + dy * dy) < circumradius)
			{
				// Add triangle edges to edge buffer
				uint tr = atomicAdd(s_triangles_removed, 1);
				uint ec = tr * 3; //atomicAdd(s_edge_count, 3);
				uint index_offset = i * 3;
				uint index0 = terrain_buffer.data[node_index].indices[index_offset + 0];
				uint index1 = terrain_buffer.data[node_index].indices[index_offset + 1];
				uint index2 = terrain_buffer.data[node_index].indices[index_offset + 2];
				// Edge 1
				s_edges[ec + 0].p1 = min(index0, index1);
				s_edges[ec + 0].p2 = max(index0, index1);
				// Edge 2
				s_edges[ec + 1].p1 = min(index1, index2);
				s_edges[ec + 1].p2 = max(index1, index2);
				// Edge 3
				s_edges[ec + 2].p1 = min(index2, index0);
				s_edges[ec + 2].p2 = max(index2, index0);

				// Mark the triangle to be removed later
				s_triangles_to_remove[tr] = i;
			}

			i += WORK_GROUP_SIZE;
		}
		s_edge_count = s_triangles_removed * 3;
		barrier();
		memoryBarrierShared();
		memoryBarrierBuffer();

		if (thid == 0)
		{
			// Delete all doubly specified edges from edge buffer (this leaves the edges of the enclosing polygon only)
			for (uint i = 0; i < s_edge_count; ++i)  // TODO: Optimize by splitting out to multiple threads
			{
				bool found = false;
				for (uint j = 0; j < s_edge_count; ++j)
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
			}

			uint old_index_count = s_index_count;
			uint old_triangle_count = s_triangle_count;
			bool all_valid = true;

			// Add to the triangle list all triangles formed between the point and the edges of the enclosing polygon
			for (uint i = 0; i < s_edge_count; ++i)
			{
				if (s_edges[i].p1 != INVALID)
				{
					vec3 P = terrain_buffer.data[node_index].positions[s_edges[i].p1].xyz;
					vec3 Q = terrain_buffer.data[node_index].positions[s_edges[i].p2].xyz;
					vec3 R = current_point.xyz;

					// Make sure winding order is correct
					vec3 n = cross(R - P, Q - P);
					if (n.y > 0)
					{
						uint temp = s_edges[i].p1;
						s_edges[i].p1 = s_edges[i].p2;
						s_edges[i].p2 = temp;
					}

					// Set indices for the new triangle
					terrain_buffer.data[node_index].indices[s_index_count + 0] = s_edges[i].p1;
					terrain_buffer.data[node_index].indices[s_index_count + 1] = s_edges[i].p2;
					terrain_buffer.data[node_index].indices[s_index_count + 2] = s_vertex_count;
					// Set circumcircles for the new triangle
					float a = distance(P.xz, Q.xz);
					float b = distance(P.xz, R.xz);
					float c = distance(R.xz, Q.xz);

					vec2 PQ = normalize(Q.xz - P.xz);
					vec2 PR = normalize(R.xz - P.xz);
					vec2 RQ = normalize(Q.xz - R.xz);
					float d1 = abs(dot(PQ, PR));
					float d2 = abs(dot(PR, RQ));
					float d3 = abs(dot(RQ, PQ));

					const float epsilon = 1 - 0.0001;
					if (d1 > epsilon || d2 > epsilon || d3 > epsilon)
					{
						all_valid = false;
						break;
					}
					terrain_buffer.data[node_index].triangles[s_triangle_count].circumcentre = find_circum_center(P.xz, Q.xz, R.xz);
					terrain_buffer.data[node_index].triangles[s_triangle_count].circumradius = find_circum_radius_squared(a, b, c);

					s_index_count += 3;
					++s_triangle_count;
				}
			}
			if (all_valid)
			{
				// Remove triangles from triangle list
				//uint last_valid_triangle = 0;
				//for (int j = int(s_triangles_removed) - 1; j >= 0; --j)
				//{
				//	uint index = s_triangles_to_remove[j];
				//	if (index < s_triangle_count - last_valid_triangle - 1)
				//	{
				//		terrain_buffer.data[node_index].indices[index * 3 + 0] = s_last_indices[last_valid_triangle * 3 + 0];
				//		terrain_buffer.data[node_index].indices[index * 3 + 1] = s_last_indices[last_valid_triangle * 3 + 1];
				//		terrain_buffer.data[node_index].indices[index * 3 + 2] = s_last_indices[last_valid_triangle * 3 + 2];
				//		terrain_buffer.data[node_index].triangles[index].circumcentre = s_last_circumcentres[last_valid_triangle];
				//		terrain_buffer.data[node_index].triangles[index].circumradius = s_last_circumradii[last_valid_triangle];
				//	}
				//	++last_valid_triangle;
				//}

				uint last_valid_triangle = s_triangle_count - 1;
				for (int j = int(s_triangles_removed) - 1; j >= 0; --j)
				{
					uint index = s_triangles_to_remove[j];
					if (index < last_valid_triangle)
					{
						terrain_buffer.data[node_index].indices[index * 3 + 0] = terrain_buffer.data[node_index].indices[last_valid_triangle * 3 + 0];
						terrain_buffer.data[node_index].indices[index * 3 + 1] = terrain_buffer.data[node_index].indices[last_valid_triangle * 3 + 1];
						terrain_buffer.data[node_index].indices[index * 3 + 2] = terrain_buffer.data[node_index].indices[last_valid_triangle * 3 + 2];
						terrain_buffer.data[node_index].triangles[index].circumcentre = terrain_buffer.data[node_index].triangles[last_valid_triangle].circumcentre;
						terrain_buffer.data[node_index].triangles[index].circumradius = terrain_buffer.data[node_index].triangles[last_valid_triangle].circumradius;
					}
					--last_valid_triangle;
				}
				s_triangle_count -= s_triangles_removed;
				s_index_count = s_triangle_count * 3;

				// Insert new point
				terrain_buffer.data[node_index].positions[s_vertex_count] = current_point;
				++s_vertex_count;
			}
			else
			{
				s_index_count = old_index_count;
				s_triangle_count = old_triangle_count;
			}

			// Reset found edge count
			s_edge_count = 0;

			s_triangles_removed = 0;
		}	
		
		barrier();
		memoryBarrierShared();
		memoryBarrierBuffer();
	}

	// Write new buffer lenghts to buffer
	if (thid == 0)
	{
		terrain_buffer.data[node_index].vertex_count = s_vertex_count;
		terrain_buffer.data[node_index].index_count = s_index_count;

		terrain_buffer.data[node_index].new_points_count = 0;
	}
}