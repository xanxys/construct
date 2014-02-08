#include <array>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <boost/optional.hpp>
#include <cairo/cairo.h>
#include <jsoncpp/json/json.h>
#include <GL/glew.h>
#include <GL/glfw3.h>
#include <v8.h>

#include "dasher.h"
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

// In-VR interactions:
// Provide text input that always works. (dasher)
// But avoid using text.

// All objects are isolated from each other;
// Object doesn't have address of other objects.
//
// Object-object interaction is limited to:
// 1 geometric neighbor access
// 2 forking itself
// 3 sending json object via id
//
// 1 and 2 should be used primarily, and 3 "very" sparingly:
// to send message from object A to distant object B,
// it's almost always better to visualize the message as another object
// and move it in the world.
//
// This way, everything is controllable from inside, by default.
// In contrast, if 3 (or direct method call) is used mainly,
// it can't be accessed safely from the inside because of
// fear of infinite loop and such (see smalltalk).

class NativeScript;

class Scene;

class Object {
public:
	Object(Scene& scene);

	OVR::Vector3f center;

	bool use_blend;
	std::shared_ptr<Geometry> geometry;
	std::shared_ptr<Shader> shader;

	// optional
	std::shared_ptr<Texture> texture;


	std::unique_ptr<NativeScript> nscript;

	void addMessage(Json::Value value);
	boost::optional<Json::Value> getMessage();

	Scene& scene;
private:
	std::vector<Json::Value> queue;
};

Object::Object(Scene& scene) : scene(scene), use_blend(false) {
}

void Object::addMessage(Json::Value value) {
	queue.push_back(value);
}

boost::optional<Json::Value> Object::getMessage() {
	if(queue.empty()) {
		return boost::optional<Json::Value>();
	} else {
		auto elem = queue.back();
		queue.pop_back();
		return boost::optional<Json::Value>(elem);
	}
}


class NativeScript {
public:
	NativeScript();
	virtual void step(float dt, Object& object);
};

NativeScript::NativeScript() {
}

void NativeScript::step(float dt, Object& object) {
}

typedef uint64_t ObjectId;

class Scene {
public:
	Scene();

	ObjectId add();
	Object& unsafeGet(ObjectId);

	void render();
	std::map<ObjectId, std::unique_ptr<Object>> objects;

	void sendMessage(ObjectId destination, Json::Value value);
private:
	ObjectId new_id;
};

Scene::Scene() {
}

ObjectId Scene::add() {
	const ObjectId id = new_id++;
	objects[id] = std::unique_ptr<Object>(new Object(*this));
	return id;
}

Object& Scene::unsafeGet(ObjectId id) {
	return *objects[id].get();
}

void Scene::sendMessage(ObjectId destination, Json::Value value) {
	auto it = objects.find(destination);
	if(it != objects.end()) {
		it->second->addMessage(value);
	}
}

void Scene::render() {
	for(auto& pair : objects) {
		auto& object = pair.second;

		if(object->use_blend) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		}

		object->shader->use();
		if(object->texture) {
			object->texture->useIn(0);
			object->shader->setUniform("texture", 0);
		}
		object->geometry->render();

		if(object->use_blend) {
			glDisable(GL_BLEND);
		}
	}
}

class DasherScript : public NativeScript {
public:
	DasherScript(
		std::function<OVR::Vector3f()> getHeadDirection,
		cairo_surface_t* surface, ObjectId label);
	~DasherScript();

	void step(float dt, Object& object) override;
private:
	Dasher dasher;
	ObjectId label;

	// TODO: unsafe reference to Core. Remove.
	std::function<OVR::Vector3f()> getHeadDirection;

	cairo_surface_t* dasher_surface;
};

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


class TextLabelScript : public NativeScript {
public:
	TextLabelScript(cairo_surface_t* surface);
	~TextLabelScript();

	void step(float dt, Object& object) override;
private:
	cairo_surface_t* surface;
};

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


class Core {
public:
	Core(bool windowed = false);

	// Blocking call to run event loop.
	void run();
protected:
	enum DisplayMode {
		// Smaller window in whatever screen. Useful for debugging.
		WINDOW,

		// True fullscreen mode. Might show up on wrong screen and
		// cause resolution-change mess. Avoid it. Slightly faster.
		HMD_FULLSCREEN,

		// Decoration-less window on top of rift screen. Works well, but
		// strange window manager might cause distortion of window.
		HMD_FRAMELESS,
	};

	void enableExtensions();
	void init(DisplayMode mode);
	void addInitialObjects();

	// Process aspects.
	void step();

	// Update everything, and draw final image to the back buffer.
	void render();

	GLFWmonitor* findHMDMonitor(std::string name, int px, int py);

	std::pair<OVR::Matrix4f, OVR::Matrix4f> calcHMDProjection(float scale);
	OVR::Vector3f getHeadDirection();

	void usePreBuffer();
	void useBackBuffer();

	void printDisplays();
	
	void attachDasherQuadAt(Object& object, ObjectId label, float height, float dx, float dy, float dz);
	void attachTextQuadAt(Object& object, std::string text, float height, float dx, float dy, float dz);

	std::shared_ptr<Texture> createTextureFromSurface(cairo_surface_t* surface);
private:
	// GL - Scene things.
	Scene scene;

	std::shared_ptr<Shader> standard_shader;
	std::shared_ptr<Shader> texture_shader;
	std::shared_ptr<Shader> warp_shader;

	OVR::Vector3f eye_position;

	int native_script_counter;

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


Core::Core(bool windowed) : native_script_counter(0) {
	init(windowed ? DisplayMode::WINDOW : DisplayMode::HMD_FRAMELESS);
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

	addInitialObjects();
}

void Core::addInitialObjects() {
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

		 	auto& obj = scene.unsafeGet(scene.add());
			obj.shader = standard_shader;
			obj.geometry = Geometry::createPos(6, &g_vertex_buffer_data[0]);
		}
	}


	attachTextQuadAt(scene.unsafeGet(scene.add()), "はろーわーるど", 0.1, 0, 1, 1.8);

	ObjectId input_object = scene.add();
	attachTextQuadAt(scene.unsafeGet(input_object), "Construct", 0.15, 0, 1, 1.0);	

	attachDasherQuadAt(scene.unsafeGet(scene.add()), input_object, 0.5, 0, 0.9, 1.4);

	eye_position = OVR::Vector3f(0, 0, 1.4);
}

void Core::attachDasherQuadAt(Object& object, ObjectId label, float height_meter, float dx, float dy, float dz) {
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

	object.shader = texture_shader;
	object.geometry = Geometry::createPosUV(6, vertex_pos_uv);
	object.texture = texture;
	object.use_blend = true;
	object.nscript.reset(new DasherScript(
		std::bind(std::mem_fn(&Core::getHeadDirection), this), dasher_surface, label));
}

void Core::attachTextQuadAt(Object& object, std::string text, float height_meter, float dx, float dy, float dz) {
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

	object.shader = texture_shader;
	object.geometry = Geometry::createPosUV(6, vertex_pos_uv);
	object.texture = texture;
	object.use_blend = true;
	object.nscript.reset(new TextLabelScript(surf));
}

std::shared_ptr<Texture> Core::createTextureFromSurface(cairo_surface_t* surface) {
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


void Core::enableExtensions() {
	GLenum err = glewInit();
	if(GLEW_OK != err) {
		throw glewGetErrorString(err);
	}
}

std::pair<OVR::Matrix4f, OVR::Matrix4f> Core::calcHMDProjection(float scale) {
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

OVR::Vector3f Core::getHeadDirection() {
	OVR::Quatf hmdOrient = sensor_fusion->GetOrientation();
	OVR::Matrix4f hmdMat(hmdOrient);

	OVR::Matrix4f ovr_to_world = OVR::Matrix4f::RotationX(OVR::Math<double>::Pi * 0.5);

	return (ovr_to_world * hmdMat).Transform(OVR::Vector3f(0, 0, -1));
}

void Core::usePreBuffer() {
	// Set attachment 0.
	glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName);

	// Use attachment 0.
	std::array<GLenum, 1> buffers = {GL_COLOR_ATTACHMENT0};
	glDrawBuffers(buffers.size(), buffers.data());

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		throw "Failed to set OpenGL frame buffer";
	}
}

void Core::useBackBuffer() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDrawBuffer(GL_BACK);
}

void Core::printDisplays() {
	int count;
	GLFWmonitor** monitors = glfwGetMonitors(&count);

	for(int i = 0; i < count; i++) {
		auto monitor = monitors[i];

		int width, height;
		glfwGetMonitorPhysicalSize(monitor, &width, &height);

		int px, py;
		glfwGetMonitorPos(monitor, &px, &py);

		std::cout <<
			glfwGetMonitorName(monitor) << " : " <<
			"size(mm) = (" << width << "," << height << ")" << " : " <<
			"pos(px) = (" << px << "," << py << ")" << std::endl;

		int mode_count;
		const GLFWvidmode* modes = glfwGetVideoModes(monitor, &mode_count);
		for(int i_mode = 0; i_mode < mode_count; i_mode++) {
			std::cout << "* " << "(" << modes[i_mode].width << "," << modes[i_mode].height << ")" << std::endl;
		}
	}
}

// Try to find HMD monitor. return nullptr when in doubt (or not connected).
GLFWmonitor* Core::findHMDMonitor(std::string name, int px, int py) {
	int count;
	GLFWmonitor** monitors = glfwGetMonitors(&count);
	return monitors[1];

	// Try to find exact name match.
	for(int i = 0; i < count; i++) {
		if(glfwGetMonitorName(monitors[i]) == name) {
			return monitors[i];
		}
	}

	// Find 7 inch (150mm x 94mm) display. (TODO: replace it with better method)
	for(int i = 0; i < count; i++) {
		int width;
		int height;
		glfwGetMonitorPhysicalSize(monitors[i], &width, &height);
		
		if(width == 150 && height == 94) {
			return monitors[i];
		}
	}

	// Find by global display coordinate origin.
	for(int i = 0; i < count; i++) {
		int mx;
		int my;
		glfwGetMonitorPos(monitors[i], &mx, &my);
		
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

void Core::init(DisplayMode mode) {
	bool use_true_fullscreen = false;

	OVR::System::Init(OVR::Log::ConfigureDefaultLog(OVR::LogMask_All));
	OVR::Ptr<OVR::DeviceManager> pManager = *OVR::DeviceManager::Create();
	OVR::Ptr<OVR::HMDDevice> pHMD = *pManager->EnumerateDevices<OVR::HMDDevice>().CreateDevice();

	pHMD->GetDeviceInfo(&hmd);
	std::cout << "DisplayName: " << hmd.DisplayDeviceName << " at " << hmd.DesktopX << "," << hmd.DesktopY << std::endl;
	

	if(!glfwInit()) {
		throw "Failed to initialize GLFW";
	}

	printDisplays();

	if(mode == DisplayMode::WINDOW) {
		screen_width = hmd.HResolution / 2;
		screen_height = hmd.VResolution / 2;

		window = glfwCreateWindow(screen_width, screen_height, "Construct", nullptr, nullptr);
	} else if(mode == DisplayMode::HMD_FULLSCREEN) {
		screen_width = hmd.HResolution;
		screen_height = hmd.VResolution;

		window = glfwCreateWindow(screen_width, screen_height, "Construct", findHMDMonitor(hmd.DisplayDeviceName, hmd.DesktopX, hmd.DesktopY), nullptr);
	} else if(mode == DisplayMode::HMD_FRAMELESS) {
		screen_width = hmd.HResolution;
		screen_height = hmd.VResolution;

		glfwWindowHint(GLFW_DECORATED, 0);
		glfwWindowHint(GLFW_VISIBLE, 0);

		auto monitor_hmd = findHMDMonitor(hmd.DisplayDeviceName, hmd.DesktopX, hmd.DesktopY);
		window = glfwCreateWindow(screen_width, screen_height, "Construct", nullptr, nullptr);
		
		if(!monitor_hmd || !window) {
			// TODO: implement proper error handling
		}

		int px, py;
		glfwGetMonitorPos(monitor_hmd, &px, &py);
		glfwSetWindowPos(window, px, py);
		glfwShowWindow(window);
	}
	buffer_width = screen_width * 2;
	buffer_height  = screen_height * 2;	
	
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

void Core::step() {
	// Native Script expects 30fps
	// Running at 60fps
	// -> load balance with modulo 2 of ObjectId
	for(auto& object : scene.objects) {
		if(object.second->nscript) {
			if(object.first % 2 == native_script_counter) {
				object.second->nscript->step(1.0 / 60, *object.second.get());
			}
		}
	}
	native_script_counter = (native_script_counter + 1) % 2;
}

void Core::render() {
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

void Core::run() {
	try {
		while(!glfwWindowShouldClose(window)) {
			step();
			render();
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

	Core core(windowed);
	core.run();

	return 0;
}
