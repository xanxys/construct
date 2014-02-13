#include "scene.h"

namespace construct {

const float pi = 3.14159265359;

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
	p0(p0), d1(p1 - p0), d2(p2 - p0), normal(d1.cross(d2).normalized()),
	ir0({0, 0, 0}), ir1({0, 0, 0}), ir2({0, 0, 0}) {
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
		t, ray.org + ray.dir * t, normal,
		(1 - a - b) * ir0 + a * ir1 + b * ir2));
}

Eigen::Vector3f Triangle::getVertexPos(int i) {
	assert(0 <= i && i < 3);
	if(i == 0) {
		return p0;
	} else if(i == 1) {
		return p0 + d1;
	} else {
		return p0 + d2;
	}
}

Eigen::Vector3f Triangle::getNormal() {
	return normal;
}

Colorf Triangle::brdf() {
	return reflectance / pi;
}


// For diffuse-like surface, luminance = candela / 2pi
// overcast sky = (200, 200, 220)
Scene::Scene() : new_id(0), native_script_counter(0), lighting_counter(0),
	background(200, 200, 220) {
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
	updateLighting();
	updateIrradiance();
}

void Scene::updateGeometry() {
	tris.clear();
	tris.reserve(objects.size());

	for(auto& pair : objects) {
		// Ignore UI elements.
		if(pair.second->texture) {
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

Colorf Scene::collectIrradiance(Eigen::Vector3f pos, Eigen::Vector3f normal) {
	const int n_samples = 5;
	
	Colorf accum(0, 0, 0);
	for(int i = 0; i < n_samples; i++) {
		auto dir = sample_hemisphere(random, normal);
		Ray ray(pos + normal * 1e-5, dir);
		auto isect = intersect(ray);

		if(isect) {
			accum += std::get<3>(*isect) * normal.dot(dir);
		} else {
			accum += background * normal.dot(dir);
		}
	}
	return accum / n_samples;
}

Colorf Scene::getBackground() {
	return background;
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
