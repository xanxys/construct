#include "scene.h"

namespace construct {

Object::Object(Scene& scene) : scene(scene), use_blend(false),
	local_to_world(Transform3f::Identity()) {
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

void Object::setLocalToWorld(Transform3f trans) {
	local_to_world = trans;
}

Transform3f Object::getLocalToWorld() {
	if(type != ObjectType::UI && type != UI_CURSOR) {
		throw "Non UI component doesn't have tranform";
	}

	return local_to_world;
}


NativeScript::NativeScript() {
}

void NativeScript::step(float dt, Object& object) {
}


// For diffuse-like surface, luminance = candela / 2pi
// overcast sky = (200, 200, 220)
Scene::Scene() : new_id(0), native_script_counter(0), lighting_counter(0) {
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

boost::optional<std::pair<Eigen::Vector3f, Eigen::Vector3f>>
	Scene::intersectAny(Ray ray) {

	// Intersect UI elements.
	auto isect_static = intersect(ray);
	auto isect_ui = intersectUI(ray);

	boost::optional<Intersection> isect;
	if(isect_ui && isect_static) {
		if(std::get<0>(*isect_ui) < std::get<0>(*isect_static)) {
			isect = isect_ui;
		} else {
			isect = isect_static;
		}
	} else if(isect_ui) {
		isect = isect_ui;
	} else if(isect_static) {
		isect = isect_static;
	}

	// Convert.
	if(isect) {
		return boost::optional<
			std::pair<Eigen::Vector3f, Eigen::Vector3f>>(
				std::make_pair(std::get<1>(*isect), std::get<2>(*isect)));
	} else {
		return boost::optional<
			std::pair<Eigen::Vector3f, Eigen::Vector3f>>();
	}
}

boost::optional<Intersection> Scene::intersectUI(Ray ray) {
	boost::optional<Intersection> isect_nearest;
	for(auto& tri : tris_ui) {
		auto isect = tri.intersect(ray);
		if(isect && (!isect_nearest || std::get<0>(*isect) < std::get<0>(*isect_nearest))) {
			isect_nearest = isect;
		}
	}
	return isect_nearest;
}

boost::optional<Intersection> Scene::intersect(Ray ray) {
	boost::optional<Intersection> isect_nearest;
	for(auto& tri : tris) {
		auto isect = tri.intersect(ray);
		if(isect && (!isect_nearest || std::get<0>(*isect) < std::get<0>(*isect_nearest))) {
			isect_nearest = isect;
		}
	}
	return isect_nearest;
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

	//updateGeometry();
	updateUIGeometry();
	updateLighting();
	updateIrradiance();
}

void Scene::updateGeometry() {
	tris.clear();
	tris.reserve(objects.size());
	for(auto& pair : objects) {
		if(pair.second->type != ObjectType::STATIC) {
			continue;
		}

		// Extract tris from PosCol format.
		auto& data = pair.second->geometry->getData();
		for(int i = 0; i < data.size() / (6 * 3); i++) {
			std::array<Eigen::Vector3f, 3> vertex;
			for(int j = 0; j < 3; j++) {
				vertex[j] = Eigen::Vector3f(
					data[6 * (3 * i + j) + 0],
					data[6 * (3 * i + j) + 1],
					data[6 * (3 * i + j) + 2]);
			}
			tris.push_back(Triangle(vertex[0], vertex[1], vertex[2]));
		}
	}
}

void Scene::updateUIGeometry() {
	tris_ui.clear();
	for(auto& pair : objects) {
		if(pair.second->type != ObjectType::UI) {
			continue;
		}

		auto trans = pair.second->getLocalToWorld();

		// Extract tris from PosUV format.
		auto& data = pair.second->geometry->getData();
		for(int i = 0; i < data.size() / (5 * 3); i++) {
			std::array<Eigen::Vector3f, 3> vertex;
			for(int j = 0; j < 3; j++) {
				vertex[j] = trans * Eigen::Vector3f(
					data[5 * (3 * i + j) + 0],
					data[5 * (3 * i + j) + 1],
					data[5 * (3 * i + j) + 2]);
				//std::cout << "UI vert:" << vertex[j] << std::endl;
			}
			tris_ui.push_back(Triangle(vertex[0], vertex[1], vertex[2]));
		}
	}
}

void Scene::updateLighting() {
	// TODO: use proper multi-threading.
	const int max_tris = 20;
	
	// assuming more than 5 samples.
	const float blend_rate = 0.5;

	for(int i = 0; i < max_tris; i++) {
		auto& tri = tris[lighting_counter];

		tri.ir0 *= (1 - blend_rate);
		tri.ir1 *= (1 - blend_rate);
		tri.ir2 *= (1 - blend_rate);

		tri.ir0 += blend_rate * collectIrradiance(tri.getVertexPos(0), tri.getNormal())
			.cwiseProduct(tri.brdf());
		tri.ir1 += blend_rate * collectIrradiance(tri.getVertexPos(1), tri.getNormal())
			.cwiseProduct(tri.brdf());
		tri.ir2 += blend_rate * collectIrradiance(tri.getVertexPos(2), tri.getNormal())
			.cwiseProduct(tri.brdf());

		lighting_counter += 1;
		lighting_counter %= tris.size();
	}

	// tris = concat(geometry).
	// Since most objects are cuboid, same face should exist just after
	// current tri.
	//
	// TODO: lift this assumption.
	for(int i = 0; i < tris.size() - 1; i++) {
		if((tris[i].getVertexPos(1) - tris[i + 1].getVertexPos(2)).norm() < 1e-3 &&
			(tris[i].getNormal() - tris[i + 1].getNormal()).norm() < 1e-3) {
			// (i, 1) - (i + 1, 2)
			// (i, 2) - (i + 1, 1)
			const Colorf vx = (tris[i].ir1 + tris[i + 1].ir2) / 2;
			const Colorf vy = (tris[i].ir2 + tris[i + 1].ir1) / 2;

			tris[i].ir1 = vx;
			tris[i + 1].ir2 = vx;
			tris[i].ir2 = vy;
			tris[i + 1].ir1 = vy;
		}
	}
}

void Scene::updateIrradiance() {
	auto it = tris.begin();

	for(auto& pair : objects) {
		auto& object = pair.second;

		if(!object->texture) {
			auto& data = object->geometry->getData();

			// assume pos + col format
			for(int i = 0; i < data.size() / (6 * 3); i++) {
				for(int j = 0; j < 3; j++) {
					Colorf ir;
					if(j == 0) {
						ir = it->ir0;
					} else if(j == 1) {
						ir = it->ir1;
					} else {
						ir = it->ir2;
					}

					data[6 * (3 * i + j) + 3] = ir[0];
					data[6 * (3 * i + j) + 4] = ir[1];
					data[6 * (3 * i + j) + 5] = ir[2];
				}

				it++;
			}

			object->geometry->notifyDataChange();
		}
	}

	// # of tris and total Geometry must match
	assert(it == tris.end());
}

Colorf Scene::getRadiance(Ray ray) {
	auto isect = intersect(ray);

	return isect ?
		std::get<3>(*isect) :
		sky.getRadianceAt(ray.dir);
}

Colorf Scene::collectIrradiance(Eigen::Vector3f pos, Eigen::Vector3f normal) {
	const int n_samples = 5;
	
	Colorf accum(0, 0, 0);
	for(int i = 0; i < n_samples; i++) {
		auto dir = sample_hemisphere(random, normal);
		Ray ray(pos + normal * 1e-5, dir);
		accum += getRadiance(ray) * normal.dot(dir);
	}
	return accum / n_samples;
}

std::shared_ptr<Texture> Scene::getBackgroundImage() {
	return sky.generateEquirectangular();
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
		if(object->type == ObjectType::UI || object->type == UI_CURSOR) {
			Eigen::Matrix<float, 4, 4, Eigen::RowMajor> m =
				object->getLocalToWorld().matrix();

			object->texture->useIn(0);
			object->shader->setUniform("texture", 0);
			object->shader->setUniform("luminance", 25.0f);
			object->shader->setUniformMat4("local_to_world", m.data());
		} else if(object->type == ObjectType::SKY) {
			Eigen::Matrix<float, 4, 4, Eigen::RowMajor> m =
				Eigen::Matrix4f::Identity();
			object->texture->useIn(0);
			object->shader->setUniform("texture", 0);
			object->shader->setUniform("luminance", 1.0f);
			object->shader->setUniformMat4("local_to_world", m.data());
		}
		object->geometry->render();

		if(object->use_blend) {
			glDisable(GL_BLEND);
		}
	}
}

}  // namespace
