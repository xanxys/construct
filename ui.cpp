#include "ui.h"

#include <boost/algorithm/string.hpp>

namespace construct {


UserMenuScript::UserMenuScript(std::function<Json::Value()> getStat,
	cairo_surface_t* surface) :
	getStat(getStat), surface(surface) {
}

UserMenuScript::~UserMenuScript() {
	cairo_surface_destroy(surface);
}

void UserMenuScript::step(float dt, Object& object) {
	const float height_px = 20;
	auto c_context = cairo_create(surface);

	cairo_select_font_face(c_context, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

	cairo_set_source_rgb(c_context, 1, 0.9, 1);
	cairo_paint(c_context);

	cairo_set_font_size(c_context, height_px);
	cairo_set_source_rgb(c_context, 0, 0, 0);
	cairo_translate(c_context, 0, 0.8 * height_px);

	const std::string stat_multiline = Json::StyledWriter().write(getStat());
	std::vector<std::string> lines;
	boost::algorithm::split(lines, stat_multiline, boost::is_any_of("\n"));
	cairo_save(c_context);
	for(const auto& line : lines) {
		cairo_show_text(c_context, line.c_str());
		cairo_fill(c_context);
		cairo_translate(c_context, 0, height_px);
	}
	cairo_restore(c_context);

	cairo_destroy(c_context);

	object.texture->useIn();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		cairo_image_surface_get_width(surface),
		cairo_image_surface_get_height(surface),
		0, GL_BGRA, GL_UNSIGNED_BYTE,
		cairo_image_surface_get_data(surface));
}


LocomotionScript::LocomotionScript(
	std::function<OVR::Vector3f()> getHeadDirection,
	std::function<Eigen::Vector3f()> getEyePosition,
	std::function<void(Eigen::Vector3f)> setMovingDirection,
	cairo_surface_t* surface) :
	getHeadDirection(getHeadDirection),
	getEyePosition(getEyePosition),
	setMovingDirection(setMovingDirection),
	surface(surface) {
}

LocomotionScript::~LocomotionScript() {
	cairo_surface_destroy(surface);
}

void LocomotionScript::step(float dt, Object& object) {
	setMovingDirection(Eigen::Vector3f::Zero());

	const auto center_u = getEyePosition() - Eigen::Vector3f(0, 0, 1.4 - 0.05);
	object.setLocalToWorld(Transform3f(Eigen::Translation<float, 3>(center_u)));

	auto dir_pre = getHeadDirection();

	const Eigen::Vector3f org = getEyePosition();
	const Eigen::Vector3f dir(dir_pre.x, dir_pre.y, dir_pre.z);
	
	// Intersect ray with z = 0.05  (p.dot(normal) = dist)
	const Eigen::Vector3f normal(0, 0, 1);
	const float dist = 0.05;

	const float t = (dist - normal.dot(org)) / normal.dot(dir);

	if(t <= 0 || t > 10) {
		return;
	}

	const Eigen::Vector3f isect = org + t * dir;

	// Calculate position in normalized surface coord [0,1]^2.
	const Eigen::Vector3f center = center_u + Eigen::Vector3f(0, 2.5, 0);
	const Eigen::Vector3f size(0.9, 0.4, 0.1);

	const Eigen::Vector3f nc = (isect - center + size/2).cwiseQuotient(size);


	if(nc.x() < 0 || nc.y() < 0 || nc.x() > 1 || nc.y() > 1) {
		return;
	}


	// Draw something.
	auto ctx = cairo_create(surface);

	cairo_set_source_rgb(ctx, 1, 1, 1);
	cairo_paint(ctx);

	cairo_destroy(ctx);

	object.texture->useIn();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		cairo_image_surface_get_width(surface),
		cairo_image_surface_get_height(surface),
		0, GL_BGRA, GL_UNSIGNED_BYTE,
		cairo_image_surface_get_data(surface));

	setMovingDirection((isect - Eigen::Vector3f(0, 0, 0.05)).normalized());
}


CursorScript::CursorScript(
	std::function<Eigen::Vector3f()> getHeadDirection,
	std::function<Eigen::Vector3f()> getEyePosition,
	cairo_surface_t* surface) :
	getHeadDirection(getHeadDirection),
	getEyePosition(getEyePosition),
	surface(surface) {
}

CursorScript::~CursorScript() {
	cairo_surface_destroy(surface);
}

void CursorScript::step(float dt, Object& object) {
	Ray ray(getEyePosition(), getHeadDirection());
	auto isect = object.scene.intersectAny(ray);

	if(!isect) {
		// TODO: hide cursor
	} else {
		// Send message to the object.
		Json::Value message;
		message["type"] = "stare";
		message["u"] = isect->uv[0];
		message["v"] = isect->uv[1];
		object.scene.sendMessage(isect->id, message);

		// Move cursor.
		object.setLocalToWorld(
			Eigen::Translation<float, 3>(isect->position + isect->normal * 0.01) *
			createBasis(isect->normal));
	}
}

Eigen::Matrix3f CursorScript::createBasis(Eigen::Vector3f normal) {
	auto seed_y = Eigen::Vector3f::UnitX();

	auto axis_x = seed_y.cross(normal);
	if(axis_x.norm() == 0) {
		axis_x = Eigen::Vector3f::UnitY().cross(normal).normalized();
	} else {
		axis_x.normalize();
	}
	auto axis_y = normal.cross(axis_x).normalized();

	Eigen::Matrix3f m;
	m.col(0) = axis_x;
	m.col(1) = axis_y;
	m.col(2) = normal;
	return m;
}

}  // namespace
