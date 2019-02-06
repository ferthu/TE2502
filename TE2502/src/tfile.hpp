#pragma once
#include "ffile.hpp"

// Loads variables from settings file and replaces variables in shader files before compiling them
class TFile
{
public:
	TFile(const std::string& settings_file, const std::string& shader_dir);

	// Returns key as u32
	uint32_t get_u32(const std::string& key);

	// Returns key as u64
	uint64_t get_u64(const std::string& key);

	void compile_shaders();
private:
	std::unordered_map<std::string, uint64_t> m_map;

	FFile m_ffile;

	std::string m_shader_dir;
	std::string m_settings_file_path;
};