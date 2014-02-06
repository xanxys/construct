#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <boost/optional.hpp>
#include <cairo/cairo.h>
#include <GL/glew.h>
#include <GL/glfw3.h>
#include <v8.h>

#include "OVR.h"
#include "gl.h"

// "Must" for NPR, comfortable rendering:
// * SSAO (uniform lighting and still look good)
// * FSAA (remove edge artifacts)
//
// Pipeline (no per-Geometry "artistic" settings).
// everything is fixed at reasonable parameters.
//
// G-buffer (pos, normal, albedo) -> 


class Object {
public:
	Object();

	bool use_blend;
	std::shared_ptr<Geometry> geometry;
	std::shared_ptr<Shader> shader;

	// optional
	std::shared_ptr<Texture> texture;
};

Object::Object() : use_blend(false) {
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
		if(object.use_blend) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		}

		object.shader->use();
		if(object.texture) {
			object.texture->useIn(0);
			object.shader->setUniform("texture", 0);
		}
		object.geometry->render();

		if(object.use_blend) {
			glDisable(GL_BLEND);
		}
	}
}



class ProbNode {
public:  // common interface
	static std::shared_ptr<ProbNode> create();
	static std::shared_ptr<ProbNode> getParent(std::shared_ptr<ProbNode> node);
	static std::vector<std::pair<float, std::shared_ptr<ProbNode>>> getChildren(std::shared_ptr<ProbNode> node);

	std::string getString();
private:
	ProbNode(std::shared_ptr<ProbNode> parent, std::string str);
private:
	std::string str;
	std::shared_ptr<ProbNode> parent;
};

std::shared_ptr<ProbNode> ProbNode::create() {
	return std::shared_ptr<ProbNode>(new ProbNode(std::shared_ptr<ProbNode>(), ""));
}

std::shared_ptr<ProbNode> ProbNode::getParent(std::shared_ptr<ProbNode> node) {
	return node->parent;
}

std::vector<std::pair<float, std::shared_ptr<ProbNode>>> ProbNode::getChildren(std::shared_ptr<ProbNode> node) {
	std::vector<std::pair<float, std::shared_ptr<ProbNode>>> children;

	std::string alphabet = "abcdefghijklmnopqrstuvwxyz ";
	for(char ch : alphabet) {
		children.push_back(std::make_pair(
			1.0 / alphabet.size(),
			std::shared_ptr<ProbNode>(new ProbNode(node, std::string(1, ch)))));
	}

	return children;
}

ProbNode::ProbNode(std::shared_ptr<ProbNode> parent, std::string str) : parent(parent), str(str) {
}

std::string ProbNode::getString() {
	return str;
}



class Dasher {
public:
	Dasher();
	std::string getFixed();

	// rel_index, rel_zoom: [-1, 1]
	void update(float dt, float rel_index, float rel_zoom);

	void visualize(cairo_t* ctx);
private:
	// Adjust current (and clip values if needed) so that invariance will hold.
	void fit();
public:
	// invariance: [local_index - local_half_span, local_index + local_half_span] is contained in [0, 1]
	std::shared_ptr<ProbNode> current;
	float local_index;
	float local_half_span;
};

Dasher::Dasher() {
	current = ProbNode::create();
	local_index = 0;
	local_half_span = 0.5;
	fit();
}

void Dasher::update(float dt, float rel_index, float rel_zoom) {
	const float speed = 1.0;

	local_half_span += dt * speed * rel_zoom;
	local_index += dt * speed * (rel_index * local_half_span);
	fit();
}

void Dasher::fit() {
	const float p0 = local_index - local_half_span;
	const float p1 = local_index + local_half_span;
	
	// Span doesn't fit in currrent node?
	if(p0 < 0 || p1 > 1) {
		auto parent = ProbNode::getParent(current);

		if(parent) {
			// convert coordinates to parent's.
			float accum_p = 0;
			for(auto& child : ProbNode::getChildren(parent)) {
				if(child.second->getString() == current->getString()) {
					// [0, 1] -> [accum_p, accum_p + child.first]
					local_index = accum_p + child.first * local_index;
					local_half_span = child.first * local_half_span;
					current = parent;
					fit();
					break;
				}
				accum_p += child.first;
			}
		} else {
			// clip, preserving span (this is somewhat arbitrary UI choice)
			if(p0 < 0 && 1 < p1) {
				local_index = 0.5;
				local_half_span = 0.5;
			} else if(p0 < 0) {
				local_index = local_half_span;
			} else if(1 < p1) {
				local_index = 1 - local_half_span;
			}
		}
	} else {
		// Span can fit completely in a child?
		float accum_p = 0;
		for(auto& child : ProbNode::getChildren(current)) {
			if(accum_p <= p0 && p1 < accum_p + child.first) {
				local_index = (local_index - accum_p) / child.first;
				local_half_span = local_half_span / child.first;
				current = child.second;
				fit();
				break;
			}
			accum_p += child.first;
		}
	}
}

std::string Dasher::getFixed() {
	std::string result;
	std::shared_ptr<ProbNode> iter = current;
	while(iter) {
		result = iter->getString() + result;
		iter = ProbNode::getParent(iter);
	}
	return result;
}

void Dasher::visualize(cairo_t* ctx) {
	cairo_save(ctx);
	cairo_scale(ctx, 250, 250);

	float accum_p = 0;
	int color_type = 0;
	for(auto& child : ProbNode::getChildren(current)) {
		if(color_type == 0) {
			cairo_set_source_rgb(ctx, 0.8, 0.8, 0.9);
		} else {
			cairo_set_source_rgb(ctx, 0.7, 0.7, 0.8);
		}
		
		cairo_rectangle(ctx, 0, accum_p, 1, child.first);
		cairo_fill(ctx);

		accum_p += child.first;
		color_type = (color_type + 1) % 2;
	}

	cairo_restore(ctx);
}


class Application {
public:
	Application(bool windowed = false);

	// Blocking call to run event loop.
	void run();

protected:
	void enableExtensions();
	void init(bool windowed);

	// Update everything, and draw final image to the back buffer.
	void step();

	GLFWmonitor* findHMDMonitor(std::string name, int px, int py);

	std::pair<OVR::Matrix4f, OVR::Matrix4f> calcHMDProjection(float scale);

	void usePreBuffer();
	void useBackBuffer();
	
	Object generateDasherQuadAt(float height, float dx, float dy, float dz);
	Object generateTextQuadAt(std::string text, float height, float dx, float dy, float dz);
	std::shared_ptr<Texture> createTextureFromSurface(cairo_surface_t* surface);
private:  // TODO: decouple members
	// Scene - dasher things.
	Dasher dasher;

	// GL - Scene things.
	Scene scene;

	std::shared_ptr<Shader> standard_shader;
	std::shared_ptr<Shader> texture_shader;
	std::shared_ptr<Shader> warp_shader;

	OVR::Vector3f eye_position;

	// OVR-GL things.
	GLuint FramebufferName;

	OVR::HMDInfo hmd;

	GLFWwindow* window;
	std::unique_ptr<OVR::SensorFusion> sensor_fusion;
	OVR::Ptr<OVR::SensorDevice> pSensor;

	int screen_width;
	int screen_height;

	int buffer_width;
	int buffer_height;

	std::shared_ptr<Geometry> proxy;
	std::shared_ptr<Texture> pre_buffer;
};


Application::Application(bool windowed) {
	init(windowed);
	v8::V8::Initialize();

	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handle_scope;

	
	// Create a template for the global Geometry.
	v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();

	
	// TODO: attach native functions to global Geometry.
	v8::Handle<v8::Context> context = v8::Context::New(nullptr, global); // nullptr, nullptr, global);
	v8::Context::Scope context_scope(context);
	
	v8::Handle<v8::Script> script = v8::Script::Compile(v8::String::New("1+2;"));
	v8::Handle<v8::Value> result = script->Run();
	
	v8::String::AsciiValue ascii(result);

	std::cout << "V8 says: " << *ascii << std::endl;

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

			Object obj;
			obj.shader = standard_shader;
			obj.geometry = Geometry::createPos(6, &g_vertex_buffer_data[0]);
			scene.objects.push_back(obj);
		}
	}

	scene.objects.push_back(generateTextQuadAt("Construct", 0.15, 0, 1, 1.3));
	scene.objects.push_back(generateTextQuadAt("はろーわーるど", 0.1, 0, 1, 1));
	scene.objects.push_back(generateDasherQuadAt(0.5, 0, 0.9, 0.5));

	eye_position = OVR::Vector3f(0, 0, 1.4);
}


Object Application::generateDasherQuadAt(float height_meter, float dx, float dy, float dz) {
	const float aspect_estimate = 1.0;
	const float px_per_meter = 500;

	const float width_meter = height_meter * aspect_estimate;

	// Create texture with string.
	const int width_px = px_per_meter * width_meter;
	const int height_px = px_per_meter * height_meter;

	auto surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width_px, height_px);
	auto c_context = cairo_create(surf);
	dasher.visualize(c_context);
	cairo_destroy(c_context);
	auto texture = createTextureFromSurface(surf);
	cairo_surface_destroy(surf);

	// Create geometry with texture.
	GLfloat vertex_pos_uv[] = {
		-1.0f, 0, -1.0f, 0, 1,
		-1.0f, 0, 1.0f, 0, 0,
		 1.0f, 0, 1.0f, 1, 0,

		-1.0f, 0, -1.0f, 0, 1,
		 1.0f, 0, 1.0f, 1, 0,
		 1.0f, 0, -1.0f, 1, 1,
	};
	for(int i = 0; i < 6; i++) {
		vertex_pos_uv[5 * i + 0] = dx + vertex_pos_uv[5 * i + 0] * 0.5 * width_meter;
		vertex_pos_uv[5 * i + 1] = dy;
		vertex_pos_uv[5 * i + 2] = dz + vertex_pos_uv[5 * i + 2] * 0.5 * height_meter;
	}

	Object obj;
	obj.shader = texture_shader;
	obj.geometry = Geometry::createPosUV(6, vertex_pos_uv);
	obj.texture = texture;
	obj.use_blend = true;

	return obj;
}


Object Application::generateTextQuadAt(std::string text, float height_meter, float dx, float dy, float dz) {
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
	cairo_surface_destroy(surf);

	std::cout << "ISize:" << width_px << " * " << height_px << std::endl;

	// Create geometry with texture.
	GLfloat vertex_pos_uv[] = {
		-1.0f, 0, -1.0f, 0, 1,
		-1.0f, 0, 1.0f, 0, 0,
		 1.0f, 0, 1.0f, 1, 0,

		-1.0f, 0, -1.0f, 0, 1,
		 1.0f, 0, 1.0f, 1, 0,
		 1.0f, 0, -1.0f, 1, 1,
	};
	for(int i = 0; i < 6; i++) {
		vertex_pos_uv[5 * i + 0] = dx + vertex_pos_uv[5 * i + 0] * 0.5 * width_meter;
		vertex_pos_uv[5 * i + 1] = dy;
		vertex_pos_uv[5 * i + 2] = dz + vertex_pos_uv[5 * i + 2] * 0.5 * height_meter;
	}

	Object obj;
	obj.shader = texture_shader;
	obj.geometry = Geometry::createPosUV(6, vertex_pos_uv);
	obj.texture = texture;
	obj.use_blend = true;

	return obj;
}

std::shared_ptr<Texture> Application::createTextureFromSurface(cairo_surface_t* surface) {
	// Convert cairo format to GL format.
	GLint gl_format;
	const cairo_format_t format = cairo_image_surface_get_format(surface);
	if(format == CAIRO_FORMAT_ARGB32) {
		gl_format = GL_RGBA;
	} else if(format == CAIRO_FORMAT_RGB24) {
		gl_format = GL_RGB;
	} else {
		throw "Unsupported surface type";
	}

	// Create texture
	const int width = cairo_image_surface_get_width(surface);
	const int height = cairo_image_surface_get_height(surface);

	auto texture = Texture::create(width, height);
	texture->useIn();
	glTexImage2D(GL_TEXTURE_2D, 0, gl_format, width, height, 0, gl_format, GL_UNSIGNED_BYTE, cairo_image_surface_get_data(surface));

	return texture;
}


void Application::enableExtensions() {
	GLenum err = glewInit();
	if(GLEW_OK != err) {
		throw glewGetErrorString(err);
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

	OVR::Matrix4f world = 
		OVR::Matrix4f::RotationX(-OVR::Math<double>::Pi * 0.5) *
		OVR::Matrix4f::Translation(-eye_position);

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

	pre_buffer = Texture::create(buffer_width, buffer_height);

	// The depth buffer
	GLuint depthrenderbuffer;
	glGenRenderbuffers(1, &depthrenderbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthrenderbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, buffer_width, buffer_height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthrenderbuffer);

	// Set "renderedTexture" as our colour attachement #0
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, pre_buffer->unsafeGetId(), 0);

	//
	standard_shader = Shader::create("base.vs", "base.fs");
	texture_shader = Shader::create("tex.vs", "tex.fs");
	warp_shader = Shader::create("warp.vs", "warp.fs");
}

void Application::step() {
	// rectangle spanning [-1, 1]^2
	if(!proxy) {
		const GLfloat vertex_pos[] = {
			-1.0f, -1.0f, 0,
			1.0f, -1.0f, 0,
			1.0f,  1.0f, 0,

			-1.0f, -1.0f, 0,
			1.0f,  1.0f, 0,
			-1.0f,  1.0f, 0,
		};

		proxy = Geometry::createPos(6, vertex_pos);
	}
	

	const bool use_distortion = true;

	OVR::Util::Render::DistortionConfig distortion(
		hmd.DistortionK[0], hmd.DistortionK[1],
		hmd.DistortionK[2], hmd.DistortionK[3]);

	const float lens_center = 
		1 - 2 * hmd.LensSeparationDistance / hmd.HScreenSize;

	const float scale = 0.9;

	// Erase all
	if(use_distortion) {
		usePreBuffer();
	} else {
		useBackBuffer();
	}
	glClearColor(0.05, 0, 0.3, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	auto projections = calcHMDProjection(1 / scale);
	const int width = use_distortion ? buffer_width : screen_width;
	const int height = use_distortion ? buffer_height : screen_height;

	// Left eye
	glViewport(0, 0, width / 2, height);
	standard_shader->use();
	standard_shader->setUniformMat4("world_to_screen", &projections.first.M[0][0]);
	texture_shader->use();
	texture_shader->setUniformMat4("world_to_screen", &projections.first.M[0][0]);
	scene.render();

	// Right eye
	glViewport(width / 2, 0, width / 2, height);
	standard_shader->use();
	standard_shader->setUniformMat4("world_to_screen", &projections.second.M[0][0]);
	texture_shader->use();
	texture_shader->setUniformMat4("world_to_screen", &projections.second.M[0][0]);
	scene.render();

	// Apply warp shader (framebuffer -> back buffer)
	if(use_distortion) {
		pre_buffer->useIn(0);

		useBackBuffer();
		warp_shader->use();
		warp_shader->setUniform("diffusion", 1.0f / buffer_height);
		warp_shader->setUniform("Texture0", 0);
		warp_shader->setUniform("HmdWarpParam",
			hmd.DistortionK[0], hmd.DistortionK[1],
			hmd.DistortionK[2], hmd.DistortionK[3]);
		warp_shader->setUniform("Scale", 0.5f * scale, 0.5f * scale);
		warp_shader->setUniform("ScaleIn", 2.0f, 2.0f);

		// left
		glViewport(0, 0, screen_width / 2, screen_height);
		warp_shader->setUniform("xoffset", 0.0f);
		warp_shader->setUniform("LensCenter", 0.25 + lens_center / 2, 0.5);
		warp_shader->setUniform("ScreenCenter", 0.25, 0.5);
		proxy->render();

		// right
		glViewport(screen_width / 2, 0, screen_width / 2, screen_height);
		warp_shader->setUniform("xoffset", 0.5f);
		warp_shader->setUniform("LensCenter", 0.75 - lens_center / 2, 0.5);
		warp_shader->setUniform("ScreenCenter", 0.75, 0.5);
		
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
