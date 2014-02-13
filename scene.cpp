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


Ray::Ray(Eigen::Vector3f org, Eigen::Vector3f dir) : org(org), dir(dir) {
}


Triangle::Triangle(Eigen::Vector3f p0, Eigen::Vector3f p1, Eigen::Vector3f p2) :
	p0(p0), d1(p1 - p0), d2(p2 - p0), normal(d1.cross(d2).normalized()) {
	reflectance = Eigen::Vector3f(0.8, 0.8, 0.9);
}

boost::optional<Intersection> Triangle::intersect(Ray ray) {
	Eigen::Vector3f s1 = ray.dir.cross(d2);
	const float div = s1.dot(d1);
	if(div <= 0) {  // parallel or back
		return boost::optional<Intersection>();
	}

	const float div_inv = 1 / div;
		
	Eigen::Vector3f s  = ray.org - p0;
	const float a = s.dot(s1) * div_inv;
	if(a < 0 || a > 1) {
		return boost::optional<Intersection>();
	}
				
	Eigen::Vector3f s2 = s.cross(d1);
	float b = ray.dir.dot(s2) * div_inv;
		
	if(b < 0 || a + b > 1) {
		return boost::optional<Intersection>();
	}

	const float t = d2.dot(s2) * div_inv;
	if(t < 0) {
		return boost::optional<Intersection>();
	}

	return boost::optional<Intersection>(std::make_tuple(
		t, ray.org + ray.dir * t, normal));
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
