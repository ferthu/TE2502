#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <regex>

// Easy way to read and write config-files or similar.
// Has no errorchecking so will crash if wrong key is entered
class FFile
{
public: 
	FFile() {}
	FFile(const std::string& path);
	virtual ~FFile() {}
	bool is_open() const;
	bool load_file(const std::string& path);

	const std::string& operator[] (const std::string&& key) const;
	const std::string& operator[] (const int line_number) const;

	std::string as_str(const std::string& key) const;
	int as_int(const std::string& key) const;
	uint32_t as_uint32(const std::string& key) const;
	uint64_t as_uint64(const std::string& key) const;
	float as_float(const std::string& key) const;

	std::vector<std::string> as_str_vec(const std::string& key) const;
	std::vector<int> as_int_vec(const std::string& key) const;
	std::vector<uint32_t> as_uint32_vec(const std::string& key) const;
	std::vector<uint64_t> as_uint64_vec(const std::string& key) const;
	std::vector<float> as_float_vec(const std::string& key) const;

	void write(const std::string&& value, const std::string&& key);
	void write(const int value, const std::string&& key);
	void write(const float value, const std::string&& key);

	void remove(const std::string&& key);

	size_t count() const;
	bool exists(const std::string&& key) const;

	std::vector<std::pair<std::string, std::vector<uint64_t>>> get_all_as_u64();

private:
	void write_all_to_file() const;

	std::string                                   m_path = "";
	std::unordered_map<std::string, std::string>  m_data;
	std::vector<std::string>                      m_index_mapping;
	bool                                          m_file_loaded = false;
};


inline FFile::FFile(const std::string& path)
{
	m_path = path;
	load_file(std::move(path));
}

inline bool FFile::is_open() const
{
	return m_file_loaded;
}

// Loads a file
inline bool FFile::load_file(const std::string& path)
{
	std::ifstream file;
	file.open(std::move(path));
	if (file.is_open())
	{
		// Go through all lines
		std::string line;
		while (std::getline(file, line))
		{
			// Extract key
			const size_t colon_index = line.find_first_of(':');
			std::string key = line.substr(0, colon_index);

			// Remove trailing, leading and extra spaces
			key = std::regex_replace(key, std::regex("^ +| +$|( ) +"), "$1");

			// Extract value
			std::string value = line.substr(colon_index + 1);
			value = value.substr(value.find_first_not_of(' '));
			size_t last_index = value.length() - 1;
			while (value[last_index] == ' ') { --last_index; }  // Remove any trailing spaces
			value = value.substr(0, last_index + 1);

			// Insert
			m_data[key] = value;
			m_index_mapping.push_back(key);
		}
		file.close();
		m_path = path;
		m_file_loaded = true;
		return true;
	}
	else
		return false;
}

// Get string from key. Will crash if invalid key
inline const std::string& FFile::operator[](const std::string&& key) const
{
	return m_data.find(std::move(key))->second;
}

// Get string from line number. Will crash if invalid key
inline const std::string& FFile::operator[](const int line_number) const
{
	return m_data.find(m_index_mapping[line_number])->second;
}

// Get string from key. Will crash if invalid key
inline std::string FFile::as_str(const std::string& key) const
{
	return m_data.find(std::move(key))->second;
}

// Get int from key. Will crash if invalid key
inline int FFile::as_int(const std::string& key) const
{
	return std::stoi(m_data.find(key)->second);
}

inline uint32_t FFile::as_uint32(const std::string& key) const
{
	return std::stoul(m_data.find(key)->second);
}

inline uint64_t FFile::as_uint64(const std::string& key) const
{
	return std::stoull(m_data.find(key)->second);
}

// Get float from key. Will crash if invalid key
inline float FFile::as_float(const std::string& key) const
{
	std::string::size_type sz;
	return std::stof(m_data.find(key)->second, &sz);
}

// Splits and returns a vector from the given key. Will crash if invalid key
inline std::vector<std::string> FFile::as_str_vec(const std::string& key) const
{
	std::vector<std::string> res;
	const std::string line = m_data.find(key)->second;
	std::string temp = "";
	for (unsigned int i = 0; i < line.length(); ++i)
	{
		if (line[i] != ' ' && line[i] != ',')
			temp += line[i];
		else
		{
			res.push_back(temp);
			temp = "";
		}
	}
	if (temp != "")
		res.push_back(temp);
	return res;
}

// Splits and returns a vector from the given key. Will crash if invalid key
inline std::vector<int> FFile::as_int_vec(const std::string& key) const
{
	std::vector<int> res;
	for (auto v : as_str_vec(std::move(key)))
		res.push_back(std::atoi(v.c_str()));
	return res;
}

inline std::vector<uint32_t> FFile::as_uint32_vec(const std::string& key) const
{
	std::vector<uint32_t> res;
	for (auto v : as_str_vec(std::move(key)))
		res.push_back(std::stoul(v.c_str()));
	return res;
}

inline std::vector<uint64_t> FFile::as_uint64_vec(const std::string& key) const
{
	std::vector<uint64_t> res;
	for (auto v : as_str_vec(std::move(key)))
		res.push_back(std::stoull(v.c_str()));
	return res;
}

// Splits and returns a vector from the given key. Will crash if invalid key
inline std::vector<float> FFile::as_float_vec(const std::string& key) const
{
	std::string::size_type sz;
	std::vector<float> res;
	for (auto v : as_str_vec(std::move(key)))
		res.push_back(std::stof(m_data.find(key)->second, &sz));
	return res;
}

inline void FFile::write(const std::string && value, const std::string && key)
{
	if (m_data.count(key) <= 0)
		m_index_mapping.push_back(key);  // Add index if new key
	m_data[std::move(key)] = value;
	write_all_to_file();
}

inline void FFile::write(const int value, const std::string && key)
{
	if (m_data.count(key) <= 0)
		m_index_mapping.push_back(key);  // Add index if new key
	m_data[std::move(key)] = std::to_string(value);
	write_all_to_file();
}

inline void FFile::write(const float value, const std::string && key)
{
	if (m_data.count(key) <= 0)
		m_index_mapping.push_back(key);  // Add index if new key
	m_data[std::move(key)] = std::to_string(value);
	write_all_to_file();
}

inline void FFile::remove(const std::string && key)
{
	m_data.erase(key);
	write_all_to_file();
}

// Get the number of lines
inline size_t FFile::count() const
{
	return m_data.size();
}

// Check if the given key exists
inline bool FFile::exists(const std::string && key) const
{
	return m_data.count(key) > 0;
}

inline std::vector<std::pair<std::string, std::vector<uint64_t>>> FFile::get_all_as_u64()
{
	std::vector<std::pair<std::string, std::vector<uint64_t>>> ret;

	for (auto& [key, val] : m_data)
	{
		ret.push_back(std::make_pair(key, as_uint64_vec(key)));
	}

	return ret;
}

inline void FFile::write_all_to_file() const
{
	std::ofstream file(m_path);
	for (std::string key : m_index_mapping)
		file << key << ": " << m_data.find(key)->second << std::endl;
	file.close();
}