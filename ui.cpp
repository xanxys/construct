#include "ui.h"

namespace construct {

DasherScript::DasherScript(
	std::function<OVR::Vector3f()> getHeadDirection,
	cairo_surface_t* surface, ObjectId label, ObjectId element) :
	getHeadDirection(getHeadDirection), dasher_surface(surface),
	label(label), element(element),
	disabled(false) {
}

DasherScript::~DasherScript() {
	cairo_surface_destroy(dasher_surface);
}

void DasherScript::step(float dt, Object& object) {
	if(disabled) {
		return;
	}

	auto dir = getHeadDirection();
	if(std::abs(dir.x) > 0.6 || std::abs(dir.z) > 0.6) {
		object.scene.deleteObject(element);
		disabled = true;
		return;
	}


	dasher.update(1.0 / 60, -dir.z * 10, -dir.x * 10);

	auto ctx = cairo_create(dasher_surface);
	dasher.visualize(ctx);

	// center
	cairo_new_path(ctx);
	cairo_arc(ctx, 125, 125, 1, 0, 2 * pi);
	cairo_set_source_rgb(ctx, 1, 0, 0);
	cairo_fill(ctx);

	cairo_destroy(ctx);

	object.texture->useIn();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		cairo_image_surface_get_width(dasher_surface),
		cairo_image_surface_get_height(dasher_surface),
		0, GL_BGRA, GL_UNSIGNED_BYTE,
		cairo_image_surface_get_data(dasher_surface));


	object.scene.sendMessage(label, Json::Value(dasher.getFixed()));
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


TextLabelScript::TextLabelScript(cairo_surface_t* surface) : surface(surface) {
}

TextLabelScript::~TextLabelScript() {
	cairo_surface_destroy(surface);
}

void TextLabelScript::step(float dt, Object& object) {
	auto message = object.getMessage();
	if(message && message->isString()) {
		const std::string text = message->asString();

		auto ctx = cairo_create(surface);
		cairo_set_source_rgb(ctx, 1, 1, 1);
		cairo_paint(ctx);

		cairo_set_source_rgb(ctx, 0, 0, 0);
		cairo_set_font_size(ctx, 30);
		cairo_translate(ctx, 10, 50);
		cairo_show_text(ctx, text.c_str());
		cairo_destroy(ctx);

		object.texture->useIn();
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
			cairo_image_surface_get_width(surface),
			cairo_image_surface_get_height(surface),
			0, GL_BGRA, GL_UNSIGNED_BYTE,
			cairo_image_surface_get_data(surface));
	}
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
