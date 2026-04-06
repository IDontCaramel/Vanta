#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "source_file.h"

class DiagnosticError : public std::runtime_error {
public:
    DiagnosticError(std::string label, std::string message, std::shared_ptr<const SourceDocument> source,
                    SourceRange range);

    const std::string& label() const;
    const std::string& message() const;
    const SourceRange& range() const;
    const std::shared_ptr<const SourceDocument>& source() const;

private:
    std::string diagnosticLabel;
    std::string diagnosticMessage;
    std::shared_ptr<const SourceDocument> diagnosticSource;
    SourceRange diagnosticRange;
};

class SyntaxError final : public DiagnosticError {
public:
    SyntaxError(std::string message, std::shared_ptr<const SourceDocument> source, SourceRange range);
};

class RuntimeError final : public DiagnosticError {
public:
    RuntimeError(std::string message, std::shared_ptr<const SourceDocument> source, SourceRange range);
};

std::string formatDiagnostic(const std::string& label, const std::string& message,
                             const std::shared_ptr<const SourceDocument>& source, const SourceRange& range);
