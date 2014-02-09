#include "ui.h"



DasherScript::DasherScript(
	std::function<OVR::Vector3f()> getHeadDirection,
	cairo_surface_t* surface, ObjectId label) :
	getHeadDirection(getHeadDirection), dasher_surface(surface), label(label) {
}

DasherScript::~DasherScript() {
	cairo_surface_destroy(dasher_surface);
}

void DasherScript::step(float dt, Object& object) {
	auto dir = getHeadDirection();

	dasher.update(1.0 / 60, -dir.z * 10, -dir.x * 1);

	auto ctx = cairo_create(dasher_surface);
	dasher.visualize(ctx);

	// center
	cairo_new_path(ctx);
	cairo_arc(ctx, 125, 125, 1, 0, 2 * 3.1415);
	cairo_set_source_rgb(ctx, 1, 0, 0);
	cairo_fill(ctx);

	// cursor
	cairo_new_path(ctx);
	cairo_arc(ctx, 125 + dir.x * 250, 125 - dir.z * 250, 10, 0, 2 * 3.1415);
	cairo_set_source_rgb(ctx, 1, 0, 0);
	cairo_set_line_width(ctx, 3);
	cairo_stroke(ctx);

	cairo_destroy(ctx);

	object.texture->useIn();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		cairo_image_surface_get_width(dasher_surface),
		cairo_image_surface_get_height(dasher_surface),
		0, GL_BGRA, GL_UNSIGNED_BYTE,
		cairo_image_surface_get_data(dasher_surface));


	object.scene.sendMessage(label, Json::Value(dasher.getFixed()));
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
