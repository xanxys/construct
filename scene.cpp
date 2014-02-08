#include "scene.h"



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




NativeScript::NativeScript() {
}

void NativeScript::step(float dt, Object& object) {
}



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
