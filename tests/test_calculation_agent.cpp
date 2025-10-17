#include <gtest/gtest.h>
#include "loki/agents/CalculationAgent.h"

// Test fixture for the CalculationAgent
class CalculationAgentTest : public ::testing::Test {
protected:
    CalculationAgent agent;
};

TEST_F(CalculationAgentTest, HandlesSimpleAddition) {
    loki::intent::Intent intent;
    intent.type = "calculation";
    intent.action = "evaluate_expression";
    intent.parameters["expression"] = "2 + 2";

    std::string result = agent.execute(intent);
    EXPECT_EQ(result, "The answer is 4");
}

TEST_F(CalculationAgentTest, HandlesComplexExpression) {
    loki::intent::Intent intent;
    intent.type = "calculation";
    intent.action = "evaluate_expression";
    intent.parameters["expression"] = "(10 * 5) - 20 / 2"; // 50 - 10

    std::string result = agent.execute(intent);
    EXPECT_EQ(result, "The answer is 40");
}

TEST_F(CalculationAgentTest, HandlesFloatingPointResult) {
    loki::intent::Intent intent;
    intent.type = "calculation";
    intent.action = "evaluate_expression";
    intent.parameters["expression"] = "10 / 4";

    std::string result = agent.execute(intent);
    EXPECT_EQ(result, "The answer is 2.5");
}

TEST_F(CalculationAgentTest, HandlesInvalidExpression) {
    loki::intent::Intent intent;
    intent.type = "calculation";
    intent.action = "evaluate_expression";
    intent.parameters["expression"] = "5 * * 8";

    std::string result = agent.execute(intent);
    EXPECT_EQ(result, "I'm sorry, I couldn't understand that math expression.");
}

TEST_F(CalculationAgentTest, HandlesMissingExpressionParameter) {
    loki::intent::Intent intent;
    intent.type = "calculation";
    intent.action = "evaluate_expression";
    // "expression" key is intentionally missing from parameters

    std::string result = agent.execute(intent);
    EXPECT_EQ(result, "You asked me to calculate something, but didn't provide an expression.");
}

TEST_F(CalculationAgentTest, HandlesUnknownAction) {
    loki::intent::Intent intent;
    intent.type = "calculation";
    intent.action = "do_something_else"; // Invalid action

    std::string result = agent.execute(intent);
    EXPECT_EQ(result, "I don't know how to perform that calculation.");
}