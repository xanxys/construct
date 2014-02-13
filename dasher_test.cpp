#include "dasher.h"

#include <memory>
#include <vector>

#include "gtest/gtest.h"

using namespace construct;

class LangModelTest : public ::testing::Test {
protected:
	virtual void SetUp() override {
		model.reset(new EnglishModel("count_1w.txt", 5000));
	}

	std::shared_ptr<EnglishModel> model;
};

TEST_F(LangModelTest, ProbIsValid) {
	std::vector<std::string> prefixes = {
		"",
		"a",
		"th",
		"the",
		"beef",
		"nonexistentlongword",
	};

	for(auto prefix : prefixes) {
		float sum = 0;
		for(auto entry : model->nextCharGivenPrefix(prefix)) {
			EXPECT_LT(0, entry.second);
			EXPECT_GE(1, entry.second);
			sum += entry.second;
		}
		EXPECT_NEAR(1, sum, 1e-5);
	}
}

TEST_F(LangModelTest, NodeProbIsValid) {
	auto root = ProbNode::create(model);

	// Test root and all of root's children.
	std::vector<std::shared_ptr<ProbNode>> targets;
	targets.push_back(root);
	for(auto& child : ProbNode::getChildren(root)) {
		targets.push_back(child.second);
	}

	for(auto& target : targets) {
		float sum = 0;
		for(auto& child : ProbNode::getChildren(target)) {
			EXPECT_LT(0, child.first);
			EXPECT_GE(1, child.first);
			sum += child.first;
		}
		EXPECT_NEAR(1, sum, 1e-5);
	}
}

TEST_F(LangModelTest, FixedStringStartsEmpty) {
	Dasher dasher(model);

	// "Fixed" input must be empty at the beginning.
	EXPECT_EQ("", dasher.getFixed());
}
