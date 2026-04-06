#include "diagnostic.h"

#include <sstream>

namespace {

std::string buildDiagnosticMessage(const std::string& label, const std::string& message,
                                   const std::shared_ptr<const SourceDocument>& source, const SourceRange& range) {
    std::ostringstream stream;
    stream << label << ": " << message;
    if (!source || !range.isValid()) {
        return stream.str();
    }

    stream << "\n  at " << formatSourceLocation(*source, range);
    const SourceExcerpt excerpt = makeSourceExcerpt(*source, range);
    if (!excerpt.line.empty()) {
        stream << "\n    " << excerpt.line;
        stream << "\n    " << excerpt.caretLine;
    }
    return stream.str();
}

}  // namespace

DiagnosticError::DiagnosticError(std::string label, std::string message, std::shared_ptr<const SourceDocument> source,
                                 SourceRange range)
    : std::runtime_error(buildDiagnosticMessage(label, message, source, range)),
      diagnosticLabel(std::move(label)),
      diagnosticMessage(std::move(message)),
      diagnosticSource(std::move(source)),
      diagnosticRange(range) {}

const std::string& DiagnosticError::label() const {
    return diagnosticLabel;
}

const std::string& DiagnosticError::message() const {
    return diagnosticMessage;
}

const SourceRange& DiagnosticError::range() const {
    return diagnosticRange;
}

const std::shared_ptr<const SourceDocument>& DiagnosticError::source() const {
    return diagnosticSource;
}

SyntaxError::SyntaxError(std::string message, std::shared_ptr<const SourceDocument> source, SourceRange range)
    : DiagnosticError("Syntax error", std::move(message), std::move(source), range) {}

RuntimeError::RuntimeError(std::string message, std::shared_ptr<const SourceDocument> source, SourceRange range)
    : DiagnosticError("Runtime error", std::move(message), std::move(source), range) {}

std::string formatDiagnostic(const std::string& label, const std::string& message,
                             const std::shared_ptr<const SourceDocument>& source, const SourceRange& range) {
    return buildDiagnosticMessage(label, message, source, range);
}
