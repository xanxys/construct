#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <GL/glew.h>
#include <GL/glfw3.h>

#include "OVR.h"

// "Must" for NPR, comfortable rendering:
// * SSAO (uniform lighting and still look good)
// * FSAA (remove edge artifacts)
//
// Pipeline (no per-object "artistic" settings).
// everything is fixed at reasonable parameters.
//
// G-buffer (pos, normal, albedo) -> 

class Object {
public:
	Object(int n_vertex, const float* pos);
	void render();
private:
	const int n_vertex;
	GLuint vertexbuffer;
};



Object::Object(int n_vertex, const float* pos) : n_vertex(n_vertex) {
	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);
	
	glGenBuffers(1, &vertexbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glBufferData(GL_ARRAY_BUFFER, n_vertex * 3 * sizeof(float), pos, GL_STATIC_DRAW);
}

void Object::render() {
	// 1rst attribute buffer : vertices
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glVertexAttribPointer(
		0,                  // attribute 0. No particular reason for 0, but must match the layout in the shader.
		3,                  // size
		GL_FLOAT,           // type
		GL_FALSE,           // normalized?
		0,                  // stride
		(void*)0            // array buffer offset
		);

	// Draw the triangle !
	glDrawArrays(GL_TRIANGLES, 0, n_vertex);

	glDisableVertexAttribArray(0);
}

class Scene {
public:
	Scene();

	void render();
	std::vector<Object> objects;
};

Scene::Scene() {
}

void Scene::render() {
	for(auto& object : objects) {
		object.render();
	}
}



// TODO: program leaks (in OpenGL context).
class Shader {
public:
	Shader();
	Shader(const char* vertex_file_path, const char* fragment_file_path);
	~Shader();

	void setUniform(std::string variable, GLint value);
	void setUniform(std::string variable, float v0);
	void setUniform(std::string variable, float v0, float v1);
	void setUniform(std::string variable, float v0, float v1, float v2, float v3);
	void setUniformMat4(std::string variable, float* pv);

	void use();
protected:
	GLint getVariable(const std::string& variable);
private:
	bool initialized;
	GLuint program;
};

Shader::Shader() : initialized(false) {
}

Shader::~Shader() {
	std::cout << "Destroying shader (not implemented)" << std::endl;
}

Shader::Shader(const char* vertex_file_path, const char* fragment_file_path) : initialized(false) {
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

	// Compile Vertex Shader
	printf("Compiling shader : %s\n", vertex_file_path);
	char const * VertexSourcePointer = VertexShaderCode.c_str();
	glShaderSource(VertexShaderID, 1, &VertexSourcePointer , NULL);
	glCompileShader(VertexShaderID);

	// Check Vertex Shader
	glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> VertexShaderErrorMessage(InfoLogLength);
	glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
	fprintf(stdout, "%s\n", &VertexShaderErrorMessage[0]);

	// Compile Fragment Shader
	printf("Compiling shader : %s\n", fragment_file_path);
	char const * FragmentSourcePointer = FragmentShaderCode.c_str();
	glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer , NULL);
	glCompileShader(FragmentShaderID);

	// Check Fragment Shader
	glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> FragmentShaderErrorMessage(InfoLogLength);
	glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
	fprintf(stdout, "%s\n", &FragmentShaderErrorMessage[0]);

	// Link the program
	fprintf(stdout, "Linking program\n");
	GLuint ProgramID = glCreateProgram();
	glAttachShader(ProgramID, VertexShaderID);
	glAttachShader(ProgramID, FragmentShaderID);
	glLinkProgram(ProgramID);

	// Check the program
	glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
	glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> ProgramErrorMessage( std::max(InfoLogLength, int(1)) );
	glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
	fprintf(stdout, "%s\n", &ProgramErrorMessage[0]);

	glDeleteShader(VertexShaderID);
	glDeleteShader(FragmentShaderID);

	program = ProgramID;
	initialized = true;
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
	if(initialized) {
		glUseProgram(program);
	} else {
		throw "Uninitialized shader";
	}
}


void enableExtensions() {
	GLenum err = glewInit();
	if(GLEW_OK != err) {
		throw glewGetErrorString(err);
	}
}


class Application {
public:
	Application(bool windowed = false);

	// Blocking call to run event loop.
	void run();

protected:
	void init(bool windowed);

	// Update everything, and draw final image to the back buffer.
	void step();

	GLFWmonitor* findHMDMonitor(std::string name, int px, int py);

	std::pair<OVR::Matrix4f, OVR::Matrix4f> calcHMDProjection(float scale);

	void usePreBuffer();
	void useBackBuffer();
	
private:
	GLuint FramebufferName;
	GLuint renderedTexture;

	OVR::HMDInfo hmd;
	Scene scene;

	Shader standard_shader;  // TODO: this is unsafe! (state of initialization is unknown. also copiable)
	Shader warp_shader;

	GLFWwindow* window;
	std::unique_ptr<OVR::SensorFusion> sensor_fusion;
	OVR::Ptr<OVR::SensorDevice> pSensor;

	int screen_width;
	int screen_height;

	int buffer_width;
	int buffer_height;

	std::unique_ptr<Object> proxy;
};

Application::Application(bool windowed) {
	init(windowed);

	// Tiles
	for(int i = -5; i <= 5; i++) {
		for(int j = -5; j <= 5; j++) {
			GLfloat g_vertex_buffer_data[] = {
				-0.45f, -0.45f, 0,
				0.45f, -0.45f, 0,
				0.45f,  0.45f, 0,

				-0.45f, -0.45f, 0,
				0.45f,  0.45f, 0,
				-0.45f, 0.45f, 0,
			};

			for(int k = 0; k < 6; k++) {
				g_vertex_buffer_data[k * 3 + 0] += i;
				g_vertex_buffer_data[k * 3 + 1] += j;
			}

			scene.objects.push_back(Object(6, &g_vertex_buffer_data[0]));
		}
	}
	
}

std::pair<OVR::Matrix4f, OVR::Matrix4f> Application::calcHMDProjection(float scale) {

	// Compute Aspect Ratio. Stereo mode cuts width in half.
	const float aspectRatio = float(hmd.HResolution * 0.5f) / float(hmd.VResolution);

	// Compute Vertical FOV based on distance.
	const float halfScreenDistance = (hmd.VScreenSize / 2);
	const float yfov = 2.0f * atan(scale * halfScreenDistance / hmd.EyeToScreenDistance);

	// Post-projection viewport coordinates range from (-1.0, 1.0), with the
	// center of the left viewport falling at (1/4) of horizontal screen size.
	// We need to shift this projection center to match with the lens center.
	// We compute this shift in physical units (meters) to correct
	// for different screen sizes and then rescale to viewport coordinates.
	const float viewCenter = hmd.HScreenSize * 0.25f;
	const float eyeProjectionShift = viewCenter - hmd.LensSeparationDistance*0.5f;
	const float projectionCenterOffset = 4.0f * eyeProjectionShift / hmd.HScreenSize;

	// Projection matrix for the "center eye", which the left/right matrices are based on.
	OVR::Matrix4f projCenter = OVR::Matrix4f::PerspectiveRH(yfov, aspectRatio, 0.1f, 100.0f);
	OVR::Matrix4f projLeft = OVR::Matrix4f::Translation(projectionCenterOffset, 0, 0) * projCenter;
	OVR::Matrix4f projRight = OVR::Matrix4f::Translation(-projectionCenterOffset, 0, 0) * projCenter;

	// View transformation translation in world units.
	const float halfIPD = hmd.InterpupillaryDistance * 0.5f;
	OVR::Matrix4f viewLeft = OVR::Matrix4f::Translation(halfIPD, 0, 0) * viewCenter;
	OVR::Matrix4f viewRight = OVR::Matrix4f::Translation(-halfIPD, 0, 0) * viewCenter;


	// Get head rotation.
	OVR::Quatf hmdOrient = sensor_fusion->GetOrientation();
	OVR::Matrix4f hmdMat(hmdOrient.Inverted());

	// TODO: i dont know why this is needed
	hmdMat = OVR::Matrix4f::Scaling(1, 1, -1) * hmdMat;

	OVR::Matrix4f world = 
		OVR::Matrix4f::RotationX(OVR::Math<double>::Pi * 0.5) *
		OVR::Matrix4f::Translation(0, 0, -1.4);

	return std::make_pair(
		projLeft * viewLeft * hmdMat * world,
		projRight * viewRight * hmdMat *  world);
}

void Application::usePreBuffer() {
	// Set attachment 0.


	glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName);

	// Use attachment 0.
	std::array<GLenum, 1> buffers = {GL_COLOR_ATTACHMENT0};
	glDrawBuffers(buffers.size(), buffers.data());

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		throw "Failed to set OpenGL frame buffer";
	}
}

void Application::useBackBuffer() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDrawBuffer(GL_BACK);
}

// Try to find HMD monitor. return nullptr when in doubt (or not connected).
GLFWmonitor* Application::findHMDMonitor(std::string name, int px, int py) {
	int count;
	GLFWmonitor** monitors = glfwGetMonitors(&count);

	// Try to find exact name match.
	for(int i = 0; i < count; i++) {
		std::cout << "Checking " << glfwGetMonitorName(monitors[i]) << std::endl;
		if(glfwGetMonitorName(monitors[i]) == name) {
			return monitors[i];
		}
	}

	// Find 7 inch (150mm x 94mm) display. (TODO: replace it with better method)
	for(int i = 0; i < count; i++) {
		int width;
		int height;
		glfwGetMonitorPhysicalSize(monitors[i], &width, &height);
		std::cout << "Checking Size" << width << "," << height << std::endl;
		
		if(width == 150 && height == 94) {
			return monitors[i];
		}
	}

	// Find by global display coordinate origin.
	for(int i = 0; i < count; i++) {
		int mx;
		int my;
		glfwGetMonitorPos(monitors[i], &mx, &my);
		std::cout << "Checking Pos" << mx << "," << my << std::endl;
		
		if(mx == px && my == py) {
			return monitors[i];
		}
	}

	// Find non-primary monitor.
	for(int i = 0; i < count; i++) {
		if(glfwGetPrimaryMonitor() != monitors[i]) {
			return monitors[i];
		}
	}

	// We have no idea.
	return nullptr;
}

void Application::init(bool windowed) {
	OVR::System::Init(OVR::Log::ConfigureDefaultLog(OVR::LogMask_All));
	OVR::Ptr<OVR::DeviceManager> pManager = *OVR::DeviceManager::Create();
	OVR::Ptr<OVR::HMDDevice> pHMD = *pManager->EnumerateDevices<OVR::HMDDevice>().CreateDevice();

	pHMD->GetDeviceInfo(&hmd);


	std::cout << "DisplayName: " << hmd.DisplayDeviceName << " at " << hmd.DesktopX << "," << hmd.DesktopY << std::endl;
	

	if(!glfwInit()) {
		throw "Failed to initialize GLFW";
	}

	if(windowed) {
		screen_width = hmd.HResolution / 2;
		screen_height = hmd.VResolution / 2;
	} else {
		screen_width = hmd.HResolution;
		screen_height = hmd.VResolution;
	}
	buffer_width = screen_width * 2;
	buffer_height  = screen_height * 2;
	
	window = glfwCreateWindow(screen_width, screen_height, "Construct", windowed ? nullptr : findHMDMonitor(hmd.DisplayDeviceName, hmd.DesktopX, hmd.DesktopY), NULL);
	if(!window) {
		glfwTerminate();
		throw "Failed to create GLFW window";
	}

	glfwMakeContextCurrent(window);



	enableExtensions();

	pSensor = *pHMD->GetSensor();

	sensor_fusion.reset(new OVR::SensorFusion());
	if(pSensor) {
		sensor_fusion->AttachToSensor(pSensor);
	}

	// OpenGL things
	FramebufferName = 0;
	glGenFramebuffers(1, &FramebufferName);
	glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName);

	// The texture we're going to render to
	glGenTextures(1, &renderedTexture);

	// "Bind" the newly created texture : all future texture functions will modify this texture
	glBindTexture(GL_TEXTURE_2D, renderedTexture);

	// Give an empty image to OpenGL ( the last "0" )
	glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB, buffer_width, buffer_height, 0,GL_RGB, GL_UNSIGNED_BYTE, 0);

	// Poor filtering. Needed !
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	// The depth buffer
	GLuint depthrenderbuffer;
	glGenRenderbuffers(1, &depthrenderbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthrenderbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, buffer_width, buffer_height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthrenderbuffer);

	// Set "renderedTexture" as our colour attachement #0
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, renderedTexture, 0);

	//
	standard_shader = Shader("base.vs", "base.fs");
	warp_shader = Shader("warp.vs", "warp.fs");
}

void Application::step() {
	// rectangle spanning [-1, 1]^2
	if(!proxy) {
		const GLfloat g_vertex_buffer_data[] = {
			-1.0f, -1.0f, 0,
			1.0f, -1.0f, 0,
			1.0f,  1.0f, 0,

			-1.0f, -1.0f, 0,
			1.0f,  1.0f, 0,
			-1.0f,  1.0f, 0,
		};

		proxy.reset(new Object(6, g_vertex_buffer_data));
	}
	

	const bool use_distortion = true;

	OVR::Util::Render::DistortionConfig distortion(
		hmd.DistortionK[0], hmd.DistortionK[1],
		hmd.DistortionK[2], hmd.DistortionK[3]);

	const float lens_center = 
		1 - 2 * hmd.LensSeparationDistance / hmd.HScreenSize;

	const float scale = distortion.DistortionFn(-1 - lens_center);
	std::cout << "K" << lens_center << " : " << scale << std::endl;

	// Erase all
	if(use_distortion) {
		usePreBuffer();
	} else {
		useBackBuffer();
	}
	glClearColor(0.05, 0, 0.3, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	auto projections = calcHMDProjection(scale);
	const int width = use_distortion ? buffer_width : screen_width;
	const int height = use_distortion ? buffer_height : screen_height;

	// Left eye
	glViewport(0, 0, width / 2, height);
	standard_shader.use();
	standard_shader.setUniformMat4("world_to_screen", &projections.first.M[0][0]);
	scene.render();

	// Right eye
	glViewport(width / 2, 0, width / 2, height);
	standard_shader.use();
	standard_shader.setUniformMat4("world_to_screen", &projections.second.M[0][0]);
	scene.render();

	// Apply warp shader (framebuffer -> back buffer)
	if(use_distortion) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, renderedTexture);

		useBackBuffer();
		warp_shader.use();
		warp_shader.setUniform("diffusion", 1.0f / buffer_height);
		warp_shader.setUniform("Texture0", 0);
		warp_shader.setUniform("HmdWarpParam",
			hmd.DistortionK[0], hmd.DistortionK[1],
			hmd.DistortionK[2], hmd.DistortionK[3]);

		// left
		glViewport(0, 0, screen_width / 2, screen_height);
		warp_shader.setUniform("xoffset", 0.0f);
		warp_shader.setUniform("LensCenter", 0.25, 0.5);
		warp_shader.setUniform("ScreenCenter", 0.25, 0.5);
		warp_shader.setUniform("Scale", 1, 1);
		warp_shader.setUniform("ScaleIn", 1, 1);
		proxy->render();

		// right
		glViewport(screen_width / 2, 0, screen_width / 2, screen_height);
		warp_shader.setUniform("xoffset", 0.5f);
		warp_shader.setUniform("LensCenter", 0.75, 0.5);
		warp_shader.setUniform("ScreenCenter", 0.75, 0.5);
		warp_shader.setUniform("Scale", 1, 1);
		warp_shader.setUniform("ScaleIn", 1, 1);
		proxy->render();
	}
}

void Application::run() {
	try {
		while(!glfwWindowShouldClose(window)) {
			step();
			glfwSwapBuffers(window);
			glfwPollEvents();
		}
		
	} catch(char* exc) {
		std::cout << "Exception: " << exc << std::endl;
	} catch(std::string exc) {
		std::cout << "Exception: " << exc << std::endl;
	} catch(std::exception& exc) {
		std::cout << "Exception: " << exc.what() << std::endl;
	} catch(...) {
		std::cout << "Unknown exception" << std::endl;
	}
	
	glfwTerminate();
}

int main(int argc, char** argv) {
	bool windowed = false;
	if(argc == 2 && std::string(argv[1]) == "--window") {
		windowed = true;
	}

	Application app(windowed);
	app.run();
	

	return 0;
}
