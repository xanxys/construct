#include <string>

#include "core.h"

int main(int argc, char** argv) {
	bool windowed = false;
	if(argc == 2 && std::string(argv[1]) == "--window") {
		windowed = true;
	}

	Core core(windowed);
	core.run();

	return 0;
}
