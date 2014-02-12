#pragma once

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <boost/optional.hpp>
#include <jsoncpp/json/json.h>
#include <GL/glew.h>
#include <GL/glfw3.h>

#include "gl.h"
#include "OVR.h"
#include "scene.h"

typedef uint64_t ObjectId;

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


class NativeScript {
public:
	NativeScript();
	virtual void step(float dt, Object& object);
};

class Scene {
public:
	Scene();

	ObjectId add();
	Object& unsafeGet(ObjectId);

	void step();
	void render();
	
	void sendMessage(ObjectId destination, Json::Value value);
	void deleteObject(ObjectId target);
private:
	std::map<ObjectId, std::unique_ptr<Object>> objects;

	std::vector<ObjectId> deletion;
	ObjectId new_id;

	int native_script_counter;
};
