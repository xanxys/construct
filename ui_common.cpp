#include "ui_common.h"

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

}  // namespace
