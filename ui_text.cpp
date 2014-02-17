#include "ui_text.h"

namespace construct {

void attachDasherQuadAt(Object& object, ObjectId label, float height_meter) {
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
		Eigen::Vector3f::Zero(), Eigen::Matrix3f::Identity());
	object.texture = texture;
	object.use_blend = true;
	object.nscript.reset(new DasherScript(dasher_surface, label));
}


Object& attachTextQuadAt(Object& object, std::string text, float height_meter, float dx, float dy, float dz) {
	const float aspect_estimate = text.size() / 3.0f;  // assuming japanese letters in UTF-8.
	const float px_per_meter = 500;

	const float width_meter = height_meter * aspect_estimate;

	// Create texture with string.
	const int width_px = px_per_meter * width_meter;
	const int height_px = px_per_meter * height_meter;

	auto surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width_px, height_px);
	auto c_context = cairo_create(surf);

	cairo_select_font_face(c_context, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_source_rgb(c_context, 1, 1, 1);

	cairo_rectangle(c_context, 0, 0, 10, 10);
	cairo_fill(c_context);

	cairo_set_font_size(c_context, 40);
	cairo_translate(c_context, 10, 0.8 * height_px);
	cairo_show_text(c_context, text.c_str());
	cairo_destroy(c_context);
	auto texture = createTextureFromSurface(surf);

	object.type = ObjectType::UI;
	object.geometry = generateTexQuadGeometry(width_meter, height_meter,
		Eigen::Vector3f(dx, dy, dz), Eigen::Matrix3f::Identity());
	object.texture = texture;
	object.use_blend = true;
	object.nscript.reset(new TextLabelScript(surf));

	return object;
}


DasherScript::DasherScript(
	cairo_surface_t* surface, ObjectId label) :
	dasher_surface(surface), label(label), disabled(false), activated(false) {
}

DasherScript::~DasherScript() {
	cairo_surface_destroy(dasher_surface);
}

void DasherScript::step(float dt, Object& object) {
	if(disabled) {
		return;
	}

	bool stared = false;
	while(true) {
		auto message = object.getMessage();
		if(!message) {
			break;
		}

		if(message->isObject()) {
			if((*message)["type"] == "stare") {
				stared = true;
				handleStare(object, *message);
			}
		}
	}

	// Remove once de-focused.
	if(!activated && stared) {
		activated = true;
	} else if(activated && !stared) {
		object.scene.deleteObject(object.id);
		disabled = true;
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

		Object& dasher = object.scene.unsafeGet(object.scene.add());
		attachDasherQuadAt(dasher, object.id, 0.5);
		dasher.setLocalToWorld(Transform3f(Eigen::Translation<float, 3>(
			object.getLocalToWorld() * Eigen::Vector3f::Zero() + Eigen::Vector3f(0, 0, 0.4))));
	}
}

}  // namespace
