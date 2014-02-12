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
#include <jsoncpp/json/json.h>
#include <GL/glew.h>
#include <GL/glfw3.h>

#include "dasher.h"
#include "gl.h"
#include "OVR.h"
#include "scene.h"

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


class TextLabelScript : public NativeScript {
public:
	TextLabelScript(cairo_surface_t* surface);
	~TextLabelScript();

	void step(float dt, Object& object) override;
private:
	cairo_surface_t* surface;
};
