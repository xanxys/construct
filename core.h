#pragma once

#include <memory>
#include <string>
#include <vector>

#include <boost/optional.hpp>
#include <cairo/cairo.h>
#include <eigen3/Eigen/Dense>
#include <GL/glew.h>
#include <glfw3.h>

#include "gl.h"
#include "OVR.h"
#include "scene.h"

namespace construct {

// Core (and a lot of code in Scene) assumes fixed 60fps.
// Make it variable when HMD with 60+fps is released.
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

	void addBuilding();

	// Process aspects.
	void step();

	// Update everything, and draw final image to the back buffer.
	void render();

	void printDisplays();
	GLFWmonitor* findHMDMonitor(std::string name, int px, int py);

	void usePreBuffer();
	void useBackBuffer();

	// avatar related
	std::pair<OVR::Matrix4f, OVR::Matrix4f> calcHMDProjection(float scale);
	Eigen::Vector3f getFootPosition();
	Eigen::Vector3f getEyePosition();
	OVR::Vector3f getHeadDirection();

	Eigen::Vector3f getViewUp();
	Eigen::Vector3f getViewRight();
	Eigen::Vector3f getViewCenter();
	void setMovingDirection(Eigen::Vector3f dir);
	float estimateMaxRadiance();

	void adaptEyes();

	// objects
	void attachSky(Object& object);	
	void attachDasherQuadAt(ObjectId widget, ObjectId label, float height, float dx, float dy, float dz);
	void attachTextQuadAt(Object& object, std::string text, float height, float dx, float dy, float dz);
	void attachLocomotionRing(Object& object);
	void attachCursor(Object& object);

	// default orientation is to surface look "normal" to Y- direction, with size
	// [-width/2, width/2] * [0,0] * [-height/2, height/2].
	std::shared_ptr<Geometry> generateTexQuadGeometry(
		float width, float height, Eigen::Vector3f pos, Eigen::Matrix3f rot);

	void attachCuboid(Object& object, Eigen::Vector3f size, Eigen::Vector3f pos, Eigen::Vector3f color = {0.9, 0.8, 0.8});

	std::shared_ptr<Texture> createTextureFromSurface(cairo_surface_t* surface);
private:
	// avatar things.
	float max_luminance;
	OVR::Vector3f eye_position;
	Eigen::Vector3f avatar_foot_pos;
	Eigen::Vector3f avatar_move_dir;

	// GL - Scene things.
	Scene scene;

	// TODO: dependency on these shaders prevents moving attach... methods
	// to ui.cpp
	std::shared_ptr<Shader> standard_shader;
	std::shared_ptr<Shader> texture_shader;
	std::shared_ptr<Shader> warp_shader;

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

	double t_last_update;
};

}  // namespace
