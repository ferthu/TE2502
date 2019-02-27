#pragma once
#include <glm/glm.hpp>
#include <vector>

class Camera;

class PathHandler
{
public:
	PathHandler(const std::string& file_path);
	~PathHandler();

	// Save the paths to file
	void save_to_file() const;

	// Attach the camera that will be used to create and follow paths
	void attach_camera(Camera* cam);

	// Check if a path name already exists
	bool path_name_exists(const std::string& name) const;

	// Start a new path (saves current position)
	void start_new_path();

	// Save the current position to the new path
	void save_path_part();

	// Finish the new path (saves current position)
	void finish_new_path();

	const std::vector<std::string>& get_path_names() const;

	void follow_path(const std::string& path_name);

	void update(const float dt);

private:
	struct PathPart
	{
		glm::vec3 pos;
		float yaw;
		float pitch;
	};
	typedef std::vector<PathPart> Path;

	std::string m_file_path;
	Camera* m_camera;
	std::vector<std::string> m_path_names;
	std::vector<Path> m_paths;
	bool m_creating_path = false;

	// Following path stuff
	bool m_following_path = false;
	int m_path_index = 0;
	int m_path_part_index = 0;
	float m_percent = 0.f;
	const float m_speed = 1.f;
};

