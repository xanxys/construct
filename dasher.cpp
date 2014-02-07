#include "dasher.h"

#include <cassert>
#include <iostream>

std::shared_ptr<ProbNode> ProbNode::create() {
	return std::shared_ptr<ProbNode>(new ProbNode(std::shared_ptr<ProbNode>(), ""));
}

std::shared_ptr<ProbNode> ProbNode::getParent(std::shared_ptr<ProbNode> node) {
	return node->parent;
}

std::vector<std::pair<float, std::shared_ptr<ProbNode>>> ProbNode::getChildren(std::shared_ptr<ProbNode> node) {
	std::vector<std::pair<float, std::shared_ptr<ProbNode>>> children;

	std::string alphabet = "abcdefghijklmnopqrstuvwxyz ";
	for(char ch : alphabet) {
		children.push_back(std::make_pair(
			(ch == 'e' ? 2.0 : 1.0) / (alphabet.size() + 1),
			std::shared_ptr<ProbNode>(new ProbNode(node, std::string(1, ch)))));
	}

	return children;
}

ProbNode::ProbNode(std::shared_ptr<ProbNode> parent, std::string str) : parent(parent), str(str) {
}

std::string ProbNode::getString() {
	return str;
}


Dasher::Dasher() {
	current = ProbNode::create();
	local_index = 0;
	local_half_span = 0.5;
	fit();
}

void Dasher::update(float dt, float rel_index, float rel_zoom) {
	const float speed = 1.0;

	local_half_span = std::max(local_half_span / 2, local_half_span + dt * speed * rel_zoom);
	local_index += dt * speed * (rel_index * local_half_span);
	fit();

	// std::cout << local_index << "+-" << local_half_span << std::endl;
}

void Dasher::fit() {
	assert(local_half_span > 0);
	const float p0 = local_index - local_half_span;
	const float p1 = local_index + local_half_span;
	
	// Span doesn't fit in currrent node?
	if(p0 < 0 || p1 > 1) {
		auto parent = ProbNode::getParent(current);

		if(parent) {
			// convert coordinates to parent's.
			float accum_p = 0;
			for(auto& child : ProbNode::getChildren(parent)) {
				if(child.second->getString() == current->getString()) {
					// [0, 1] -> [accum_p, accum_p + child.first]
					local_index = accum_p + child.first * local_index;
					local_half_span = child.first * local_half_span;
					current = parent;
					fit();
					break;
				}
				accum_p += child.first;
			}
		} else {
			// clip, preserving span (this is somewhat arbitrary UI choice)
			if(p0 < 0 && 1 < p1) {
				local_index = 0.5;
				local_half_span = 0.5;
			} else if(p0 < 0) {
				local_index = local_half_span;
			} else if(1 < p1) {
				local_index = 1 - local_half_span;
			}
		}
	} else {
		// Span can fit completely in a child?
		float accum_p = 0;
		for(auto& child : ProbNode::getChildren(current)) {
			if(accum_p <= p0 && p1 < accum_p + child.first) {
				local_index = (local_index - accum_p) / child.first;
				local_half_span = local_half_span / child.first;
				current = child.second;
				fit();
				break;
			}
			accum_p += child.first;
		}
	}
}

std::string Dasher::getFixed() {
	std::string result;
	std::shared_ptr<ProbNode> iter = current;
	while(iter) {
		result = iter->getString() + result;
		iter = ProbNode::getParent(iter);
	}
	return result;
}

void Dasher::visualize(cairo_t* ctx) {
	cairo_save(ctx);
	cairo_scale(ctx, 250, 250);

	cairo_set_source_rgb(ctx, 1, 1, 1);
	cairo_new_path(ctx);
	cairo_rectangle(ctx, 0, 0, 1, 1);
	cairo_fill(ctx);

	cairo_translate(ctx, 1, 0);
	cairo_scale(ctx, 1.0 / (2 * local_half_span), 1.0 / (2 * local_half_span));
	cairo_translate(ctx, 0, -(local_index - local_half_span));

	// draw in [-1,0] * [0,1]
	float accum_p = 0;
	for(auto& child : ProbNode::getChildren(current)) {
		const auto color = getNodeColor(child.second);
		cairo_set_source_rgb(ctx,
			std::get<0>(color), std::get<1>(color), std::get<2>(color));
		
		cairo_rectangle(ctx, -child.first, accum_p, child.first, child.first);
		cairo_fill(ctx);

		cairo_save(ctx);
		cairo_set_source_rgb(ctx, 0, 0, 0);
		cairo_translate(ctx, -child.first, accum_p);
		cairo_scale(ctx, 0.003, 0.003);
		cairo_translate(ctx, 1, 10);
		cairo_show_text(ctx, child.second->getString().c_str());
		cairo_restore(ctx);

		accum_p += child.first;
	}

	cairo_restore(ctx);
}

std::tuple<double, double, double> Dasher::getNodeColor(
	std::shared_ptr<ProbNode> node) {

	if(static_cast<int>(node->getString()[0]) % 2 == 0) {
		return std::make_tuple(0.8, 0.8, 0.9);
	} else {
		return std::make_tuple(0.7, 0.7, 0.8);
	}
}

void Dasher::drawNode(std::shared_ptr<ProbNode> node, cairo_t* ctx) {
}
