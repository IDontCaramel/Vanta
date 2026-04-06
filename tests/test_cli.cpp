#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "cli.h"

namespace {

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

std::filesystem::path writeTempSource(const std::string& name, const std::string& source) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream file(path);
    file << source;
    file.close();
    return path;
}

}  // namespace

TEST(CliTest, ExecutesInlineCode) {
    std::istringstream input;
    std::ostringstream output;
    std::ostringstream error;

    const int exitCode = runCli({"-e", "print(1 + 2);"}, input, output, error);

    EXPECT_EQ(exitCode, 0);
    EXPECT_EQ(output.str(), "3\n");
    EXPECT_TRUE(error.str().empty());
}

TEST(CliTest, PrintsTokensForFileExecution) {
    const auto path = writeTempSource("vanta_cli_tokens.vt", "print(1 + 2);");

    std::istringstream input;
    std::ostringstream output;
    std::ostringstream error;

    const int exitCode = runCli({"--tokens", path.string()}, input, output, error);

    EXPECT_EQ(exitCode, 0);
    EXPECT_TRUE(contains(output.str(), "IDENTIFIER \"print\""));
    EXPECT_TRUE(contains(output.str(), "PLUS \"+\""));
    EXPECT_TRUE(contains(output.str(), "EOF \"\""));
    EXPECT_TRUE(error.str().empty());
}

TEST(CliTest, PrintsAstBeforeEvaluation) {
    const auto path = writeTempSource("vanta_cli_ast.vt", "print(1 + 2);");

    std::istringstream input;
    std::ostringstream output;
    std::ostringstream error;

    const int exitCode = runCli({"--ast", path.string()}, input, output, error);

    EXPECT_EQ(exitCode, 0);
    EXPECT_TRUE(contains(output.str(), "Program\n"));
    EXPECT_TRUE(contains(output.str(), "ExprStmt\n"));
    EXPECT_TRUE(contains(output.str(), "CallExpr\n"));
    EXPECT_TRUE(contains(output.str(), "Binary(+)\n"));
    EXPECT_TRUE(contains(output.str(), "3\n"));
    EXPECT_TRUE(error.str().empty());
}

TEST(CliTest, ExecutesSourceFile) {
    const auto path = writeTempSource("vanta_cli_file.vt", "print(\"from file\");");

    std::istringstream input;
    std::ostringstream output;
    std::ostringstream error;

    const int exitCode = runCli({path.string()}, input, output, error);

    EXPECT_EQ(exitCode, 0);
    EXPECT_EQ(output.str(), "from file\n");
    EXPECT_TRUE(error.str().empty());
}

TEST(CliTest, StartsReplWhenNoArgumentsAreProvided) {
    std::istringstream input("1 + 2\nvar answer = 40 + 2;\nanswer\nquit\n");
    std::ostringstream output;
    std::ostringstream error;

    const int exitCode = runCli({}, input, output, error);

    EXPECT_EQ(exitCode, 0);
    EXPECT_TRUE(contains(output.str(), "vanta> "));
    EXPECT_TRUE(contains(output.str(), "3\n"));
    EXPECT_TRUE(contains(output.str(), "42\n"));
    EXPECT_TRUE(error.str().empty());
}

TEST(CliTest, PrintsUsageForHelp) {
    std::istringstream input;
    std::ostringstream output;
    std::ostringstream error;

    const int exitCode = runCli({"--help"}, input, output, error);

    EXPECT_EQ(exitCode, 0);
    EXPECT_TRUE(contains(output.str(), "Usage: vanta"));
    EXPECT_TRUE(error.str().empty());
}

TEST(CliTest, RejectsUnsupportedExtensions) {
    const auto path = writeTempSource("vanta_cli_bad_extension.txt", "print(\"wrong\");");

    std::istringstream input;
    std::ostringstream output;
    std::ostringstream error;

    const int exitCode = runCli({path.string()}, input, output, error);

    EXPECT_EQ(exitCode, 1);
    EXPECT_TRUE(output.str().empty());
    EXPECT_TRUE(contains(error.str(), "expected a *.vt source file"));
}

TEST(CliTest, RejectsConflictingInlineAndFileInputs) {
    const auto path = writeTempSource("vanta_cli_conflict.vt", "print(\"ignored\");");

    std::istringstream input;
    std::ostringstream output;
    std::ostringstream error;

    const int exitCode = runCli({"-e", "print(1);", path.string()}, input, output, error);

    EXPECT_EQ(exitCode, 1);
    EXPECT_TRUE(output.str().empty());
    EXPECT_TRUE(contains(error.str(), "cannot use a source file together with -e"));
    EXPECT_TRUE(contains(error.str(), "Usage: vanta"));
}

TEST(CliTest, ReportsParserErrorsFromSourceFiles) {
    const auto path = writeTempSource("vanta_cli_parse_error.vt", "var answer = 42");

    std::istringstream input;
    std::ostringstream output;
    std::ostringstream error;

    const int exitCode = runCli({path.string()}, input, output, error);

    EXPECT_EQ(exitCode, 1);
    EXPECT_TRUE(output.str().empty());
    EXPECT_TRUE(contains(error.str(), "Syntax error"));
    EXPECT_TRUE(contains(error.str(), path.string() + ":1:16"));
    EXPECT_TRUE(contains(error.str(), "expected ; after variable declaration"));
}

TEST(CliTest, ReportsUncaughtThrowsFromInlineCode) {
    std::istringstream input;
    std::ostringstream output;
    std::ostringstream error;

    const int exitCode = runCli({"-e", "throw \"boom\";"}, input, output, error);

    EXPECT_EQ(exitCode, 1);
    EXPECT_TRUE(output.str().empty());
    EXPECT_TRUE(contains(error.str(), "Uncaught exception: boom"));
    EXPECT_TRUE(contains(error.str(), "<inline>:1:1"));
}
