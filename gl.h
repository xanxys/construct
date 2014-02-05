#pragma once
#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <GL/glew.h>

class Shader {
public:
	static std::shared_ptr<Shader> create(const char* vertex_file_path, const char* fragment_file_path);
	~Shader();

	void setUniform(std::string variable, GLint value);
	void setUniform(std::string variable, float v0);
	void setUniform(std::string variable, float v0, float v1);
	void setUniform(std::string variable, float v0, float v1, float v2, float v3);
	void setUniformMat4(std::string variable, float* pv);

	void use();
protected:
	Shader(const char* vertex_file_path, const char* fragment_file_path);
	GLint getVariable(const std::string& variable);
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
