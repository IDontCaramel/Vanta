#include <gtest/gtest.h>

#include <sstream>

#include "diagnostic.h"
#include "evaluator.h"
#include "lexer.h"
#include "parser.h"
#include "source_file.h"

namespace {

ExprPtr parseExpression(const std::string& source) {
    auto document = makeSourceDocument("expr.vt", source);
    Parser parser(Lexer(source).tokenize(), document);
    return parser.parseExpressionOnly();
}

void expectRuntimeError(Evaluator& evaluator, const std::string& source, const std::string& message) {
    try {
        (void)evaluator.evaluateExpr(parseExpression(source));
        FAIL() << "expected runtime error";
    } catch (const std::exception& error) {
        EXPECT_NE(std::string(error.what()).find(message), std::string::npos) << error.what();
    }
}

}  // namespace

TEST(EvaluatorTest, SupportsIndexedReadsAndWrites) {
    std::ostringstream output;
    std::istringstream input;
    Evaluator evaluator(output, input);

    evaluator.globals()->define("arr", Value::array({Value(10.0), Value(20.0), Value(30.0)}));
    evaluator.globals()->define("text", Value("vanta"));

    EXPECT_EQ(evaluator.evaluateExpr(parseExpression("arr[1]")).toString(), "20");
    EXPECT_EQ(evaluator.evaluateExpr(parseExpression("text[2]")).toString(), "n");
    EXPECT_EQ(evaluator.evaluateExpr(parseExpression("arr[1] = 99")).toString(), "99");
    EXPECT_EQ(evaluator.evaluateExpr(parseExpression("arr[1]")).toString(), "99");
}

TEST(EvaluatorTest, ReportsIndexValidationErrorsClearly) {
    std::ostringstream output;
    std::istringstream input;
    Evaluator evaluator(output, input);

    evaluator.globals()->define("arr", Value::array({Value(1.0), Value(2.0)}));
    evaluator.globals()->define("text", Value("ok"));
    evaluator.globals()->define("obj", Value::object({{"name", Value("Ada")}}));

    expectRuntimeError(evaluator, "arr[2]", "array index out of bounds");
    expectRuntimeError(evaluator, "arr[1.5]", "array index must be an integer");
    expectRuntimeError(evaluator, "arr[\"0\"]", "array index must be a number");
    expectRuntimeError(evaluator, "obj[0]", "indexing requires an array or string");
    expectRuntimeError(evaluator, "text[0] = \"x\"", "cannot assign through string indexing");
}
