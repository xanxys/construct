#pragma once

#include <memory>
#include <string>
#include <vector>

#include <cairo/cairo.h>

class ProbNode {
public:  // common interface
	static std::shared_ptr<ProbNode> create();
	static std::shared_ptr<ProbNode> getParent(std::shared_ptr<ProbNode> node);
	static std::vector<std::pair<float, std::shared_ptr<ProbNode>>> getChildren(std::shared_ptr<ProbNode> node);

	std::string getString();
private:
	ProbNode(std::shared_ptr<ProbNode> parent, std::string str);
private:
	std::string str;
	std::shared_ptr<ProbNode> parent;
};


class Dasher {
public:
	Dasher();
	std::string getFixed();

	// rel_index, rel_zoom: [-1, 1]
	void update(float dt, float rel_index, float rel_zoom);

	void visualize(cairo_t* ctx);
private:
	// Adjust current (and clip values if needed) so that invariance will hold.
	void fit();
public:
	// invariance: [local_index - local_half_span, local_index + local_half_span] is contained in [0, 1]
	std::shared_ptr<ProbNode> current;
	float local_index;
	float local_half_span;
};