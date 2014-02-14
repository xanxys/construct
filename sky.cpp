#include "sky.h"

#include <cmath>

#include <eigen3/Eigen/Dense>


namespace construct {

const float pi = 3.14159265359;

Sky::Sky() {
	turbidity = 10;

	// Initialize the Sun.
	sun_direction = Eigen::Vector3f(0, 20, 1);
	sun_direction.normalize();

	sun_power = Colorf(150e3, 150e3, 150e3);  // lx
}

std::shared_ptr<Texture> Sky::generateEquirectangular() {
	const int height = 256;
	const int width = height * 2;
	auto texture = Texture::create(width, height, true);

	std::vector<float> data(width * height * 3, 0);
	for(int y = 0; y < height; y++) {
		for(int x = 0; x < width; x++) {
			const float theta = pi * static_cast<float>(y) / height;
			const float phi = 2 * pi * static_cast<float>(x) / width;

			const Colorf radiance = getRadianceAt(theta, phi, false);

			for(int channel = 0; channel < 3; channel++) {
				data[(y * width + x) * 3 + channel] = radiance[channel];
			}
		}
	}

	texture->useIn();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_FLOAT, data.data());
	return texture;
}

// TODO: implement preetham sky model
Colorf Sky::getRadianceAt(float theta, float phi, bool checkerboard) {
	if(checkerboard) {
		const int x = theta / (pi / 5);
		const int y = phi / (pi / 5);

		const int parity = (x ^ y) % 2;
		const float val = parity ? 150 : 100;

		return Colorf(val, val, val);
	}

	if(theta > pi / 2) {
		return Colorf(0, 0, 0);
	}

	// Preetham sky model
	// http://www.cs.utah.edu/~shirley/papers/sunsky/sunsky.pdf
	//
	// Hints for reading the paper:
	// * we don't consider object light scattering
	//   (maybe needed later when considering far-away moutains or such)
	// * when there's no object, L(0) = 0 (universe background)
	// * don't get distracted by approximations for hand-calculation
	//   (it's simpler (and more flexible) to do numerical calc + smart memoization)
	const float view_height = 0;

	const float alpha_haze = 0.8333;  // haze: /km
	const float alpha_mole = 0.1136;  // molecules: /km

	// Constants for closed form solution of accumulated decay factor.
	/*
	const float k_haze = - beta0 / (alpha_haze * std::cos(theta));
	const float h_haze = std::exp(- alpha_haze * view_height);

	const float k_mole = - beta0 / (alpha_mole * std::cos(theta));
	const float h_mole = std::exp(- alpha_mole * view_height);
	*/

	// We use Eq.9 and 1km step numerical integration.
	// (Since it's easier to understand)
	// Considering up to 50km is enough. (which is top of stratosphere)
	//
	// TODO: if we use numerical calculation, we don't need to
	// pre-calculate decay factor at each distance.
	// Just decay step-by-step!! (like ray-tracing fog).
	//
	// Consider doing this when we add clouds.
	/*
	float intensity_haze = 0;
	for(int i = 0; i < 50; i++) {
		const float distance = i;
		const float density_haze = particleDensity(alpha_haze, distance);
		const float density_mole = particleDensity(alpha_mole, distance);

		intensity_haze +=
			density_haze *
			std::exp(-k_haze * (h_haze - density_haze)) *
			std::exp(-k_mole * (h_mole - density_mole));
	}

	float intensity_mole = 0;
	for(int i = 0; i < 50; i++) {
		const float distance = i;
		const float density_haze = particleDensity(alpha_haze, distance);
		const float density_mole = particleDensity(alpha_mole, distance);

		intensity_mole +=
			density_mole *
			std::exp(-k_haze * (h_haze - density_haze)) *
			std::exp(-k_mole * (h_mole - density_mole));
	}
	*/

	const Eigen::Vector3f view_direction(
		std::sin(theta) * std::cos(phi),
		std::sin(theta) * std::sin(phi),
		std::cos(theta));

	

	// Based on Nishita's 1st-order scattering sky model.
	// We ignore point-to-space decay.
	Colorf radiance(0, 0, 0);
	Colorf decay_here_to_view(1, 1, 1);
	for(int i = 0; i < 50; i++) {
		const float distance = i;
		const float height = distance * std::cos(theta) + view_height;

		//
		const float cos_view_sun = view_direction.dot(sun_direction);

		// Calculate scattering
		// Mie scattering's phase function is not well-described anywhere.
		// But it's said to be very sharp. So
		// we use (1+cos^3 theta)^2/4
		// (http://www.wolframalpha.com/input/?i=plot+r%3D%281%2Bcos%5E3+theta%29%5E2%2F4)
		const Colorf scatter =
			particleDensity(alpha_mole, distance, theta) * rayleigh(cos_view_sun) +
			particleDensity(alpha_haze, distance, theta) * mie(cos_view_sun);

		// Note. decay_space_to_here and decay_here_to_view are not colinear.
		radiance += sun_power
			.cwiseProduct(scatter)
			.cwiseProduct(decay_here_to_view);

		const Colorf decay_scatter =
			particleDensity(alpha_mole, distance, theta) * rayleighTotal() +
			particleDensity(alpha_haze, distance, theta) * mieTotal();

		decay_here_to_view = decay_here_to_view.cwiseProduct(
			Colorf(1, 1, 1) - decay_scatter);
	}
	assert(radiance[0] >= 0 && radiance[1] >= 0 && radiance[2] >= 0);
	radiance /= 100;
	return radiance;
}

Colorf Sky::rayleighTotal() {
	return Colorf(
		rayleighTotal(wl_r),
		rayleighTotal(wl_g),
		rayleighTotal(wl_b));
}

Colorf Sky::mieTotal() {
	return Colorf(
		mieTotal(wl_r),
		mieTotal(wl_g),
		mieTotal(wl_b));
}

float Sky::rayleighTotal(float lambda) {
	const float n_minus_1 = 0.00003;
	const float N = 2.545e25;
	const float pn = 0.035;

	return
		8 * std::pow(pi, 3) * std::pow(2 * n_minus_1, 2) /
		(3 * N * std::pow(lambda, 4)) *
		((6 + 3 * pn) / (6 - 7 * pn));
}

float Sky::mieTotal(float lambda) {
	assert(1 <= turbidity);
	const float n = 1.00003;
	const float c = (0.6544 * turbidity - 0.6510) * 1e-16;
	const float junge_exponent = 4;

	return 0.434 * c * std::pow(2 * pi / lambda, junge_exponent - 2) * 0.5 *
		0.67;
}

Colorf Sky::rayleigh(float cos) {
	return Colorf(
		rayleigh(cos, wl_r),
		rayleigh(cos, wl_g),
		rayleigh(cos, wl_b));
}

Colorf Sky::mie(float cos) {
	return Colorf(
		mie(cos, wl_r),
		mie(cos, wl_g),
		mie(cos, wl_b));
}

// Taken from Preetham, Appendix.3
float Sky::rayleigh(float cos, float lambda) {
	const float n_minus_1 = 0.00003;
	const float N = 2.545e25;
	const float pn = 0.035;

	return
		std::pow(pi * (2 * n_minus_1), 2) / (2 * N * std::pow(lambda, 4)) *
		((6 + 3 * pn) / (6 - 7 * pn)) *
		(1 + std::pow(cos, 2));
}

// Taken from Preetham, Appendix.3 (Wavelength-dependent component is
// approximated by hand)
float Sky::mie(float cos, float lambda) {
	assert(1 <= turbidity);
	const float n = 1.00003;
	const float c = (0.6544 * turbidity - 0.6510) * 1e-16;
	const float junge_exponent = 4;

	return 0.434 * c * std::pow(2 * pi / lambda, junge_exponent - 2) * 0.5 *
		std::pow(1 + std::pow(cos, 3), 2);
}

// u(distance) in the paper.
float Sky::particleDensity(float alpha, float distance, float theta) {
	const float view_height = 0;
	return std::exp(- alpha * (view_height + distance * std::cos(theta)));
}


}  // namespace
