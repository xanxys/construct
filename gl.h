#pragma once

#include <memory>
#include <string>
#include <string>
#include <vector>

#include <GL/glew.h>

namespace construct {

class Shader {
public:
	static std::shared_ptr<Shader> create(const std::string vertex_file_path, const std::string fragment_file_path);
	~Shader();

	void setUniform(std::string variable, GLint value);
	void setUniform(std::string variable, float v0);
	void setUniform(std::string variable, float v0, float v1);
	void setUniform(std::string variable, float v0, float v1, float v2, float v3);
	void setUniformMat4(std::string variable, float* pv);

	void use();
protected:
	Shader(const std::string vertex_file_path, const std::string fragment_file_path);
	GLint getVariable(const std::string& variable);

	std::string readFile(std::string path);
	std::string getLogFor(GLint id);
private:
	GLuint program;
};


class Texture {
public:
	~Texture();
	static std::shared_ptr<Texture> create(int width, int height);
	GLuint unsafeGetId();

	// n: texture slot index
	void useIn(int n = 0);
private:
	Texture(int width, int height);
private:
	GLuint id;
};


// Purpose of vertex array is unclear to me. keep it as is (or add helpful comment).
// Vertex buffer is tabular data, with columns = posx, posy, posz, u, v, for example.
//
// An attribute is a bunch of consectuve columns, such as position or texture coordinates.
class Geometry {
public:
	static std::shared_ptr<Geometry> createPos(int n_vertex, const float* pos);
	static std::shared_ptr<Geometry> createPosColor(int n_vertex, const float* pos_col);
	static std::shared_ptr<Geometry> createPosUV(int n_vertex, const float* pos_uv);
	~Geometry();

	void render();

	std::vector<float>& getData();
	void notifyDataChange();
protected:
	Geometry(int n_vertex, std::vector<int> attributes, const float* data);
	int getColumns();

	void sendToGPU();
private:
	const int n_vertex;
	GLuint vertex_array;
	GLuint vertex_buffer;

	std::vector<int> attributes;
	std::vector<float> raw_data;
};

}  // namespace
