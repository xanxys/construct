#pragma once

#include <memory>

#include "gl.h"

namespace construct {

class Sky {
public:
	Sky();
	std::shared_ptr<Texture> generateEquirectangular();
};

}  // namespace
