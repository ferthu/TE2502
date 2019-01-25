#pragma once
#include <string>
#include <intrin.h>

// Clears "output.txt"
void clear_output_file();

// Prints string to console and "output.txt"
void print(const std::string& str);

// Prints string, WITH newline, to console and "output.txt"
void println(const std::string& str);

#ifdef _DEBUG
#define CHECK(expression, error_string)\
do\
{\
	if (!(expression))\
	{\
		__debugbreak();\
	}\
} while (0);
#else
#define CHECK(expression, error_string)\
do\
{\
	if (!(expression))\
	{\
		println(error_string); \
		exit(1); \
	}\
} while (0);
#endif

// Checks that a Vulkan call results in VK_SUCCESS. If not, the program 
// writes 'error_string' to log file and exits, or if in debug build, 
// triggers a breakpoint
#define VK_CHECK(expression, error_string) CHECK((expression) != VK_SUCCESS, error_string)