#pragma once

#include <memory>

#include "gl.h"
#include "scene.h"

namespace construct {

// Large-scale, non-interactive effects, such as
// sky, weather, stars, sunlight, fog, etc. comes here.
class Sky {
public:
	Sky();

	// Return 1:2 texture
	// direction spec: TBD
	std::shared_ptr<Texture> generateEquirectangular();
protected:
	Colorf getRadianceAt(float theta, float phi, bool checkerboard = false);

	float particleDensity(float alpha, float distance, float theta);

	

	// Total Scattering
	Colorf rayleighTotal();
	Colorf mieTotal();
	
	float rayleighTotal(float lambda);
	float mieTotal(float lambda);

	// Directional scattering
	Colorf rayleigh(float cos);
	Colorf mie(float cos);

	float rayleigh(float cos, float lambda);
	float mie(float cos, float lambda);
private:
	Eigen::Vector3f sun_direction;
	Colorf sun_power;
	float turbidity;  // 5:fog 20:hazy 5:clear 1.5:super clear

	// color space
	// Narest pure colors of sRGB vertices.
	const float wl_r = 615e-9;
	const float wl_g = 545e-9;
	const float wl_b = 465e-9;
};

}  // namespace
