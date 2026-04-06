#include <gtest/gtest.h>

#include "diagnostic.h"
#include "lexer.h"
#include "parser.h"
#include "source_file.h"

namespace {

std::string parseError(const std::string& source, const std::string& filename = "<test>") {
    try {
        auto document = makeSourceDocument(filename, source);
        Parser parser(Lexer(source).tokenize(), document);
        (void)parser.parse();
    } catch (const SyntaxError& error) {
        return error.what();
    }

    ADD_FAILURE() << "Expected parser failure";
    return {};
}

}  // namespace

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
        var answer: Number = 42;
        var [a, b] = [1, 2];
        var mapper = (x: Number) => x * 2;
    )";

    Parser parser(Lexer(source).tokenize());
    auto program = parser.parse();

    ASSERT_EQ(program->statements.size(), 3U);

    auto typedVariable = std::dynamic_pointer_cast<VarDeclStmt>(program->statements[0]);
    ASSERT_TRUE(typedVariable);
    EXPECT_EQ(typedVariable->name, "answer");
    EXPECT_EQ(typedVariable->typeHint, "Number");

    auto destructuring = std::dynamic_pointer_cast<VarDeclStmt>(program->statements[1]);
    ASSERT_TRUE(destructuring);
    EXPECT_TRUE(destructuring->isDestructuring);
    EXPECT_TRUE(destructuring->isArrayPattern);

    auto mapper = std::dynamic_pointer_cast<VarDeclStmt>(program->statements[2]);
    ASSERT_TRUE(mapper);
    ASSERT_TRUE(mapper->initializer);
    auto fn = std::dynamic_pointer_cast<FunctionExpr>(mapper->initializer);
    ASSERT_TRUE(fn);
    EXPECT_TRUE(fn->implicitReturn);
    ASSERT_EQ(fn->parameters.size(), 1U);
    EXPECT_EQ(fn->parameters[0].typeHint, "Number");
}

TEST(ParserTest, ParsesTryCatchFinallyAndFunctionExpressions) {
    const std::string source = R"(
        var makeGreeter = func(name) {
            return func() {
                return $"hello {name}";
            };
        };

        try {
            throw "boom";
        } catch (error) {
            print(error);
        } finally {
            print("done");
        }
    )";

    Parser parser(Lexer(source).tokenize());
    auto program = parser.parse();

    ASSERT_EQ(program->statements.size(), 2U);

    auto makeGreeter = std::dynamic_pointer_cast<VarDeclStmt>(program->statements[0]);
    ASSERT_TRUE(makeGreeter);
    auto outerFunction = std::dynamic_pointer_cast<FunctionExpr>(makeGreeter->initializer);
    ASSERT_TRUE(outerFunction);
    ASSERT_EQ(outerFunction->body.size(), 1U);

    auto tryStmt = std::dynamic_pointer_cast<TryStmt>(program->statements[1]);
    ASSERT_TRUE(tryStmt);
    EXPECT_EQ(tryStmt->catchParam, "error");
    ASSERT_TRUE(tryStmt->catchBlock);
    ASSERT_TRUE(tryStmt->finallyBlock);
}

TEST(ParserTest, ReportsMissingSemicolonAfterVariableDeclaration) {
    const std::string message = parseError("var answer = 42", "broken.vt");

    EXPECT_NE(message.find("Syntax error"), std::string::npos);
    EXPECT_NE(message.find("broken.vt:1:16"), std::string::npos);
    EXPECT_NE(message.find("1 | var answer = 42"), std::string::npos);
    EXPECT_NE(message.find("^"), std::string::npos);
    EXPECT_NE(message.find("expected ; after variable declaration"), std::string::npos);
}

TEST(ParserTest, ReportsTryWithoutCatchOrFinally) {
    const std::string message = parseError("try { print(\"nope\"); }");

    EXPECT_NE(message.find("expected catch or finally after try block"), std::string::npos);
}

TEST(ParserTest, ReportsInvalidDestructuringPattern) {
    const std::string message = parseError("var [first, 2] = [1, 2];");

    EXPECT_NE(message.find("expected array destructuring name"), std::string::npos);
}

TEST(ParserTest, ParsesIndexedExpressionsInPostfixChains) {
    const std::string source = R"(
        var result = getItems()[1].length;
    )";

    Parser parser(Lexer(source).tokenize());
    auto program = parser.parse();

    ASSERT_EQ(program->statements.size(), 1U);
    auto declaration = std::dynamic_pointer_cast<VarDeclStmt>(program->statements[0]);
    ASSERT_TRUE(declaration);

    auto member = std::dynamic_pointer_cast<MemberExpr>(declaration->initializer);
    ASSERT_TRUE(member);
    EXPECT_EQ(member->property, "length");

    auto index = std::dynamic_pointer_cast<IndexExpr>(member->object);
    ASSERT_TRUE(index);

    auto call = std::dynamic_pointer_cast<CallExpr>(index->object);
    ASSERT_TRUE(call);
    auto callee = std::dynamic_pointer_cast<VariableExpr>(call->callee);
    ASSERT_TRUE(callee);
    EXPECT_EQ(callee->name, "getItems");

    auto literal = std::dynamic_pointer_cast<LiteralExpr>(index->index);
    ASSERT_TRUE(literal);
    ASSERT_TRUE(std::holds_alternative<double>(literal->value));
    EXPECT_EQ(std::get<double>(literal->value), 1.0);
}

TEST(ParserTest, RejectsUnterminatedIndexExpressions) {
    Parser parser(Lexer("var result = items[0;").tokenize());

    try {
        (void)parser.parse();
        FAIL() << "expected parser error";
    } catch (const SyntaxError& error) {
        EXPECT_NE(std::string(error.what()).find("expected ] after index expression"), std::string::npos);
    }
}
