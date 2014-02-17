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

#include "gl.h"
#include "scene.h"

namespace construct {

std::shared_ptr<Texture> createTextureFromSurface(cairo_surface_t* surface);

// default orientation is to surface look "normal" to Y- direction, with size
// [-width/2, width/2] * [0,0] * [-height/2, height/2].
std::shared_ptr<Geometry> generateTexQuadGeometry(
	float width, float height, Eigen::Vector3f pos, Eigen::Matrix3f rot);

void attachDasherQuadAt(Object& widget, ObjectId label, float height);

}  // namespace
