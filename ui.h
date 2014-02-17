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

std::shared_ptr<Texture> createTextureFromSurface(cairo_surface_t* surface);

// default orientation is to surface look "normal" to Y- direction, with size
// [-width/2, width/2] * [0,0] * [-height/2, height/2].
std::shared_ptr<Geometry> generateTexQuadGeometry(
	float width, float height, Eigen::Vector3f pos, Eigen::Matrix3f rot);

void attachDasherQuadAt(Object& widget, ObjectId label, float height, float dx, float dy, float dz);

class DasherScript : public NativeScript {
public:
	DasherScript(
		cairo_surface_t* surface, ObjectId label);
	~DasherScript();

	void step(float dt, Object& object) override;
private:
	void handleStare(Object& object, Json::Value v);
private:
	Dasher dasher;
	ObjectId label;
	bool disabled;

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
	int stare_count;
	bool editing;
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
