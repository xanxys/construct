#include "scene.h"

#include <memory>
#include <vector>

#include "gtest/gtest.h"

using namespace construct;

TEST(TriangleTest, IntersectionIsValid) {
	Triangle triangle(
		Eigen::Vector3f(0, 0, 0),
		Eigen::Vector3f(1, 0, 0),
		Eigen::Vector3f(0, 1, 0));

	// CCW is front, CW is back.
	Ray ray_front_inside(Eigen::Vector3f(0.25, 0.25, 1), Eigen::Vector3f(0, 0, -1));
	Ray ray_front_outside(Eigen::Vector3f(0.75, 0.75, 1), Eigen::Vector3f(0, 0, -1));
	Ray ray_back_inside(Eigen::Vector3f(0.25, 0.25, -1), Eigen::Vector3f(0, 0, 1));
	Ray ray_back_outside(Eigen::Vector3f(0.75, 0.75, -1), Eigen::Vector3f(0, 0, 1));

	// Check intersection's existence.
	EXPECT_TRUE(triangle.intersect(ray_front_inside));
	EXPECT_FALSE(triangle.intersect(ray_front_outside));
	EXPECT_FALSE(triangle.intersect(ray_back_inside));
	EXPECT_FALSE(triangle.intersect(ray_back_outside));

	// Check t & normal.
	auto isect = *triangle.intersect(ray_front_inside);
	EXPECT_NEAR(1, std::get<0>(isect), 1e-5);  // t
	EXPECT_NEAR(0, (std::get<1>(isect) - Eigen::Vector3f(0.25, 0.25, 0)).norm(), 1e-5);
	EXPECT_NEAR(0, (std::get<2>(isect) - Eigen::Vector3f(0, 0, 1)).norm(), 1e-5);
}
