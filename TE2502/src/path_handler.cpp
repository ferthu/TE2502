#include "path_handler.hpp"
#include <fstream>

#include "utilities.hpp"
#include "camera.hpp"

PathHandler::PathHandler(const std::string& file_path)
{
	m_file_path = file_path;
	std::ifstream in(file_path);
	if (in.is_open())
	{
		int path_count;
		in >> path_count;
		m_paths.reserve(path_count);
		// For each camera path
		for (int pp = 0; pp < path_count; ++pp)
		{
			// Read path name
			std::string name;
			in >> name;
			m_path_names.push_back(name);

			// Read path positions
			Path path;
			int pos_count;
			in >> pos_count;
			path.reserve(pos_count);
			for (int cc = 0; cc < pos_count; ++cc)
			{
				PathPart part;
				in >> part.pos.x >> part.pos.y >> part.pos.z >> part.yaw >> part.pitch;
				path.push_back(part);
			}
			m_paths.push_back(path);
		}
		in.close();
	}
	else
	{
		println("Could not open file for loading: " + file_path);
	}
}


PathHandler::~PathHandler()
{
	// Nothing
}

void PathHandler::save_to_file() const
{
	std::ofstream out(m_file_path);
	if (out.is_open())
	{
		out << m_paths.size() << "\n\n";
		for (int pp = 0; pp < m_paths.size(); ++pp)
		{
			out << m_path_names[pp] << "\n" << m_paths[pp].size() << "\n";
			
			for (auto& part : m_paths[pp])
			{
				out << part.pos.x << "\t"
					<< part.pos.y << "\t"
					<< part.pos.z << "\t\t"
					<< part.yaw << "\t"
					<< part.pitch << "\n";
			}

			out << std::endl;
		}
	}
	else
	{
		println("Could not open file for saving: " + m_file_path);
	}
}

void PathHandler::attach_camera(Camera* cam)
{
	m_camera = cam;
}

bool PathHandler::path_name_exists(const std::string & name) const
{
	for (auto& n : m_path_names)
	{
		if (name == n)
			return true;
	}
	return false;
}

void PathHandler::start_new_path()
{
	m_creating_path = true;
	std::string name;
	do
	{
		name = "path_" + std::to_string(rand() % 10000);
	} while (path_name_exists(name));
	m_path_names.push_back(name);
	m_paths.push_back(Path());
	save_path_part();
}

void PathHandler::save_path_part()
{
	if (m_creating_path)
	{
		PathPart part;
		part.pos = m_camera->get_pos();
		part.yaw = m_camera->get_yaw();
		part.pitch = m_camera->get_pitch();
		m_paths.back().push_back(part);
	}
}

void PathHandler::finish_new_path()
{
	save_path_part();
	save_to_file();
	m_creating_path = false;
}

const std::vector<std::string>& PathHandler::get_path_names() const
{
	return m_path_names;
}

void PathHandler::follow_path(const std::string & path_name)
{
	for (int ii = 0; ii < m_path_names.size(); ++ii)
	{
		if (m_path_names[ii] == path_name)
		{
			m_following_path = true;
			m_path_index = ii;
			m_path_part_index = 0;

			break;
		}
	}
}

void PathHandler::update(const float dt)
{
	if (m_following_path)
	{
		// Position
		glm::vec3 current_pos = m_paths[m_path_index][m_path_part_index].pos;
		glm::vec3 next_pos = m_paths[m_path_index][m_path_part_index + 1].pos;
		glm::vec3 pos = (1.f - m_percent) * current_pos + m_percent * next_pos;

		// Yaw
		float current_yaw = m_paths[m_path_index][m_path_part_index].yaw;
		float next_yaw = m_paths[m_path_index][m_path_part_index + 1].yaw;
		float yaw = (1.f - m_percent) * current_yaw + m_percent * next_yaw;

		// Pitch
		float current_pitch = m_paths[m_path_index][m_path_part_index].pitch;
		float next_pitch = m_paths[m_path_index][m_path_part_index + 1].pitch;
		if (fabsf(current_pitch - next_pitch) > 3.141592f )
			current_pitch += 6.283184f;
		float pitch = (1.f - m_percent) * current_pitch + m_percent * next_pitch;
		pitch = fmod(pitch, 6.283184f);

		m_camera->set_pos(pos);
		m_camera->set_yaw_pitch(yaw, pitch);

		m_percent += m_speed * dt;
		if (m_percent > 0.99)
		{
			++m_path_part_index;
			m_percent = 0.f;
			if (m_path_part_index == m_paths[m_path_index].size() - 1)
				m_following_path = false;
		}
	}
}
