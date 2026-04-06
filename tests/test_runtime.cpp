#include <gtest/gtest.h>

#include "environment.h"
#include "value.h"

TEST(ValueTest, SupportsArithmeticAndStringOperations) {
    Value number(10.0);
    Value other(2.0);

    EXPECT_EQ(number.add(other).toString(), "12");
    EXPECT_EQ(number.subtract(other).toString(), "8");
    EXPECT_EQ(number.multiply(other).toString(), "20");
    EXPECT_EQ(number.divide(other).toString(), "5");
    EXPECT_EQ(Value("Hello ").add(Value("Vanta")).toString(), "Hello Vanta");
}

TEST(ValueTest, DivisionByZeroThrows) {
    Value number(10.0);

    EXPECT_THROW(number.divide(Value(0.0)), std::runtime_error);
}

TEST(EnvironmentTest, ResolvesThroughParentChain) {
    auto global = std::make_shared<Environment>();
    auto child = std::make_shared<Environment>(global);

    global->define("answer", Value(42.0));
    EXPECT_EQ(child->get("answer").toString(), "42");

    child->define("local", Value("ok"));
    EXPECT_EQ(child->get("local").toString(), "ok");
    EXPECT_THROW(global->get("local"), std::runtime_error);
}

TEST(EnvironmentTest, RejectsAssigningUndefinedVariables) {
    auto env = std::make_shared<Environment>();

    EXPECT_THROW(env->assign("missing", Value(1.0)), std::runtime_error);
}

TEST(EnvironmentTest, EnforcesTypeHintsOnDefineAndAssign) {
    auto env = std::make_shared<Environment>();

    EXPECT_THROW(env->define("count", Value("oops"), "Number"), std::runtime_error);

    env->define("count", Value(1.0), "Number");
    EXPECT_THROW(env->assign("count", Value("oops")), std::runtime_error);
}
