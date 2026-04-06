#include <fstream>
#include <iostream>
#include <sstream>

#include "evaluator.h"
#include "lexer.h"
#include "parser.h"
#include "source_file.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: vanta <filename" << kSourceFileExtension << ">" << std::endl;
        return 1;
    }

    if (!hasSupportedSourceExtension(argv[1])) {
        std::cerr << "Error: expected a " << supportedSourceFileDescription()
                  << " source file, got: " << argv[1] << std::endl;
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file) {
        std::cerr << "Could not open file: " << argv[1] << std::endl;
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    try {
        Lexer lexer(buffer.str());
        Parser parser(lexer.tokenize());
        auto program = parser.parse();
        Evaluator evaluator;
        evaluator.evaluate(program);
        return 0;
    } catch (const ThrowSignal& signal) {
        std::cerr << "Uncaught exception: " << signal.value.toString() << std::endl;
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << std::endl;
        return 1;
    }
}
