#pragma once
#include <string>

// Clears "output.txt"
void clear_output_file();

// Prints string to console and "output.txt"
void print(const std::string& str);

// Prints string, WITH newline, to console and "output.txt"
void println(const std::string& str);
