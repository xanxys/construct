#include "light.h"

namespace construct {

Ray::Ray(Eigen::Vector3f org, Eigen::Vector3f dir) : org(org), dir(dir) {
}


Triangle::Triangle(Eigen::Vector3f p0, Eigen::Vector3f p1, Eigen::Vector3f p2) :
	p0(p0), d1(p1 - p0), d2(p2 - p0), normal(d1.cross(d2).normalized()),
	ir0({0, 0, 0}), ir1({0, 0, 0}), ir2({0, 0, 0}) {
	reflectance = Eigen::Vector3f(0.8, 0.8, 0.9);
}

boost::optional<Intersection> Triangle::intersect(Ray ray) {
	Eigen::Vector3f s1 = ray.dir.cross(d2);
	const float div = s1.dot(d1);
	if(div <= 0) {  // parallel or back
		return boost::optional<Intersection>();
	}

	const float div_inv = 1 / div;
		
	Eigen::Vector3f s  = ray.org - p0;
	const float a = s.dot(s1) * div_inv;
	if(a < 0 || a > 1) {
		return boost::optional<Intersection>();
	}
				
	Eigen::Vector3f s2 = s.cross(d1);
	const float b = ray.dir.dot(s2) * div_inv;
		
	if(b < 0 || a + b > 1) {
		return boost::optional<Intersection>();
	}

	const float t = d2.dot(s2) * div_inv;
	if(t < 0) {
		return boost::optional<Intersection>();
	}

	return boost::optional<Intersection>(std::make_tuple(
		t, ray.org + ray.dir * t, normal,
		(1 - a - b) * ir0 + a * ir1 + b * ir2));
}

Eigen::Vector3f Triangle::getVertexPos(int i) {
	assert(0 <= i && i < 3);
	if(i == 0) {
		return p0;
	} else if(i == 1) {
		return p0 + d1;
	} else {
		return p0 + d2;
	}
}

Eigen::Vector3f Triangle::getNormal() {
	return normal;
}

Colorf Triangle::brdf() {
	return reflectance / pi;
}

}  // namespace
