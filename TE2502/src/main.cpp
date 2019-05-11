#include <crtdbg.h>
#include <time.h>
#include <random>
#include "graphics/vulkan_context.hpp"

#include "application.hpp"
#include "utilities.hpp"

int main(int argc, const char** argv) 
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	srand((unsigned int)(time(NULL)));
	clear_output_file();
	{
		Application app(1800, 900, {"noise_texture.png"});
		if (argc >= 2 && strcmp(argv[1], "-r") == 0)
		{
			app.run(true);
		}
		else
		{
			app.run(false);
		}
	}
	return 0;
}