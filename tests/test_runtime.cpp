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

TEST(EnvironmentTest, ResolvesThroughParentChain) {
    auto global = std::make_shared<Environment>();
    auto child = std::make_shared<Environment>(global);

    global->define("answer", Value(42.0));
    EXPECT_EQ(child->get("answer").toString(), "42");

    child->define("local", Value("ok"));
    EXPECT_EQ(child->get("local").toString(), "ok");
    EXPECT_THROW(global->get("local"), std::runtime_error);
}
