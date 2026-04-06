#include <gtest/gtest.h>

#include <sstream>

#include "diagnostic.h"
#include "evaluator.h"
#include "lexer.h"
#include "parser.h"
#include "source_file.h"

namespace {

std::shared_ptr<SourceDocument> makeTestDocument(const std::string& source, const std::string& filename = "test.vt") {
    return makeSourceDocument(filename, source);
}

std::string runProgram(const std::string& source, const std::string& input = "") {
    std::ostringstream output;
    std::istringstream in(input);
    auto document = makeTestDocument(source);
    Evaluator evaluator(output, in, document);
    Parser parser(Lexer(source).tokenize(), document);
    evaluator.evaluate(parser.parse());
    return output.str();
}

std::string runtimeError(const std::string& source) {
    try {
        (void)runProgram(source);
    } catch (const RuntimeError& error) {
        return error.what();
    }

    ADD_FAILURE() << "Expected runtime failure";
    return {};
}

Value uncaughtThrowValue(const std::string& source, std::string* outputText = nullptr) {
    std::ostringstream output;
    std::istringstream in;
    auto document = makeTestDocument(source);
    Evaluator evaluator(output, in, document);
    Parser parser(Lexer(source).tokenize(), document);

    try {
        evaluator.evaluate(parser.parse());
    } catch (const ThrowSignal& signal) {
        if (outputText) {
            *outputText = output.str();
        }
        return signal.value;
    }

    ADD_FAILURE() << "Expected uncaught throw";
    return Value();
}

}  // namespace

TEST(IntegrationTest, RunsHelloWorld) {
    EXPECT_EQ(runProgram("print(\"Hello, World!\");"), "Hello, World!\n");
}

TEST(IntegrationTest, RunsRecursiveFunctions) {
    const std::string source = R"(
        func fibonacci(n) {
            if (n <= 1) return n;
            return fibonacci(n - 1) + fibonacci(n - 2);
        }
        print(fibonacci(10));
    )";

    EXPECT_EQ(runProgram(source), "55\n");
}

TEST(IntegrationTest, SupportsClassesMethodsAndInterpolation) {
    const std::string source = R"(
        class Person {
            func __init__(name) {
                this.name = name;
            }

            func greet() {
                return $"Hello, I'm {this.name}";
            }
        }

        var person = new Person("Alice");
        print(person.greet());
    )";

    EXPECT_EQ(runProgram(source), "Hello, I'm Alice\n");
}

TEST(IntegrationTest, SupportsExceptions) {
    const std::string source = R"(
        try {
            throw "boom";
        } catch (error) {
            print($"Caught: {error}");
        } finally {
            print("done");
        }
    )";

    EXPECT_EQ(runProgram(source), "Caught: boom\ndone\n");
}

TEST(IntegrationTest, SupportsClosuresAndCapturedState) {
    const std::string source = R"(
        func makeCounter(start) {
            var count = start;
            return func() {
                count += 1;
                return count;
            };
        }

        var first = makeCounter(0);
        var second = makeCounter(10);
        print(first());
        print(first());
        print(second());
    )";

    EXPECT_EQ(runProgram(source), "1\n2\n11\n");
}

TEST(IntegrationTest, SupportsArraysArrowFunctionsAndDestructuring) {
    const std::string source = R"(
        var numbers = [1, 2, 3, 4];
        var doubled = numbers.map(x => x * 2);
        doubled.forEach(x => print(x));

        var [a, b] = [10, 20];
        print($"{a}, {b}");

        var {name, age} = {name: "Alice", age: 30};
        print($"{name} is {age}");
    )";

    EXPECT_EQ(runProgram(source), "2\n4\n6\n8\n10, 20\nAlice is 30\n");
}

TEST(IntegrationTest, FillsMissingDestructuredValuesWithNull) {
    const std::string source = R"(
        var [first, second, third] = [1];
        var {name, age} = {name: "Alice"};
        print($"{first}, {second}, {third}");
        print($"{name}, {age}");
    )";

    EXPECT_EQ(runProgram(source), "1, null, null\nAlice, null\n");
}

TEST(IntegrationTest, SupportsInheritanceAndStringHelpers) {
    const std::string source = R"(
        class Animal {
            func speak() {
                return "noise";
            }
        }

        class Dog extends Animal {
            func speak() {
                return "woof";
            }
        }

        var dog = new Dog();
        print(dog.speak());
        print("vanta".toUpperCase());
        print("a,b,c".split(",").length);
    )";

    EXPECT_EQ(runProgram(source), "woof\nVANTA\n3\n");
}

TEST(IntegrationTest, SupportsStringHelpersWithClampedBounds) {
    const std::string source = R"(
        print("vanta".substring(-5, 50));
        print("vanta".substring(2, 4));
        print("abc".split("").length);
    )";

    EXPECT_EQ(runProgram(source), "vanta\nnt\n3\n");
}

TEST(IntegrationTest, RunsFinallyBlocksBeforeReturning) {
    const std::string source = R"(
        func demo() {
            try {
                return "done";
            } finally {
                print("cleanup");
            }
        }

        print(demo());
    )";

    EXPECT_EQ(runProgram(source), "cleanup\ndone\n");
}

TEST(IntegrationTest, RethrowsAfterFinallyWhenExceptionIsUncaught) {
    const std::string source = R"(
        try {
            throw "boom";
        } finally {
            print("cleanup");
        }
    )";

    std::string output;
    const Value thrown = uncaughtThrowValue(source, &output);

    EXPECT_EQ(output, "cleanup\n");
    EXPECT_EQ(thrown.toString(), "boom");
}

TEST(IntegrationTest, ReportsRuntimeErrorsForBadPropertyAccess) {
    const std::string message = runtimeError("print({}.missing);");

    EXPECT_NE(message.find("missing"), std::string::npos) << message;
    EXPECT_NE(message.find("test.vt:1:7"), std::string::npos);
    EXPECT_NE(message.find("1 | print({}.missing);"), std::string::npos);
}

TEST(IntegrationTest, SupportsIndexingAndHighValueCollectionHelpers) {
    const std::string source = R"(
        func getItems() {
            return [10, 20, 30];
        }

        var numbers = [1, 2, 3, 4];
        numbers[1] = getItems()[2];

        print(numbers[1]);
        print("vanta"[2]);
        print(numbers.slice(1, 3).join("-"));
        print(numbers.reduce((sum, value) => sum + value, 0));
        print(keys({alpha: 1, beta: 2}).join(","));
        print("vanta".slice(1, 4));
    )";

    EXPECT_EQ(runProgram(source), "30\nn\n30-3\n38\nalpha,beta\nant\n");
}

TEST(IntegrationTest, SurfacesIndexErrorsEndToEnd) {
    try {
        (void)runProgram("print([1][2]);");
        FAIL() << "expected runtime error";
    } catch (const RuntimeError& error) {
        EXPECT_NE(std::string(error.what()).find("array index out of bounds"), std::string::npos);
    }
}

TEST(IntegrationTest, ReportsUndefinedVariablesWithSourceLocation) {
    const std::string message = runtimeError("print(name);");

    EXPECT_NE(message.find("Runtime error: undefined variable 'name'"), std::string::npos);
    EXPECT_NE(message.find("test.vt:1:7"), std::string::npos);
    EXPECT_NE(message.find("1 | print(name);"), std::string::npos);
}

TEST(IntegrationTest, ReportsInvalidAssignmentTargetsWithSourceLocation) {
    const std::string message = runtimeError("foo() = 1;");

    EXPECT_NE(message.find("Runtime error: invalid assignment target"), std::string::npos);
    EXPECT_NE(message.find("test.vt:1:1"), std::string::npos);
}

TEST(IntegrationTest, KeepsLanguageThrowsSeparateFromRuntimeErrors) {
    std::string output;
    const Value thrown = uncaughtThrowValue("throw \"boom\";", &output);

    EXPECT_TRUE(output.empty());
    EXPECT_EQ(thrown.toString(), "boom");
}

TEST(IntegrationTest, EnforcesVariableParameterAndReturnTypeHints) {
    const std::string source = R"(
        var total: Number = 41;
        total = total + 1;

        func double(value: Number) -> Number {
            return value * 2;
        }

        print(double(total));
    )";

    EXPECT_EQ(runProgram(source), "84\n");
}

TEST(IntegrationTest, AllowsUninitializedTypedVariablesUntilAssignment) {
    const std::string source = R"(
        var name: String;
        name = "Vanta";
        print(name);
    )";

    EXPECT_EQ(runProgram(source), "Vanta\n");
}

TEST(IntegrationTest, LeavesUnsupportedTypeHintsGradual) {
    const std::string source = R"(
        var user: Person = "Alice";
        print(user);
    )";

    EXPECT_EQ(runProgram(source), "Alice\n");
}

TEST(IntegrationTest, RejectsVariableTypeMismatch) {
    const std::string message = runtimeError(R"(
        var answer: Number = "forty two";
    )");

    EXPECT_NE(message.find("Runtime error: type mismatch for variable 'answer': expected Number, got String"),
              std::string::npos);
    EXPECT_NE(message.find("test.vt:2:9"), std::string::npos);
}

TEST(IntegrationTest, RejectsAssignmentToTypedVariable) {
    const std::string message = runtimeError(R"(
        var enabled: Boolean = true;
        enabled = "yes";
    )");

    EXPECT_NE(message.find("Runtime error: type mismatch for variable 'enabled': expected Boolean, got String"),
              std::string::npos);
    EXPECT_NE(message.find("test.vt:3:9"), std::string::npos);
}

TEST(IntegrationTest, RejectsParameterTypeMismatch) {
    const std::string message = runtimeError(R"(
        func greet(name: String) {
            print(name);
        }

        greet(123);
    )");

    EXPECT_NE(message.find("Runtime error: type mismatch for parameter 'name': expected String, got Number"),
              std::string::npos);
    EXPECT_NE(message.find("test.vt:6:9"), std::string::npos);
}

TEST(IntegrationTest, RejectsReturnTypeMismatch) {
    const std::string message = runtimeError(R"(
        func isReady() -> Boolean {
            return "yes";
        }

        print(isReady());
    )");

    EXPECT_NE(message.find("Runtime error: type mismatch for return value: expected Boolean, got String"),
              std::string::npos);
    EXPECT_NE(message.find("test.vt:6:15"), std::string::npos);
}
