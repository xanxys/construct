#include "scene.h"

namespace construct {

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



Scene::Scene() : new_id(0), native_script_counter(0) {
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

void Scene::deleteObject(ObjectId target) {
	deletion.push_back(target);
}

void Scene::step() {
	// Native Script expects 30fps
	// Running at 60fps
	// -> load balance with modulo 2 of ObjectId
	for(auto& object : objects) {
		if(object.second->nscript) {
			if(object.first % 2 == native_script_counter) {
				object.second->nscript->step(1.0 / 30, *object.second.get());
			}
		}
	}
	native_script_counter = (native_script_counter + 1) % 2;

	for(ObjectId target : deletion) {
		objects.erase(target);
	}
	deletion.clear();
}

void Scene::render() {
	for(auto& pair : objects) {
		auto& object = pair.second;

		if(object->use_blend) {
			glEnable(GL_BLEND);
			
			// additive blending
			// glBlendFunc(GL_SRC_ALPHA, GL_ONE);

			// alpha blend
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}

		object->shader->use();
		if(object->texture) {
			OVR::Matrix4f m = 
				OVR::Matrix4f::Translation(object->center);

			object->texture->useIn(0);
			object->shader->setUniform("texture", 0);
			object->shader->setUniformMat4("local_to_world", &m.M[0][0]);
		}
		object->geometry->render();

		if(object->use_blend) {
			glDisable(GL_BLEND);
		}
	}
}

}  // namespace
