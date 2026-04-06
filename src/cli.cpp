#include "cli.h"

#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>

#include "diagnostic.h"
#include "debug_output.h"
#include "evaluator.h"
#include "lexer.h"
#include "parser.h"
#include "source_file.h"

namespace {

struct CliOptions {
    bool showTokens = false;
    bool showAst = false;
    std::optional<std::string> inlineCode;
    std::optional<std::string> filePath;
};

std::string usage() {
    std::ostringstream stream;
    stream << "Usage: vanta [--tokens] [--ast] <filename" << kSourceFileExtension << ">\n"
           << "       vanta [--tokens] [--ast] -e \"<code>\"\n"
           << "       vanta [--tokens] [--ast]\n";
    return stream.str();
}

std::string trim(const std::string& value) {
    const std::size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return {};
    }
    const std::size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

CliOptions parseArgs(const std::vector<std::string>& args) {
    CliOptions options;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--tokens") {
            options.showTokens = true;
            continue;
        }
        if (arg == "--ast") {
            options.showAst = true;
            continue;
        }
        if (arg == "-e") {
            if (options.filePath) {
                throw std::runtime_error("cannot use -e together with a source file");
            }
            if (options.inlineCode) {
                throw std::runtime_error("inline code already provided");
            }
            if (i + 1 >= args.size()) {
                throw std::runtime_error("missing argument after -e");
            }
            options.inlineCode = args[++i];
            continue;
        }
        if (arg == "-h" || arg == "--help") {
            throw std::runtime_error("");
        }
        if (!arg.empty() && arg.front() == '-') {
            throw std::runtime_error("unknown option: " + arg);
        }
        if (options.inlineCode) {
            throw std::runtime_error("cannot use a source file together with -e");
        }
        if (options.filePath) {
            throw std::runtime_error("expected a single source file");
        }
        options.filePath = arg;
    }

    return options;
}

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Could not open file: " + path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void printTokensIfRequested(const std::vector<Token>& tokens, const CliOptions& options, std::ostream& out) {
    if (options.showTokens) {
        out << formatTokens(tokens);
    }
}

void printProgramAstIfRequested(const std::shared_ptr<ProgramStmt>& program, const CliOptions& options,
                                std::ostream& out) {
    if (options.showAst) {
        out << formatAst(program);
    }
}

void printExprAstIfRequested(const ExprPtr& expression, const CliOptions& options, std::ostream& out) {
    if (options.showAst) {
        out << formatAst(expression);
    }
}

void evaluateProgram(const std::shared_ptr<SourceDocument>& document, Evaluator& evaluator,
                     const CliOptions& options, std::ostream& out) {
    const std::vector<Token> tokens = Lexer(document->text()).tokenize();
    printTokensIfRequested(tokens, options, out);
    Parser parser(tokens, document);
    auto program = parser.parse();
    printProgramAstIfRequested(program, options, out);
    evaluator.evaluate(program);
}

void evaluateReplLine(const std::shared_ptr<SourceDocument>& document, Evaluator& evaluator, const CliOptions& options,
                      std::ostream& out) {
    const std::vector<Token> tokens = Lexer(document->text()).tokenize();
    printTokensIfRequested(tokens, options, out);

    try {
        Parser parser(tokens, document);
        auto program = parser.parse();
        printProgramAstIfRequested(program, options, out);
        evaluator.evaluate(program);
        return;
    } catch (const SyntaxError&) {
    }

    Parser parser(tokens, document);
    ExprPtr expression = parser.parseExpressionOnly();
    printExprAstIfRequested(expression, options, out);
    const Value result = evaluator.evaluateExpr(expression);
    out << result.toString() << '\n';
}

int executeSource(const std::shared_ptr<SourceDocument>& document, const CliOptions& options, std::istream& in,
                  std::ostream& out, std::ostream& err) {
    try {
        Evaluator evaluator(out, in, document);
        evaluateProgram(document, evaluator, options, out);
        return 0;
    } catch (const ThrowSignal& signal) {
        err << formatDiagnostic("Uncaught exception", signal.value.toString(), document, signal.range) << '\n';
        return 1;
    } catch (const DiagnosticError& error) {
        err << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        err << "Internal interpreter error: " << error.what() << '\n';
        return 1;
    }
}

int runRepl(const CliOptions& options, std::istream& in, std::ostream& out, std::ostream& err) {
    auto replDocument = makeSourceDocument("<repl>", "");
    Evaluator evaluator(out, in, replDocument);
    std::string line;

    while (true) {
        out << "vanta> " << std::flush;
        if (!std::getline(in, line)) {
            return 0;
        }

        const std::string trimmed = trim(line);
        if (trimmed.empty()) {
            continue;
        }
        if (trimmed == "exit" || trimmed == "quit") {
            return 0;
        }

        try {
            replDocument = makeSourceDocument("<repl>", line);
            evaluator.setSourceDocument(replDocument);
            evaluateReplLine(replDocument, evaluator, options, out);
        } catch (const ThrowSignal& signal) {
            err << formatDiagnostic("Uncaught exception", signal.value.toString(), replDocument, signal.range) << '\n';
        } catch (const DiagnosticError& error) {
            err << error.what() << '\n';
        } catch (const std::exception& error) {
            err << "Internal interpreter error: " << error.what() << '\n';
        }
    }
}

}  // namespace

int runCli(const std::vector<std::string>& args, std::istream& in, std::ostream& out, std::ostream& err) {
    CliOptions options;

    try {
        options = parseArgs(args);
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()).empty()) {
            out << usage();
            return 0;
        }
        err << "Error: " << error.what() << '\n' << usage();
        return 1;
    }

    if (options.inlineCode) {
        return executeSource(makeSourceDocument("<inline>", *options.inlineCode), options, in, out, err);
    }

    if (options.filePath) {
        if (!hasSupportedSourceExtension(*options.filePath)) {
            err << "Error: expected a " << supportedSourceFileDescription()
                << " source file, got: " << *options.filePath << '\n';
            return 1;
        }
        try {
            return executeSource(makeSourceDocument(*options.filePath, readFile(*options.filePath)), options, in, out,
                                 err);
        } catch (const std::exception& error) {
            err << error.what() << '\n';
            return 1;
        }
    }

    return runRepl(options, in, out, err);
}
