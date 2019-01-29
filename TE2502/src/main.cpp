#include <crtdbg.h>
#include <time.h>
#include <random>
#include "graphics/vulkan_context.hpp"

#include "application.hpp"
#include "utilities.hpp"

int main() {
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	srand((unsigned int)(time(NULL)));
	clear_output_file();
	{
		Application app;
		app.run();
	}
	return 0;
}