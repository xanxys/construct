#include "gl.h"

#include <array>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>

std::shared_ptr<Shader> Shader::create(const std::string vertex_file_path, const std::string fragment_file_path) {
	return std::shared_ptr<Shader>(new Shader(vertex_file_path, fragment_file_path));
}

Shader::~Shader() {
	glDeleteShader(program);
}

Shader::Shader(const std::string vertex_file_path, const std::string fragment_file_path) {
	std::cout << "Compiling shader: " << vertex_file_path << " + " << fragment_file_path << std::endl;

	// Compile Vertex Shader
	const GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	std::string vs_code = readFile(vertex_file_path);
	const char* vs_code_ptr = vs_code.c_str();
	glShaderSource(VertexShaderID, 1, &vs_code_ptr, NULL);
	glCompileShader(VertexShaderID);
	std::cout << getLogFor(VertexShaderID) << std::endl;

	// Compile Fragment Shader
	const GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	std::string fs_code = readFile(fragment_file_path);
	const char* fs_code_ptr = fs_code.c_str();
	glShaderSource(FragmentShaderID, 1, &fs_code_ptr, NULL);
	glCompileShader(FragmentShaderID);
	std::cout << getLogFor(FragmentShaderID) << std::endl;

	// Link the program
	program = glCreateProgram();
	glAttachShader(program, VertexShaderID);
	glAttachShader(program, FragmentShaderID);
	glLinkProgram(program);
	std::cout << getLogFor(program) << std::endl;

	// Release componenets.
	glDeleteShader(VertexShaderID);
	glDeleteShader(FragmentShaderID);
}

std::string Shader::readFile(std::string path) {
	std::ifstream ifs(path);
	std::stringstream buffer;
	buffer << ifs.rdbuf();
	return buffer.str();
}

std::string Shader::getLogFor(GLint id) {
	int log_size;
	glGetShaderiv(id, GL_INFO_LOG_LENGTH, &log_size);

	std::vector<char> log(log_size);
	glGetShaderInfoLog(id, log_size, nullptr, log.data());

	return std::string(log.begin(), log.end());
}

GLint Shader::getVariable(const std::string& variable) {
	const GLint shader_variable = glGetUniformLocation(program, variable.c_str());
	if(shader_variable < 0) {
		throw "Variable in shader \"" + variable + "\" not found or removed due to lack of use";
	}
	return shader_variable;
}

void Shader::setUniform(std::string variable, GLint value) {
	glUniform1i(getVariable(variable), value);
}

void Shader::setUniform(std::string variable, float v0) {
	glUniform1f(getVariable(variable), v0);
}

void Shader::setUniform(std::string variable, float v0, float v1) {
	glUniform2f(getVariable(variable), v0, v1);
}

void Shader::setUniform(std::string variable, float v0, float v1, float v2, float v3) {
	glUniform4f(getVariable(variable), v0, v1, v2, v3);
}

void Shader::setUniformMat4(std::string variable, float* pv) {
	glUniformMatrix4fv(getVariable(variable), 1, GL_TRUE, pv);
}


void Shader::use() {
	glUseProgram(program);
}


std::shared_ptr<Texture> Texture::create(int width, int height) {
	return std::shared_ptr<Texture>(new Texture(width, height));
}

GLuint Texture::unsafeGetId() {
	return id;
}

Texture::Texture(int width, int height) {
	glGenTextures(1, &id);

	// "Bind" the newly created texture : all future texture functions will modify this texture
	glBindTexture(GL_TEXTURE_2D, id);

	// Give an empty image to OpenGL ( the last "0" )
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height,0, GL_RGB, GL_UNSIGNED_BYTE, 0);

	// Poor filtering. Needed !
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
}

Texture::~Texture() {
	glDeleteTextures(1, &id);
}

void Texture::useIn(int n) {
	glActiveTexture(GL_TEXTURE0 + n);
	glBindTexture(GL_TEXTURE_2D, id);
}


std::shared_ptr<Geometry> Geometry::createPos(int n_vertex, const float* pos) {
	return std::shared_ptr<Geometry>(new Geometry(
		n_vertex,
		std::vector<int>({3}),
		pos));
}

std::shared_ptr<Geometry> Geometry::createPosColor(int n_vertex, const float* pos_col) {
	return std::shared_ptr<Geometry>(new Geometry(
		n_vertex,
		std::vector<int>({3, 3}),
		pos_col));
}

std::shared_ptr<Geometry> Geometry::createPosUV(int n_vertex, const float* pos_uv) {
	return std::shared_ptr<Geometry>(new Geometry(
		n_vertex,
		std::vector<int>({3, 2}),
		pos_uv));
}

Geometry::Geometry(int n_vertex, std::vector<int> attributes, const float* data) : n_vertex(n_vertex), attributes(attributes) {
	glGenVertexArrays(1, &vertex_array);
	glBindVertexArray(vertex_array);

	glGenBuffers(1, &vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * n_vertex * getColumns(), data, GL_STATIC_DRAW);
}

Geometry::~Geometry() {
	glDeleteVertexArrays(1, &vertex_array);
	glDeleteBuffers(1, &vertex_buffer);
}

int Geometry::getColumns() {
	return std::accumulate(attributes.begin(), attributes.end(), 0);
}

void Geometry::render() {
	glBindVertexArray(vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

	const int columns = getColumns();
	int current_offset = 0;
	for(int i_attrib = 0; i_attrib < attributes.size(); i_attrib++) {
		glEnableVertexAttribArray(i_attrib);
		glVertexAttribPointer(
			i_attrib,
			attributes[i_attrib],
			GL_FLOAT,
			GL_FALSE,
			sizeof(float) * columns,
			(void*)(sizeof(float) * current_offset));
		current_offset += attributes[i_attrib];
	}
	
	glDrawArrays(GL_TRIANGLES, 0, n_vertex);

	for(int i_attrib; i_attrib < attributes.size(); i_attrib++) {
		glDisableVertexAttribArray(i_attrib);
	}
}
