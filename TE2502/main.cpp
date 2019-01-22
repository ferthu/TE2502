#include <crtdbg.h>
#include <time.h>
#include <random>

#include "application.hpp"

int main() {
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	srand((unsigned int)(time(NULL)));

	Application app;
	app.run();

	return 0;
}