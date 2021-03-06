#include "core.h"

#include <array>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <random>

#include <json/json.h>
#include <v8.h>

#include "sky.h"
#include "ui.h"
#include "util.h"

namespace construct {

Eigen::Vector3f ovrToEigen(OVR::Vector3f v) {
	return Eigen::Vector3f(v.x, v.y, v.z);
}

Eigen::Vector3f projectSphere(float theta, float phi) {
	return Eigen::Vector3f(
		std::sin(theta) * std::cos(phi),
		std::sin(theta) * std::sin(phi),
		std::cos(theta));
}

Core::Core(bool windowed) :
	avatar_foot_pos(Eigen::Vector3f::Zero()),
	avatar_move_dir(Eigen::Vector3f::UnitY()),
	max_luminance(150),
	t_last_update(0) {

	init(windowed ? DisplayMode::WINDOW : DisplayMode::HMD_FRAMELESS);
	v8::V8::Initialize();

	/*
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
	*/

	// Create scene
	scene.reset(new Scene());

	addInitialObjects();
	scene->updateGeometry();
}

void Core::addInitialObjects() {
	attachSky(scene->unsafeGet(scene->add()));

	addBuilding();

	// Prepare special UIs.
	attachLocomotionRing(scene->unsafeGet(scene->add()));
	attachCursor(scene->unsafeGet(scene->add()));

	attachUserMenu(scene->unsafeGet(scene->add()))
		.setLocalToWorld(Transform3f(Eigen::Translation<float, 3>(
			Eigen::Vector3f(-0.8, 1, 1.5))));

	// Prepare example UIs.
	attachTextQuadAt(scene->unsafeGet(scene->add()), "Input    ", 0.1, 0, 0, 0)
		.setLocalToWorld(Transform3f(Eigen::Translation<float, 3>(
			Eigen::Vector3f(0, 1, 1.8))));

	attachTextQuadAt(scene->unsafeGet(scene->add()), "------------------------", 0.12, 0, 0, 0)
		.setLocalToWorld(Transform3f(Eigen::Translation<float, 3>(
			Eigen::Vector3f(0, 1, 1.0))));	
}

Object& Core::attachCursor(Object& object) {
	cairo_surface_t* cursor_surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 50, 50);
	auto c_context = cairo_create(cursor_surface);
	cairo_set_source_rgb(c_context, 1, 0, 0);
	cairo_arc(c_context, 25, 25, 20, 0, 2 * pi);
	cairo_set_line_width(c_context, 3);
	cairo_stroke(c_context);
	cairo_destroy(c_context);
	auto texture = createTextureFromSurface(cursor_surface);

	object.type = ObjectType::UI_CURSOR;

	Eigen::Matrix3f rot;
	rot = Eigen::AngleAxisf(-0.5 * pi, Eigen::Vector3f::UnitX());
	object.geometry = generateTexQuadGeometry(0.1, 0.1,
		Eigen::Vector3f::Zero(), rot);
	object.texture = texture;
	object.use_blend = true;
	
	object.nscript.reset(new CursorScript(
		std::bind(std::mem_fn(&Core::getViewCenter), this),
		std::bind(std::mem_fn(&Core::getEyePosition), this),
		cursor_surface));

	return object;
}

Object& Core::attachUserMenu(Object& object) {
	cairo_surface_t* surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 250, 500);

	object.type = ObjectType::UI;
	object.geometry = generateTexQuadGeometry(0.4, 0.8,
		Eigen::Vector3f::Zero(), Eigen::Matrix3f::Identity());
	object.texture = createTextureFromSurface(surface);
	object.use_blend = false;
	object.nscript.reset(new UserMenuScript(
		std::bind(std::mem_fn(&Core::getStat), this),
		surface));

	return object;
}

Eigen::Vector3f Core::getFootPosition() {
	return avatar_foot_pos;
}

Eigen::Vector3f Core::getEyePosition() {
	return avatar_foot_pos + Eigen::Vector3f(0, 0, 1.4);
}

Eigen::Vector3f Core::getViewCenter() {
	return ovrToEigen(getHeadDirection());
}

float Core::estimateMaxRadiance() {
	const auto eye_pos = getEyePosition();
	const auto view_center = getViewCenter();
	const auto view_r = getViewRight();
	const auto view_u = getViewUp();

	std::vector<float> radiances;
	for(int i = -5; i < 6; i++) {
		for (int j = -5; j < 6; j++) {
			Eigen::Vector3f sample_dir =
				view_center +
				view_u * i / 5.0 +
				view_r * j / 8.0;
			sample_dir.normalize();

			radiances.push_back(
				scene->getRadiance(Ray(eye_pos, sample_dir)).norm());
		}
	}
	std::sort(radiances.begin(), radiances.end());

	const int ix_b = static_cast<int>(radiances.size() * 0.75);
	float acc = 0;
	for(int i = ix_b; i < radiances.size(); i++) {
		acc += radiances[i];
	}
	return acc / (radiances.size() - ix_b);
}

void Core::adaptEyes() {
	// pupillary reflex takes about 250ms to complete.
	// http://www.faa.gov/data_research/research/med_humanfacs/oamtechreports/1960s/media/AM65-25.pdf
	const float latency = 0.25;
	const float frame_count = 60 * latency;

	const float lum = std::max(0.01f, estimateMaxRadiance());

	// Blend ratio s.t. 90% complete is achieved with specified latency.
	// in log space!
	const float ratio = 1 - std::pow(0.1, 1.0 / frame_count);
	max_luminance = std::exp((1 - ratio) * std::log(max_luminance) + ratio * std::log(lum));
}

// Architectural concept: modernized middle-age
// (lots of symmetry, few colors, geometric shapes, semi-open)
void Core::addBuilding() {
	std::mt19937 random;

	// Tiles
	for(int z = 0; z < 2; z++) {
		for(int i = -8; i <= 8; i++) {
			for(int j = -8; j <= 8; j++) {
				const bool parity = (i + j + 10000) % 2 == 0;
				const float refl = parity ?
					std::normal_distribution<float>(0.9, 0.01)(random) :
					std::normal_distribution<float>(0.8, 0.01)(random);

				attachCuboid(scene->unsafeGet(scene->add()),
					Eigen::Vector3f(0.45, 0.45, 0.04),
					Eigen::Vector3f(i * 0.5, j * 0.5, -0.02 + z * 4),
					Eigen::Vector3f(refl, refl, refl));
			}
		}

		attachCuboid(scene->unsafeGet(scene->add()),
			Eigen::Vector3f(8, 8, 0.04),
			Eigen::Vector3f(0, 0, -0.06 + z * 4),
			Eigen::Vector3f(0.8, 0.8, 0.8));
	}

	// pillars
	for(int dx = -1; dx < 1; dx++) {
		for(int dy = -1; dy < 1; dy++) {
			const float spacing = 5;
			const float height = 4;

			attachCuboid(scene->unsafeGet(scene->add()),
				Eigen::Vector3f(0.5, 0.5, height),
				Eigen::Vector3f((dx + 0.5) * spacing, (dy + 0.5) * spacing, height / 2),
				Eigen::Vector3f(0.7, 0.7, 0.7));
		}
	}

	// Generate chairs
	for(int i = 0; i < 8; i++) {
		const float height = std::normal_distribution<float>(0.45, 0.1)(random);

		const float px = std::normal_distribution<float>(0, 4)(random);
		const float py = std::normal_distribution<float>(0, 4)(random);

		// pillar
		attachCuboid(scene->unsafeGet(scene->add()),
			Eigen::Vector3f(0.08, 0.08, height),
			Eigen::Vector3f(px, py, height * 0.5));

		// seat
		attachCuboid(scene->unsafeGet(scene->add()),
			Eigen::Vector3f(0.25, 0.25, 0.07),
			Eigen::Vector3f(px, py, height));
	}

	
	// stairs
	for(int i = 0; i < 40; i++) {
		attachCuboid(scene->unsafeGet(scene->add()),
			Eigen::Vector3f(1, 0.2, 0.2),
			Eigen::Vector3f(1.5, 1.5 + 0.2 * i, 0.1 + 0.2 * i));
	}

	// add elevator
	attachCuboid(scene->unsafeGet(scene->add()),
			Eigen::Vector3f(1.8, 1.8, 8),
			Eigen::Vector3f(0, -4, 0),
			Eigen::Vector3f(0.5, 0.5, 0.5));
	
}

void Core::attachCuboid(Object& object,
	Eigen::Vector3f size, Eigen::Vector3f pos, Eigen::Vector3f color) {

	Eigen::Matrix<float, 6 * 6, 3 + 3, Eigen::RowMajor> vertex;
	for(int i = 0; i < 3; i++) {
		Eigen::Vector3f d(0, 0, 0);
		Eigen::Vector3f e0(0, 0, 0);
		Eigen::Vector3f e1(0, 0, 0);

		d[i] = 0.5;
		e0[(i + 1) % 3] = 0.5;
		e1[(i + 2) % 3] = 0.5;

		for(int side = 0; side < 2; side++) {
			const int face_offset = 6 * (i * 2 + side);

			vertex.row(face_offset + 0).head(3) = d - e0 - e1;
			vertex.row(face_offset + 1).head(3) = d + e0 - e1;
			vertex.row(face_offset + 2).head(3) = d - e0 + e1;

			vertex.row(face_offset + 3).head(3) = d + e0 + e1;
			vertex.row(face_offset + 4).head(3) = d - e0 + e1;
			vertex.row(face_offset + 5).head(3) = d + e0 - e1;

			d *= -1;
			e0 *= -1;
		}
	}

	for(int i = 0; i < 36; i++) {
		Eigen::Vector3f pre_v = vertex.row(i).head(3);
		vertex.row(i).head(3) = pre_v.cwiseProduct(size) + pos;
		vertex.row(i).tail(3) = color;
	}

	object.type = ObjectType::STATIC;
	object.geometry = Geometry::createPosColor(vertex.rows(), vertex.data());
}



void Core::attachSky(Object& object) {
	// UV sphere for Equirectangular mapping.
	const int n_vert = 25;
	const int n_horz = n_vert * 2;
	Eigen::Matrix<float, n_vert * n_horz * 6, 3 + 2, Eigen::RowMajor> vertex;
	for(int y = 0; y < n_vert; y++) {
		const float theta0 = static_cast<float>(y) / n_vert * pi;
		const float theta1 = static_cast<float>(y + 1) / n_vert * pi;

		for(int x = 0; x < n_horz; x++) {
			const float phi0 = static_cast<float>(x) / n_vert * pi;
			const float phi1 = static_cast<float>(x + 1) / n_vert * pi;

			// n.b. we're looking from inside.
			vertex.row(6 * (y * n_horz + x) + 0).head(3) = projectSphere(theta0, phi0) * 500;
			vertex.row(6 * (y * n_horz + x) + 1).head(3) = projectSphere(theta0, phi1) * 500;
			vertex.row(6 * (y * n_horz + x) + 2).head(3) = projectSphere(theta1, phi0) * 500;

			vertex.row(6 * (y * n_horz + x) + 3).head(3) = projectSphere(theta1, phi1) * 500;
			vertex.row(6 * (y * n_horz + x) + 4).head(3) = projectSphere(theta1, phi0) * 500;
			vertex.row(6 * (y * n_horz + x) + 5).head(3) = projectSphere(theta0, phi1) * 500;

			vertex.row(6 * (y * n_horz + x) + 0).tail(2) = Eigen::Vector2f(phi0 / (2 * pi), theta0 / pi);
			vertex.row(6 * (y * n_horz + x) + 1).tail(2) = Eigen::Vector2f(phi1 / (2 * pi), theta0 / pi);
			vertex.row(6 * (y * n_horz + x) + 2).tail(2) = Eigen::Vector2f(phi0 / (2 * pi), theta1 / pi);

			vertex.row(6 * (y * n_horz + x) + 3).tail(2) = Eigen::Vector2f(phi1 / (2 * pi), theta1 / pi);
			vertex.row(6 * (y * n_horz + x) + 4).tail(2) = Eigen::Vector2f(phi0 / (2 * pi), theta1 / pi);
			vertex.row(6 * (y * n_horz + x) + 5).tail(2) = Eigen::Vector2f(phi1 / (2 * pi), theta0 / pi);
		}
	}

	object.geometry = Geometry::createPosUV(vertex.rows(), vertex.data());

	// Create HDR texture
	object.texture = scene->getBackgroundImage();
	object.type = ObjectType::SKY;
}

Object& Core::attachLocomotionRing(Object& object) {
	// Maybe we need to adjust size etc. depending on distance to obstacles.

	cairo_surface_t* locomotion_surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 100, 100);
	auto c_context = cairo_create(locomotion_surface);
	cairo_set_source_rgb(c_context, 1, 1, 1);
	cairo_paint(c_context);
	cairo_destroy(c_context);
	auto texture = createTextureFromSurface(locomotion_surface);

	object.type = ObjectType::UI;
	Eigen::Matrix3f rot;
	rot = Eigen::AngleAxisf(-0.5 * pi, Eigen::Vector3f::UnitX());
	object.geometry = generateTexQuadGeometry(0.9, 0.4,
		Eigen::Vector3f(0, 2.5, 0.05), rot);
	object.texture = texture;
	object.nscript.reset(new LocomotionScript(
		std::bind(std::mem_fn(&Core::getHeadDirection), this),
		std::bind(std::mem_fn(&Core::getEyePosition), this),
		std::bind(std::mem_fn(&Core::setMovingDirection), this, std::placeholders::_1),
		locomotion_surface));
	object.use_blend = false;

	return object;
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
	OVR::Matrix4f projCenter = OVR::Matrix4f::PerspectiveRH(yfov, aspectRatio, 0.1f, 1000.0f);
	OVR::Matrix4f projLeft = OVR::Matrix4f::Translation(projectionCenterOffset, 0, 0) * projCenter;
	OVR::Matrix4f projRight = OVR::Matrix4f::Translation(-projectionCenterOffset, 0, 0) * projCenter;

	// View transformation translation in world units.
	const float halfIPD = hmd.InterpupillaryDistance * 0.5f;
	OVR::Matrix4f viewLeft = OVR::Matrix4f::Translation(halfIPD, 0, 0) * viewCenter;
	OVR::Matrix4f viewRight = OVR::Matrix4f::Translation(-halfIPD, 0, 0) * viewCenter;


	// Get head rotation.
	OVR::Quatf hmdOrient = sensor_fusion->GetOrientation();
	OVR::Matrix4f hmdMat(hmdOrient.Inverted());

	auto eye_position = getEyePosition();

	OVR::Matrix4f world = 
		OVR::Matrix4f::RotationX(-OVR::Math<double>::Pi * 0.5) *
		OVR::Matrix4f::Translation(
			-eye_position.x(), -eye_position.y(), -eye_position.z());

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

Eigen::Vector3f Core::getViewUp() {
	OVR::Quatf hmdOrient = sensor_fusion->GetOrientation();
	OVR::Matrix4f hmdMat(hmdOrient);
	OVR::Matrix4f ovr_to_world = OVR::Matrix4f::RotationX(OVR::Math<double>::Pi * 0.5);

	return ovrToEigen((ovr_to_world * hmdMat).Transform(OVR::Vector3f(0, 1, 0)));
}

Eigen::Vector3f Core::getViewRight() {
	OVR::Quatf hmdOrient = sensor_fusion->GetOrientation();
	OVR::Matrix4f hmdMat(hmdOrient);
	OVR::Matrix4f ovr_to_world = OVR::Matrix4f::RotationX(OVR::Math<double>::Pi * 0.5);

	return ovrToEigen((ovr_to_world * hmdMat).Transform(OVR::Vector3f(1, 0, 0)));
}

void Core::setMovingDirection(Eigen::Vector3f dir) {
	avatar_move_dir = dir;
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

	pre_buffer = Texture::create(buffer_width, buffer_height, true);

	// The depth buffer
	GLuint depthrenderbuffer;
	glGenRenderbuffers(1, &depthrenderbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthrenderbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, buffer_width, buffer_height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthrenderbuffer);

	// Set "renderedTexture" as our colour attachement #0
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, pre_buffer->unsafeGetId(), 0);

	//
	warp_shader = Shader::create("gpu/warp.vs", "gpu/warp.fs");
}

void Core::step() {
	// 1.4 m/s is recommended in oculus best practice guide.
	avatar_foot_pos += avatar_move_dir * 1.4 * (1.0 / 60);
	avatar_foot_pos.z() = 0;

	adaptEyes();

	scene->step();
}

Json::Value Core::getStat() {
	Json::Value stat;
	stat["uptime"] = glfwGetTime();
	return stat;
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
	glClearColor(0, 0, 0, 1);
	glClearDepth(1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

	auto projections = calcHMDProjection(1 / scale);
	const int width = use_distortion ? buffer_width : screen_width;
	const int height = use_distortion ? buffer_height : screen_height;

	// Left eye
	glViewport(0, 0, width / 2, height);
	scene->render(&projections.first.M[0][0]);

	// Right eye
	glViewport(width / 2, 0, width / 2, height);
	scene->render(&projections.second.M[0][0]);

	// Apply warp shader (framebuffer -> back buffer)
	if(use_distortion) {
		pre_buffer->useIn(0);

		glDisable(GL_DEPTH_TEST);

		useBackBuffer();
		warp_shader->use();
		warp_shader->setUniform("diffusion", 1.0f / buffer_height);
		warp_shader->setUniform("Texture0", 0);
		warp_shader->setUniform("HmdWarpParam",
			hmd.DistortionK[0], hmd.DistortionK[1],
			hmd.DistortionK[2], hmd.DistortionK[3]);
		warp_shader->setUniform("Scale", 0.5f * scale, 0.5f * scale);
		warp_shader->setUniform("ScaleIn", 2.0f, 2.0f);
		// Measured oculus gamma = 2.3
		// (by using an image at
		//  http://www.eizo.co.jp/eizolibrary/other/itmedia02_07/)
		warp_shader->setUniform("hmd_gamma", 2.3f);
		warp_shader->setUniform("max_luminance", max_luminance);

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
			const double step_t0 = glfwGetTime();
			step();
			const double step_dt = glfwGetTime() - step_t0;
			if(step_dt > 1.0 / 60) {
				std::cout << "Warn: too much time in step()" << step_dt << std::endl;
			}

			render();
			auto error = glGetError();
			if(error != GL_NO_ERROR) {
				std::cout << "error: OpenGL error: " << error << std::endl;
			}

			glfwSwapBuffers(window);
			const double t = glfwGetTime();
			const double dt = t - t_last_update;
			if(dt > 1.5 / 60) {
				std::cout << "Missed frame: latency=" << dt << " sec" <<std::endl;
			}
			t_last_update = t;

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

}  // namespace
