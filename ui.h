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

// This is the place for
// 1. special scripts
// 2. in-dev widgets
//
// Special scripts are NativeScript-derived classes that holds non-local references.
// As such, there should be no attach... functions.
//
// In-dev widgets are normal widgets, but too small to warrant their own files.
// As soon as they get large and mature enough, they should be moved to ui_... files.
namespace construct {

class UserMenuScript : public NativeScript {
public:
	UserMenuScript(std::function<Json::Value()> getStat,
		cairo_surface_t* surface);
	~UserMenuScript();

	void step(float dt, Object& object) override;
private:
	std::function<Json::Value()> getStat;
	cairo_surface_t* surface;
};


// Controls avatar movement by stare.
class LocomotionScript : public NativeScript {
public:
	// TODO: OVR dependency should be dropped.
	LocomotionScript(
		std::function<OVR::Vector3f()> getHeadDirection,
		std::function<Eigen::Vector3f()> getEyePosition,
		std::function<void(Eigen::Vector3f)> setMovingDirection,
		cairo_surface_t* surface);
	~LocomotionScript();

	void step(float dt, Object& object) override;
private:
	std::function<OVR::Vector3f()> getHeadDirection;
	std::function<Eigen::Vector3f()> getEyePosition;
	std::function<void(Eigen::Vector3f)> setMovingDirection;

	cairo_surface_t* surface;
};


// Show user's focus and send "stare" message to an object being looked at.
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
	std::function<Eigen::Vector3f()> getHeadDirection;
	std::function<Eigen::Vector3f()> getEyePosition;

	cairo_surface_t* surface;
};

}  // namespace
