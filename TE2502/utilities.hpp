#pragma once
#include <string>
#include <iostream>

// Clears "output.txt"
void clear_output_file()
{
#ifdef _DEBUG
	FILE* fp;
	fopen_s(&fp, "output.txt", "w");
	fclose(fp);
#endif // DEBUG
}

// Prints string to console and "output.txt"
void print(const std::string& str)
{
#ifdef _DEBUG
	std::cout << str;

	FILE* fp;
	fopen_s(&fp, "output.txt", "a");
	fprintf(fp, str.c_str());
	fclose(fp);
#endif // DEBUG
}

// Prints string, WITH newline, to console and "output.txt"
void println(const std::string& str)
{
#ifdef _DEBUG
	std::cout << str << std::endl;

	FILE* fp;
	fopen_s(&fp, "output.txt", "a");
	fprintf(fp, str.c_str());
	fprintf(fp, "\n");
	fclose(fp);
#endif // DEBUG
}
