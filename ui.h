#pragma once

#include <array>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <boost/optional.hpp>
#include <cairo/cairo.h>
#include <eigen3/Eigen/Dense>
#include <eigen3/Eigen/Geometry>
#include <GL/glew.h>
#include <json/json.h>

#include "gl.h"
#include "OVR.h"
#include "scene.h"
#include "ui_common.h"
#include "ui_text.h"

namespace construct {

class LocomotionScript : public NativeScript {
public:
	LocomotionScript(
		std::function<OVR::Vector3f()> getHeadDirection,
		std::function<Eigen::Vector3f()> getEyePosition,
		std::function<void(Eigen::Vector3f)> setMovingDirection,
		cairo_surface_t* surface);
	~LocomotionScript();

	void step(float dt, Object& object) override;
private:
	// TODO: unsafe reference to Core. Remove.
	std::function<OVR::Vector3f()> getHeadDirection;
	std::function<Eigen::Vector3f()> getEyePosition;
	std::function<void(Eigen::Vector3f)> setMovingDirection;

	cairo_surface_t* surface;
};


class CursorScript : public NativeScript {
public:
	CursorScript(
		std::function<Eigen::Vector3f()> getHeadDirection,
		std::function<Eigen::Vector3f()> getEyePosition,
		cairo_surface_t* surface);
	~CursorScript();

	void step(float dt, Object& object) override;
private:
	// Generate a transform such that:
	// x,y -> perpendiculat to normal
	// z -> normal
	Eigen::Matrix3f createBasis(Eigen::Vector3f normal);
private:
	// TODO: unsafe reference to Core. Remove.
	std::function<Eigen::Vector3f()> getHeadDirection;
	std::function<Eigen::Vector3f()> getEyePosition;

	cairo_surface_t* surface;
};

}  // namespace
