#include <gtest/gtest.h>

#include <sstream>

#include "evaluator.h"
#include "lexer.h"
#include "parser.h"

namespace {

std::string runProgram(const std::string& source, const std::string& input = "") {
    std::ostringstream output;
    std::istringstream in(input);
    Evaluator evaluator(output, in);
    Parser parser(Lexer(source).tokenize());
    evaluator.evaluate(parser.parse());
    return output.str();
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
