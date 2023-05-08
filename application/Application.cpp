#include <iostream>
#include <stdexcept>
#include <cstdlib>

#include <Engine.hpp>

int main()
{
	Engine app;

	try {
		app.run();
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}