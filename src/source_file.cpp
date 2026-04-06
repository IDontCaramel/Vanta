#include "source_file.h"

#include <algorithm>
#include <filesystem>
#include <sstream>

bool hasSupportedSourceExtension(std::string_view path) {
    return std::filesystem::path(path).extension() == kSourceFileExtension;
}

std::string supportedSourceFileDescription() {
    return std::string("*") + std::string(kSourceFileExtension);
}

bool SourceRange::isValid() const {
    return start.line > 0 && start.column > 0 && end.line > 0 && end.column > 0;
}

int SourceRange::highlightWidth() const {
    if (!isValid()) {
        return 1;
    }
    if (start.line != end.line) {
        return 1;
    }
    return std::max(1, end.column - start.column);
}

SourceRange SourceRange::singlePoint(int line, int column) {
    return fromBounds(line, column, line, column + 1);
}

SourceRange SourceRange::fromBounds(int startLine, int startColumn, int endLine, int endColumn) {
    return SourceRange{{startLine, startColumn}, {endLine, endColumn}};
}

SourceDocument::SourceDocument(std::string filename, std::string text)
    : path(std::move(filename)), contents(std::move(text)) {
    std::size_t start = 0;
    while (start <= contents.size()) {
        const std::size_t end = contents.find('\n', start);
        if (end == std::string::npos) {
            lines.emplace_back(contents.data() + start, contents.size() - start);
            break;
        }
        lines.emplace_back(contents.data() + start, end - start);
        start = end + 1;
    }

    if (lines.empty()) {
        lines.emplace_back(contents.data(), 0);
    }
}

const std::string& SourceDocument::filename() const {
    return path;
}

const std::string& SourceDocument::text() const {
    return contents;
}

std::string_view SourceDocument::lineText(int line) const {
    if (line <= 0 || static_cast<std::size_t>(line) > lines.size()) {
        return {};
    }
    return lines[static_cast<std::size_t>(line - 1)];
}

std::string formatSourceLocation(const SourceDocument& source, const SourceRange& range) {
    std::ostringstream stream;
    stream << source.filename() << ":" << range.start.line << ":" << range.start.column;
    return stream.str();
}

std::shared_ptr<SourceDocument> makeSourceDocument(std::string filename, std::string text) {
    return std::make_shared<SourceDocument>(std::move(filename), std::move(text));
}

SourceExcerpt makeSourceExcerpt(const SourceDocument& source, const SourceRange& range) {
    SourceExcerpt excerpt;
    if (!range.isValid()) {
        return excerpt;
    }

    const std::string_view line = source.lineText(range.start.line);
    if (line.empty() && range.start.line > 0) {
        return excerpt;
    }

    std::ostringstream prefix;
    prefix << range.start.line << " | ";
    excerpt.line = prefix.str() + std::string(line);

    const std::size_t gutterWidth = prefix.str().size();
    const int caretIndent = std::max(0, range.start.column - 1);
    const int caretWidth = range.highlightWidth();
    excerpt.caretLine = std::string(gutterWidth + static_cast<std::size_t>(caretIndent), ' ') +
                        std::string(static_cast<std::size_t>(caretWidth), '^');
    return excerpt;
}
