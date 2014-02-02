#include <vector>

#include <GL/glfw3.h>

#include "OVR.h"


class Object {
public:
	void render();
};

void Object::render() {
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glRotatef((float) glfwGetTime() * 50.f, 0.f, 0.f, 1.f);

	glBegin(GL_TRIANGLES);
	glColor3f(1.f, 0.f, 0.f);
	glVertex3f(-0.6f, -0.4f, 0.f);
	glColor3f(0.f, 1.f, 0.f);
	glVertex3f(0.6f, -0.4f, 0.f);
	glColor3f(0.f, 0.f, 1.f);
	glVertex3f(0.f, 0.6f, 0.f);
	glEnd();
}

class Scene {
public:
	void render();
	std::vector<Object> objects;
};

void Scene::render() {
	for(auto& object : objects) {
		object.render();
	}
}

void renderScene(float ratio) {
	Scene scene;
	scene.objects.push_back(Object());

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-ratio, ratio, -1.f, 1.f, 1.f, -1.f);

	scene.render();
}

class Application {
public:
	Application();

	// Blocking call to run event loop.
	void run();

	void setupHMD();
private:
	OVR::HMDInfo hmd;
};

Application::Application() {
	OVR::System::Init(OVR::Log::ConfigureDefaultLog(OVR::LogMask_All));
	
	OVR::Ptr<OVR::DeviceManager> pManager = *OVR::DeviceManager::Create();
	OVR::Ptr<OVR::HMDDevice> pHMD = *pManager->EnumerateDevices<OVR::HMDDevice>().CreateDevice();

	pHMD->GetDeviceInfo(&hmd);
}

void Application::setupHMD() {
	// Compute Aspect Ratio. Stereo mode cuts width in half.
	const float aspectRatio = float(hmd.HResolution * 0.5f) / float(hmd.VResolution);

	// Compute Vertical FOV based on distance.
	const float halfScreenDistance = (hmd.VScreenSize / 2);
	const float yfov = 2.0f * atan(halfScreenDistance / hmd.EyeToScreenDistance);

	// Post-projection viewport coordinates range from (-1.0, 1.0), with the
	// center of the left viewport falling at (1/4) of horizontal screen size.
	// We need to shift this projection center to match with the lens center.
	// We compute this shift in physical units (meters) to correct
	// for different screen sizes and then rescale to viewport coordinates.
	const float viewCenter = hmd.HScreenSize * 0.25f;
	const float eyeProjectionShift = viewCenter - hmd.LensSeparationDistance*0.5f;
	const float projectionCenterOffset = 4.0f * eyeProjectionShift / hmd.HScreenSize;

	// Projection matrix for the "center eye", which the left/right matrices are based on.
	OVR::Matrix4f projCenter = OVR::Matrix4f::PerspectiveRH(yfov, aspectRatio, 0.3f, 1000.0f);
	OVR::Matrix4f projLeft = OVR::Matrix4f::Translation(projectionCenterOffset, 0, 0) * projCenter;
	OVR::Matrix4f projRight = OVR::Matrix4f::Translation(-projectionCenterOffset, 0, 0) * projCenter;

	// View transformation translation in world units.
	const float halfIPD = hmd.InterpupillaryDistance * 0.5f;
	OVR::Matrix4f viewLeft = OVR::Matrix4f::Translation(halfIPD, 0, 0) * viewCenter;
	OVR::Matrix4f viewRight = OVR::Matrix4f::Translation(-halfIPD, 0, 0) * viewCenter;
}

void Application::run() {
	if(!glfwInit()) {
		throw "Failed to initialize GLFW";
	}

	GLFWwindow* window = glfwCreateWindow(640, 400, "Construct", NULL, NULL);
	if(!window) {
		glfwTerminate();
		throw "Failed to create GLFW window";
	}

	/* Make the window's context current */
	glfwMakeContextCurrent(window);

	/* Loop until the user closes the window */
	while(!glfwWindowShouldClose(window)) {
		/* Render here */
		float ratio;
		int width, height;
		// glfwGetFramebufferSize(window, &width, &height);
		width = 640;
		height = 400;

		ratio = width / (float) height;

		// Erase all
		glClear(GL_COLOR_BUFFER_BIT);

		// Left eye
		glViewport(0, 0, width / 2, height);
		renderScene(ratio / 2);

		// Right eye
		glViewport(width / 2, 0, width / 2, height);
		renderScene(ratio / 2);

		/* Swap front and back buffers */
		glfwSwapBuffers(window);

		/* Poll for and process events */
		glfwPollEvents();
	}

	glfwTerminate();
}

int main() {
	Application app;
	app.run();

	return 0;
}
