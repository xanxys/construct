#include "dasher.h"

#include <boost/algorithm/string.hpp>
#include <cassert>
#include <fstream>
#include <iostream>
#include <numeric>
#include <thread>


EnglishModel::EnglishModel(std::string w1_file) :
	alphabet("abcdefghijklmnopqrstuvwxyz") {
	// Parse 1-word frequency table.
	std::map<std::string, uint64_t> freq_table;

	int est_num_prefix = 0;
	std::ifstream fs(w1_file);
	while(!fs.eof()) {
		std::string line;
		std::getline(fs, line);

		std::vector<std::string> entry;
		boost::algorithm::split(entry, line, boost::is_any_of("\t"));

		if(entry.size() != 2) {
			continue;
		}

		try {
			freq_table[entry[0]] = std::stoull(entry[1]);
			est_num_prefix += entry[0].size() + 1;
		} catch(std::invalid_argument exc) {
			// ignore
		}
	}

	// Construct prefix table.
	const uint64_t max_char_table_size = alphabet.size() + 1;

	std::unordered_map<std::string, std::map<char, uint64_t>> prefix_table;
	prefix_table.reserve(est_num_prefix);
	for(const auto& word_freq : freq_table) {
		const std::string& word = word_freq.first;
		const uint64_t freq = word_freq.second;

		// Ignore empty word.
		if(word.empty()) {
			continue;
		}

		// "ab" -> "", "a", "ab"
		for(int i = 0; i < word.size() + 1; i++) {
			const std::string prefix = word.substr(0, i);
			const char next = (i == word.size()) ? ' ' : word[i];

			auto insertion = prefix_table.emplace(prefix,
				std::map<char, uint64_t>());

			auto insertion_p = insertion.first->second.emplace(next, 0);

			insertion_p.first->second += freq;
		}
	}
	
	// Smooth & Normalize prefix table.
	word1_prefix_table.reserve(est_num_prefix);
	for(auto& prefix_e : prefix_table) {
		uint64_t sum_char_freq = 0;
		for(const auto& char_freq : prefix_e.second) {
			sum_char_freq += char_freq.second;
		}

		std::map<char, float> ch_table;
		for(const auto& char_freq : prefix_e.second) {
			assert(char_freq.second <= sum_char_freq);

			ch_table[char_freq.first] = 
				static_cast<float>(char_freq.second) / sum_char_freq;
		}

		// Smooth distribution by giving additional 0.001 probability for all chars.
		float sum_new_prob = 0;
		for(char ch : alphabet + " ") {
			auto insertion = ch_table.emplace(ch, 0);
			insertion.first->second += 0.001;
			sum_new_prob += insertion.first->second;
		}

		// Re-normalize.
		for(auto& char_prob : ch_table) {
			char_prob.second /= sum_new_prob;
		}

		word1_prefix_table[prefix_e.first] = ch_table;
	}

	// Create fall-back letter table.
	for(char ch : alphabet + " ") {
		letter_table_any[ch] =
			(ch == 'e' ? 2.0 : 1.0) / (alphabet.size() + 1 + 1);
	}
}

const std::map<char, float> EnglishModel::nextCharGivenPrefix(std::string prefix) {
	// TODO: smooth so that all characters appears with non-zero probability.
	auto char_distrib = word1_prefix_table.find(prefix);
	if(char_distrib != word1_prefix_table.end()) {
		return char_distrib->second;
	} else {
		return letter_table_any;
	}
}

std::shared_ptr<ProbNode> ProbNode::create(std::shared_ptr<EnglishModel> model) {
	return std::shared_ptr<ProbNode>(new ProbNode(model, std::shared_ptr<ProbNode>(), ""));
}

std::shared_ptr<ProbNode> ProbNode::getParent(std::shared_ptr<ProbNode> node) {
	return node->parent;
}

std::string ProbNode::getWordPrefix() {
	std::string result;
	ProbNode* iter = this;

	while(iter && iter->getString() != " ") {
		result = iter->getString() + result;
		iter = iter->parent.get();
	}

	return result;
}

std::vector<std::pair<float, std::shared_ptr<ProbNode>>> ProbNode::getChildren(std::shared_ptr<ProbNode> node) {
	std::vector<std::pair<float, std::shared_ptr<ProbNode>>> children;


	const auto char_table = node->model->nextCharGivenPrefix(node->getWordPrefix());
	for(const auto ch_prob : char_table) {
		children.push_back(std::make_pair(
			ch_prob.second,
			std::shared_ptr<ProbNode>(new ProbNode(
				node->model, node, std::string(1, ch_prob.first)))));
	}

	return children;
}

ProbNode::ProbNode(
	std::shared_ptr<EnglishModel> model,
	std::shared_ptr<ProbNode> parent, std::string str) :
	model(model), parent(parent), str(str) {
}

std::string ProbNode::getString() {
	return str;
}


Dasher::Dasher() : model(nullptr) {
	// Initiate model loading.
	std::thread([this]() {
		model.store(new EnglishModel("count_1w.txt"));
	}).detach();

	local_index = 0;
	local_half_span = 0.5;
}

Dasher::Dasher(std::shared_ptr<EnglishModel> model) :
	current(ProbNode::create(model)) {
	local_index = 0;
	local_half_span = 0.5;	
}

void Dasher::update(float dt, float rel_index, float rel_zoom) {
	if(!current) {
		if(model.load()) {
			current = ProbNode::create(std::shared_ptr<EnglishModel>(model.load()));
		} else {
			return;
		}
	}

	const float speed = 1.0;
	local_half_span = std::max(local_half_span / 4, local_half_span + dt * local_half_span * speed * rel_zoom);
	local_index += dt * speed * (rel_index * local_half_span);
	fit();
}

void Dasher::fit() {
	assert(current);
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
	if(!current) {
		return "";
	}

	// Traverse upward, collecting string.
	std::string result;
	std::shared_ptr<ProbNode> iter = getProbableNode();
	while(iter) {
		result = iter->getString() + result;
		iter = ProbNode::getParent(iter);
	}
	return result;
}

std::shared_ptr<ProbNode> Dasher::getProbableNode() {
	assert(current);

	// By the invariance, current always contains the center.
	std::shared_ptr<ProbNode> node = current;
	std::shared_ptr<ProbNode> prev_node = current;
	float index = local_index;
	float half_span = local_half_span;

	while(half_span <= 1) {
		float accum_p = 0;
		for(auto& child : ProbNode::getChildren(node)) {
			if(accum_p <= index && index < accum_p + child.first) {
				index = (index - accum_p) / child.first;
				half_span /= child.first;

				prev_node = node;
				node = child.second;
				break;
			}
			accum_p += child.first;
		}
	}

	return prev_node;
}

void Dasher::visualize(cairo_t* ctx) {
	cairo_save(ctx);
	cairo_scale(ctx, 250, 250);

	cairo_set_source_rgb(ctx, 1, 1, 1);
	cairo_paint(ctx);

	cairo_translate(ctx, 1, 0);
	cairo_scale(ctx, 1.0 / (2 * local_half_span), 1.0 / (2 * local_half_span));
	cairo_translate(ctx, 0, -(local_index - local_half_span));

	// draw in [-1,0] * [0,1]
	if(current) {
		drawNode(current, ctx, 0, 1);
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

// TODO: node should have aspect > 1, because when a child of a node is
// almost 0, there's no space for characters.
void Dasher::drawNode(std::shared_ptr<ProbNode> node, cairo_t* ctx, float p0, float p1) {
	const float dp = p1 - p0;
	assert(dp <= 1.0);
	if(dp < 0.001) {
		return;
	}

	// Draw box. Don't use cairo_stroke to avoid node overlap.
	const auto color = getNodeColor(node);

	// outer
	cairo_new_path(ctx);
	cairo_rectangle(ctx, -dp, p0, dp, dp);
	cairo_set_source_rgb(ctx, 0.2, 0.2, 0.2);
	cairo_fill(ctx);

	// inner (left, top, bottom)
	const float margin = dp * 0.01;
	cairo_new_path(ctx);
	cairo_rectangle(ctx,
		-dp + margin, p0 + margin,
		dp - margin, dp - margin * 2);
	cairo_set_source_rgb(ctx,
		std::get<0>(color), std::get<1>(color), std::get<2>(color));
	cairo_fill(ctx);

	// Show text.
	const std::string input_string = node->getString();
	const std::string display_string = (input_string == " ") ? "␣" : input_string;
	cairo_save(ctx);
	cairo_set_source_rgb(ctx, 0, 0, 0);
	cairo_translate(ctx, -dp, p0);
	cairo_scale(ctx, dp * 0.1, dp * 0.1);
	cairo_translate(ctx, 0, 5);
	cairo_show_text(ctx, display_string.c_str());
	cairo_restore(ctx);

	// Draw children.
	float accum_p = 0;
	for(auto& child : ProbNode::getChildren(node)) {
		assert(0 < child.first && child.first <= 1);

		drawNode(child.second, ctx,
			p0 + dp * accum_p, p0 + dp * (accum_p + child.first));
		accum_p += child.first;
	}
}
