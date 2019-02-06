#include <Windows.h>
#include <tchar.h> 
#include <strsafe.h>
#include <iostream>

#include "tfile.hpp"
#include "utilities.hpp"

TFile::TFile(const std::string& settings_file, const std::string& shader_dir) : m_ffile(settings_file), m_settings_file_path(settings_file), m_shader_dir(shader_dir)
{
	// key : value, alignment, alignment_offset
	std::vector<std::pair<std::string, std::vector<uint64_t>>> vars = m_ffile.get_all_as_u64();

	for (auto&[key, nums] : vars)
	{
		// First value in vector is desired number
		uint64_t num = nums[0];

		// Second value specifies alignment. The value of num will be increased to fit the alignment
		uint32_t alignment = 1;
		if (nums.size() >= 2 && nums[1] > 1)
			alignment = nums[1];

		// If it exists, alignment offset is the third number in vector
		// (num + alignment_offset) % alignment should be equal to 0
		uint64_t alignment_offset = 0;
		if (nums.size() >= 3)
			alignment_offset = nums[2];

		// Increase num to satisfy alignment
		uint64_t misalignment = (num + alignment_offset) % alignment;
		if (misalignment != 0)
		{
			num += alignment - misalignment;
		}

		m_map[key] = num;
	}
}

uint32_t TFile::get_u32(const std::string& key)
{
	return static_cast<uint32_t>(m_map.at(key));
}

uint64_t TFile::get_u64(const std::string& key)
{
	return m_map.at(key);
}

void TFile::compile_shaders()
{
	// Get all shader files

	// For all shader files
		// Create temp copy
		// Replace vars with numbers
		// Save temp file
		
		// Start compiler
		// Wait

	TCHAR dir[MAX_PATH];
	WIN32_FIND_DATA ffd;
	HANDLE h_find = INVALID_HANDLE_VALUE;
	DWORD dw_error = 0;

	// Copy directory to buffer and append wildcard
	StringCchCopyA(dir, MAX_PATH, m_shader_dir.c_str());
	StringCchCatA(dir, MAX_PATH, "*.glsl");

	// Find the first file in directory
	h_find = FindFirstFileA(dir, &ffd);

	CHECK(INVALID_HANDLE_VALUE != h_find, "Found no files in specified shader directory!");

#ifdef _DEBUG
	std::cout << "Compiling shaders:\n";
#endif

	do
	{
		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			// File is a directory, do nothing
		}
		else
		{
			std::string command_line = m_shader_dir;
			command_line += "glslangValidator.exe ";
			command_line += "-o \"" + m_shader_dir + "compiled/";
			command_line += ffd.cFileName;
			command_line += ".spv\" ";

			for (auto& [key, val] : m_map)
			{
				command_line += "-D" + key + "=" + std::to_string(val) + " ";
			}

			command_line += "-V " + m_shader_dir;
			command_line += ffd.cFileName;

			// Run compiler
			STARTUPINFO si;
			PROCESS_INFORMATION pi;

			ZeroMemory(&si, sizeof(si));
			si.cb = sizeof(si);
			ZeroMemory(&pi, sizeof(pi));



			CHECK(CreateProcessA(NULL,
				command_line.data(),						// Command line
				NULL,										// Process handle not inheritable
				NULL,										// Thread handle not inheritable
				FALSE,										// Set handle inheritance to FALSE
				0,											// No creation flags
				NULL,										// Use parent's environment block
				NULL,										// Use parent's starting directory 
				&si,										// Pointer to STARTUPINFO structure
				&pi)										// Pointer to PROCESS_INFORMATION structure
				, "Failed to start shader compiler!");


			DWORD error = GetLastError();
			
			//LPVOID lpMsgBuf;
			//LPVOID lpDisplayBuf;

			//FormatMessage(
			//	FORMAT_MESSAGE_ALLOCATE_BUFFER |
			//	FORMAT_MESSAGE_FROM_SYSTEM |
			//	FORMAT_MESSAGE_IGNORE_INSERTS,
			//	NULL,
			//	error,
			//	MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			//	(LPTSTR)&lpMsgBuf,
			//	0, NULL);

			// Wait until child process exits
			WaitForSingleObject(pi.hProcess, INFINITE);

			LPDWORD exit_code = &error;
			GetExitCodeProcess(pi.hProcess, exit_code);

			CHECK(*exit_code == 0, "Shader compilation failed!");

			// Close process and thread handles
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
	} while (FindNextFileA(h_find, &ffd) != 0);

	dw_error = GetLastError();
	CHECK(dw_error == ERROR_NO_MORE_FILES, "Error searching directory!");

	FindClose(h_find);
}
