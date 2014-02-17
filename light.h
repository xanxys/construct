#pragma once

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <boost/optional.hpp>
#include <eigen3/Eigen/Dense>
#include <json/json.h>

#include "util.h"

namespace construct {

typedef uint64_t ObjectId;

class Intersection {
public:
	Intersection(float t,
		Eigen::Vector3f pos, Eigen::Vector3f n, Colorf radiance,
		ObjectId id);

	// ray info
	float t;

	// surface info
	Eigen::Vector3f position;
	Eigen::Vector3f normal;
	Colorf radiance;

	// object info
	ObjectId id;
};


class Ray {
public:
	Ray(Eigen::Vector3f org, Eigen::Vector3f dir);
	Eigen::Vector3f at(float t);

	Eigen::Vector3f org;
	Eigen::Vector3f dir;
};


// Used for lighting.
class Triangle {
public:
	Triangle(Eigen::Vector3f p0, Eigen::Vector3f p1, Eigen::Vector3f p2);
	boost::optional<Intersection> intersect(Ray ray);

	Eigen::Vector3f getVertexPos(int i);
	Eigen::Vector3f getNormal();

	Colorf brdf();
public:
	// per-vertex irradiance.
	// TODO: decide if these should be shared or not.
	// pros of sharing:
	// * typically 1/3 lighting computation
	// cons of sharing:
	// * more complex and inflexible architecture (needs mesh class)
	// * slower lookup (due to de-referencing vertex info)
	Colorf ir0;
	Colorf ir1;
	Colorf ir2;


	Eigen::Vector3f p0;
	Eigen::Vector3f d1;
	Eigen::Vector3f d2;

	ObjectId attribute;
private:
	Eigen::Vector3f normal;
	// pointer = 8 byte, Vec3f = 12 byte. storing reference is stupid.

	// Currently we assume non-smooth shading (normal = triangle normal).

	// Lambert BRDF
	Colorf reflectance;
};

}  // namespace
