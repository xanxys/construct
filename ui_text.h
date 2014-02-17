#pragma once

#include <array>
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

#include "dasher.h"
#include "gl.h"
#include "scene.h"
#include "ui_common.h"

namespace construct {

void attachDasherQuadAt(Object& widget, ObjectId label, float height);
Object& attachTextQuadAt(Object& object, std::string text, float height, float dx, float dy, float dz);

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
	bool activated;

	cairo_surface_t* dasher_surface;
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

}  // namespace
