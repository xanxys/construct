#include "dasher.h"
#include "gtest/gtest.h"

TEST(LangModelTest, SumIsOne) {
	auto current =
		ProbNode::create(std::shared_ptr<EnglishModel>(new EnglishModel("count_1w.txt")));

	current = ProbNode::getChildren(current)[0].second;

	float sum = 0;
	for(auto& child : ProbNode::getChildren(current)) {
		EXPECT_LT(0, child.first);
		EXPECT_GE(1, child.first);
		sum += child.first;
	}
	EXPECT_FLOAT_EQ(1, sum);
}
