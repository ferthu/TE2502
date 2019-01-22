#pragma once
#include <string>
#include <iostream>

void clear_output_file()
{
#ifdef _DEBUG
	FILE* fp;
	fopen_s(&fp, "output.txt", "w");
	fclose(fp);
#endif // DEBUG
}

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
