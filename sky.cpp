#include "sky.h"

namespace construct {

Sky::Sky() {

}

std::shared_ptr<Texture> Sky::generateEquirectangular() {
	const int height = 512;
	const int width = height * 2;
	auto texture = Texture::create(width, height, true);

	std::vector<float> data(width * height * 3, 0);
	for(int y = 0; y < height; y++) {
		for(int x = 0; x < width; x++) {
			// Random data.
			const int parity = ((x / 16) ^ (y / 16)) % 2;
			const float val = parity ? 150 : 100;
			data[(y * width + x) * 3 + 0] = val;
			data[(y * width + x) * 3 + 1] = val;
			data[(y * width + x) * 3 + 2] = val;
		}
	}

	texture->useIn();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_FLOAT, data.data());
	return texture;
}

}  // namespace
