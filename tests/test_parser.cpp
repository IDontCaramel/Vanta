#include <gtest/gtest.h>

#include "lexer.h"
#include "parser.h"

TEST(ParserTest, ParsesFunctionAndClassDeclarations) {
    const std::string source = R"(
        func add(a: Number, b: Number) -> Number {
            return a + b;
        }

        class Box {
            func __init__(value) {
                this.value = value;
            }
        }
    )";

    Parser parser(Lexer(source).tokenize());
    auto program = parser.parse();

    ASSERT_EQ(program->statements.size(), 2U);
    auto function = std::dynamic_pointer_cast<FunctionDeclStmt>(program->statements[0]);
    ASSERT_TRUE(function);
    EXPECT_EQ(function->name, "add");
    ASSERT_EQ(function->parameters.size(), 2U);
    EXPECT_EQ(function->parameters[0].typeHint, "Number");
    EXPECT_EQ(function->returnType, "Number");

    auto klass = std::dynamic_pointer_cast<ClassDeclStmt>(program->statements[1]);
    ASSERT_TRUE(klass);
    EXPECT_EQ(klass->name, "Box");
    ASSERT_EQ(klass->methods.size(), 1U);
    EXPECT_EQ(klass->methods[0]->name, "__init__");
}

TEST(ParserTest, ParsesArrowFunctionsAndDestructuring) {
    const std::string source = R"(
        var [a, b] = [1, 2];
        var mapper = x => x * 2;
    )";

    Parser parser(Lexer(source).tokenize());
    auto program = parser.parse();

    ASSERT_EQ(program->statements.size(), 2U);
    auto destructuring = std::dynamic_pointer_cast<VarDeclStmt>(program->statements[0]);
    ASSERT_TRUE(destructuring);
    EXPECT_TRUE(destructuring->isDestructuring);
    EXPECT_TRUE(destructuring->isArrayPattern);

    auto mapper = std::dynamic_pointer_cast<VarDeclStmt>(program->statements[1]);
    ASSERT_TRUE(mapper);
    ASSERT_TRUE(mapper->initializer);
    auto fn = std::dynamic_pointer_cast<FunctionExpr>(mapper->initializer);
    ASSERT_TRUE(fn);
    EXPECT_TRUE(fn->implicitReturn);
    ASSERT_EQ(fn->parameters.size(), 1U);
}
