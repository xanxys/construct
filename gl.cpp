#include "gl.h"

std::shared_ptr<Shader> Shader::create(const char* vertex_file_path, const char* fragment_file_path) {
	return std::shared_ptr<Shader>(new Shader(vertex_file_path, fragment_file_path));
}

Shader::~Shader() {
	glDeleteShader(program);
}

Shader::Shader(const char* vertex_file_path, const char* fragment_file_path) {
	// Create the shaders
	GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

	// Read the Vertex Shader code from the file
	std::string VertexShaderCode;
	std::ifstream VertexShaderStream(vertex_file_path, std::ios::in);
	if(VertexShaderStream.is_open())
	{
	    std::string Line = "";
	    while(getline(VertexShaderStream, Line))
	        VertexShaderCode += "\n" + Line;
	    VertexShaderStream.close();
	}

	// Read the Fragment Shader code from the file
	std::string FragmentShaderCode;
	std::ifstream FragmentShaderStream(fragment_file_path, std::ios::in);
	if(FragmentShaderStream.is_open()){
	    std::string Line = "";
	    while(getline(FragmentShaderStream, Line))
	        FragmentShaderCode += "\n" + Line;
	    FragmentShaderStream.close();
	}

	GLint Result = GL_FALSE;
	int InfoLogLength;

	std::cout << "Compiling shader: " << vertex_file_path << " + " << fragment_file_path << std::endl;

	// Compile Vertex Shader
	char const * VertexSourcePointer = VertexShaderCode.c_str();
	glShaderSource(VertexShaderID, 1, &VertexSourcePointer , NULL);
	glCompileShader(VertexShaderID);
	std::cout << getLogFor(VertexShaderID) << std::endl;

	// Compile Fragment Shader
	char const * FragmentSourcePointer = FragmentShaderCode.c_str();
	glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer , NULL);
	glCompileShader(FragmentShaderID);
	std::cout << getLogFor(FragmentShaderID) << std::endl;

	// Link the program
	GLuint ProgramID = glCreateProgram();
	glAttachShader(ProgramID, VertexShaderID);
	glAttachShader(ProgramID, FragmentShaderID);
	glLinkProgram(ProgramID);
	std::cout << getLogFor(ProgramID) << std::endl;

	// Release componenets.
	glDeleteShader(VertexShaderID);
	glDeleteShader(FragmentShaderID);

	program = ProgramID;
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
