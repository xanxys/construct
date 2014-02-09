// "Must" for NPR, comfortable rendering:
// * SSAO (uniform lighting and still look good)
// * FSAA (remove edge artifacts)
//
// Pipeline (no per-Geometry "artistic" settings).
// everything is fixed at reasonable parameters.
//
// G-buffer (pos, normal, albedo) -> 

// In-VR interactions:
// Provide text input that always works. (dasher)
// But avoid using text.

// All objects are isolated from each other;
// Object doesn't have address of other objects.
//
// Object-object interaction is limited to:
// 1 geometric neighbor access
// 2 forking itself
// 3 sending json object via id
//
// 1 and 2 should be used primarily, and 3 "very" sparingly:
// to send message from object A to distant object B,
// it's almost always better to visualize the message as another object
// and move it in the world.
//
// This way, everything is controllable from inside, by default.
// In contrast, if 3 (or direct method call) is used mainly,
// it can't be accessed safely from the inside because of
// fear of infinite loop and such (see smalltalk).

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
