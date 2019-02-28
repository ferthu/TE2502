#pragma once
#include <glm/glm.hpp>
#include <vector>

class Camera;

enum class MODE {
	CREATING,
	FOLLOWING,
	NOTHING
};

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

	// Finish the new path (saves current position)
	void finish_new_path();

	// Cancel and delete the new path
	void cancel_new_path();

	// Get all path names
	const std::vector<std::string>& get_path_names() const;

	// Get the current internal mode/state
	MODE get_mode() const;

	// Start following an existing path with the given name
	void follow_path(const std::string& path_name);

	// Stop following the currently followed path
	void stop_following();

	// Move along the started path
	// Need to call follow_path(...) before this does anything
	void update(const float dt);

private:
	// Save current position
	void save_path_part();

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

	MODE m_mode = MODE::NOTHING;
	float m_countdown;
	const float m_max_countdown = 0.1f;  // If this changes, the existing paths will not be the speed they were saved at

	// Following path stuff
	int m_path_index = 0;
	int m_path_part_index = 0;
	float m_percent = 0.f;
	const float m_speed = 1.f;
};

