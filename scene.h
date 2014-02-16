#pragma once

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <boost/optional.hpp>
#include <eigen3/Eigen/Dense>
#include <json/json.h>
#include <GL/glew.h>
#include <glfw3.h>

#include "gl.h"
#include "light.h"
#include "OVR.h"
#include "scene.h"
#include "sky.h"
#include "util.h"

namespace construct {

typedef uint64_t ObjectId;

class NativeScript;
class Scene;

template<class Generator>
	Eigen::Vector3f sample_hemisphere(Generator& g, Eigen::Vector3f n) {

	auto scalar = std::uniform_real_distribution<float>(-1, 1);
	while(true) {
		Eigen::Vector3f v(scalar(g), scalar(g), scalar(g));
		const float length = v.norm();

		// Reject vectors outside unit sphere
		if(length > 1) {
			continue;
		}

		v /= length;
		if(v.dot(n) > 0) {
			return v;
		} else {
			return -v;
		}
	}
}

enum ObjectType {
	UI,
	STATIC,
	SKY,
};

// There are two kinds of objects:
// * static (base shader): once it's put, it can't be moved freely
//  (we don't yet have enough resource to make everything look good, freely movable,
// and not optimize it)
// * UI (tex shader): can move freely, with almost no physics.
class Object {
public:
	Object(Scene& scene);

	OVR::Vector3f center;

	bool use_blend;

	// These details should not belong to Object. Instead,
	// they are generated as needed in lighting etc.
	std::shared_ptr<Geometry> geometry;
	std::shared_ptr<Shader> shader;

	// optional
	std::shared_ptr<Texture> texture;


	std::unique_ptr<NativeScript> nscript;

	void addMessage(Json::Value value);
	boost::optional<Json::Value> getMessage();

	Scene& scene;

	ObjectType type;
private:
	std::vector<Json::Value> queue;
};


class NativeScript {
public:
	NativeScript();
	virtual void step(float dt, Object& object);
};


// Rendering equation for surfaces:
// radiance(pos, dir) = radiance_emit(pos, dir) + 
//   integral(brdf(pos, dir, dir_in) * radiance(pos, -dir_in) * normal(pos).dot(dir_in)
//      for dir_in in all_directions)
//
// By assuming Lambertian surface (brdf = reflectance / pi),
// we can cache a single irradiance for pos to calculate outgoing radiance for any dir.
// (by the way, by using sum-of-product form, other BRDFs can be used. this is called separable BRDF)
//
// We can cache every part of scene this way.
//
// And, SUPER important thing is, the calculation order of nested ingeral doesn't matter.
// (although this is not proven).
//
// This is like on-the-fly radiosity (NOT instant radiosity, nor photon mapping).
// it's more similar to voxel cone tracing.
//
// lighting pass treats scene as triangle soup.
class Scene {
public:
	Scene();

	ObjectId add();
	Object& unsafeGet(ObjectId);

	void step();
	void render();
	
	void sendMessage(ObjectId destination, Json::Value value);
	void deleteObject(ObjectId target);

	void updateGeometry();

	std::shared_ptr<Texture> getBackgroundImage();

	Colorf getRadiance(Ray ray);

	// Return (pos, normal) of the intersection. Targets are
	// STATIC and UI.
	boost::optional<std::pair<Eigen::Vector3f, Eigen::Vector3f>>
		intersectAny(Ray ray);
private:
	// TODO: Current process is tangled. Fix it.
	// ideal:
	//   Object.triangles ->
	//   Scene.Geometry
	// now:
	//   Object.Geometry -(updateGeometry)->
	//   Scene.triangles -(updateLighting)->
	//   Scene.triangles -(updateIrradiance)->
	//   Object.Geometry
	
	void updateLighting();
	void updateIrradiance();
	
	boost::optional<Intersection> intersect(Ray ray);

	// Approximate integral(irradiance(pos, -dir_in) * normal(pos).dot(dir_in) for dir_in in sphere)
	// return value: radiance
	Colorf collectIrradiance(Eigen::Vector3f pos, Eigen::Vector3f normal);

	std::mt19937 random;
	int lighting_counter;
private:
	Sky sky;
	
	// geometry
	std::vector<Triangle> tris;


	// nodes
	std::map<ObjectId, std::unique_ptr<Object>> objects;

	std::vector<ObjectId> deletion;
	ObjectId new_id;

	int native_script_counter;
};

}  // namespace
