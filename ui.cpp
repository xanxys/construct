#include "ui.h"

namespace construct {

std::shared_ptr<Texture> createTextureFromSurface(cairo_surface_t* surface) {
	// Convert cairo format to GL format.
	GLint gl_internal_format;
	GLint gl_format;
	const cairo_format_t format = cairo_image_surface_get_format(surface);
	if(format == CAIRO_FORMAT_ARGB32) {
		gl_internal_format = GL_RGBA;
		gl_format = GL_BGRA;
	} else if(format == CAIRO_FORMAT_RGB24) {
		gl_internal_format = GL_RGB;
		gl_format = GL_BGR;
	} else {
		throw "Unsupported surface type";
	}

	// Create texture
	const int width = cairo_image_surface_get_width(surface);
	const int height = cairo_image_surface_get_height(surface);

	auto texture = Texture::create(width, height);
	texture->useIn();
	glTexImage2D(GL_TEXTURE_2D, 0, gl_internal_format, width, height, 0, gl_format, GL_UNSIGNED_BYTE, cairo_image_surface_get_data(surface));

	return texture;
}

std::shared_ptr<Geometry> generateTexQuadGeometry(
	float width, float height, Eigen::Vector3f pos, Eigen::Matrix3f rot) {

	Eigen::Matrix<float, 6, 3 + 2, Eigen::RowMajor> vertex;

	GLfloat vertex_pos_uv[] = {
		-1.0f, 0, -1.0f, 0, 1,
		1.0f, 0, 1.0f, 1, 0,
		-1.0f, 0, 1.0f, 0, 0,

		 1.0f, 0, 1.0f, 1, 0,
		 -1.0f, 0, -1.0f, 0, 1,
		 1.0f, 0, -1.0f, 1, 1,
	};

	for(int i = 0; i < 6; i++) {
		Eigen::Vector3f p_local(
			vertex_pos_uv[i * 5 + 0] * width / 2,
			vertex_pos_uv[i * 5 + 1],
			vertex_pos_uv[i * 5 + 2] * height / 2);

		vertex.row(i).head(3) = rot * p_local + pos;
		vertex.row(i).tail(2) = Eigen::Vector2f(
			vertex_pos_uv[i * 5 + 3], vertex_pos_uv[i * 5 + 4]);
	}

	return Geometry::createPosUV(vertex.rows(), vertex.data());
}

void attachDasherQuadAt(Object& object, ObjectId label, float height_meter, float dx, float dy, float dz) {
	const float aspect_estimate = 1.0;
	const float px_per_meter = 500;

	const float width_meter = height_meter * aspect_estimate;

	// Create texture with string.
	const int width_px = px_per_meter * width_meter;
	const int height_px = px_per_meter * height_meter;

	cairo_surface_t* dasher_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width_px, height_px);
	auto c_context = cairo_create(dasher_surface);
	cairo_set_source_rgb(c_context, 1, 1, 1);
	cairo_paint(c_context);
	cairo_destroy(c_context);
	auto texture = createTextureFromSurface(dasher_surface);

	// Create geometry with texture.
	object.type = ObjectType::UI;
	object.geometry = generateTexQuadGeometry(width_meter, height_meter,
		Eigen::Vector3f(dx, dy, dz), Eigen::Matrix3f::Identity());
	object.texture = texture;
	object.use_blend = true;
	object.nscript.reset(new DasherScript(dasher_surface, label));
}

DasherScript::DasherScript(
	cairo_surface_t* surface, ObjectId label) :
	dasher_surface(surface), label(label), disabled(false) {
}

DasherScript::~DasherScript() {
	cairo_surface_destroy(dasher_surface);
}

void DasherScript::step(float dt, Object& object) {
	if(disabled) {
		return;
	}

	while(true) {
		auto message = object.getMessage();
		if(!message) {
			break;
		}

		if(message->isObject()) {
			if((*message)["type"] == "stare") {
				handleStare(object, *message);
			}
		}
	}
}

void DasherScript::handleStare(Object& object, Json::Value message) {
	dasher.update(1.0 / 30,
		10 * (message["v"].asFloat() - 0.5),
		10 * (0.5 - message["u"].asFloat()));

	auto ctx = cairo_create(dasher_surface);
	dasher.visualize(ctx);

	// center dot
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


TextLabelScript::TextLabelScript(cairo_surface_t* surface) :
	surface(surface), editing(false), stare_count(0) {
}

TextLabelScript::~TextLabelScript() {
	cairo_surface_destroy(surface);
}

void TextLabelScript::step(float dt, Object& object) {
	bool stare_found = false;
	while(true) {
		auto message = object.getMessage();
		if(!message) {
			break;
		}

		if(message->isString()) {
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
		} else if(message->isObject()) {
			if((*message)["type"] == "stare") {
				stare_found = true;
			}
		}
	}

	if(stare_found) {
		stare_count += 1;
	} else {
		stare_count = 0;
	}

	if(stare_count >= 15 && !editing) {
		editing = true;
		attachDasherQuadAt(object.scene.unsafeGet(object.scene.add()),
			object.id, 0.5, 0, 0.9, 1.4);
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
