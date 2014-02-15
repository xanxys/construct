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
#include <GL/glew.h>
#include <glfw3.h>
#include <json/json.h>

#include "dasher.h"
#include "gl.h"
#include "OVR.h"
#include "scene.h"

namespace construct {

class DasherScript : public NativeScript {
public:
	DasherScript(
		std::function<OVR::Vector3f()> getHeadDirection,
		cairo_surface_t* surface, ObjectId label, ObjectId element);
	~DasherScript();

	void step(float dt, Object& object) override;
private:
	Dasher dasher;
	ObjectId label;

	bool disabled;
	ObjectId element;

	// TODO: unsafe reference to Core. Remove.
	std::function<OVR::Vector3f()> getHeadDirection;

	cairo_surface_t* dasher_surface;
};


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


class TextLabelScript : public NativeScript {
public:
	TextLabelScript(cairo_surface_t* surface);
	~TextLabelScript();

	void step(float dt, Object& object) override;
private:
	cairo_surface_t* surface;
};


class CursorScript : public NativeScript {
public:
	CursorScript(
		std::function<OVR::Vector3f()> getHeadDirection,
		std::function<Eigen::Vector3f()> getEyePosition,
		cairo_surface_t* surface);
	~CursorScript();

	void step(float dt, Object& object) override;
private:
	// TODO: unsafe reference to Core. Remove.
	std::function<OVR::Vector3f()> getHeadDirection;
	std::function<Eigen::Vector3f()> getEyePosition;

	cairo_surface_t* surface;
};

}  // namespace
