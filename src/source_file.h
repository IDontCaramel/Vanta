#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

constexpr std::string_view kSourceFileExtension = ".vt";

struct SourcePosition {
    int line = 1;
    int column = 1;
};

struct SourceRange {
    SourcePosition start;
    SourcePosition end;

    bool isValid() const;
    int highlightWidth() const;
    static SourceRange singlePoint(int line, int column);
    static SourceRange fromBounds(int startLine, int startColumn, int endLine, int endColumn);
};

class SourceDocument {
public:
    SourceDocument(std::string filename, std::string text);

    const std::string& filename() const;
    const std::string& text() const;
    std::string_view lineText(int line) const;

private:
    std::string path;
    std::string contents;
    std::vector<std::string_view> lines;
};

struct SourceExcerpt {
    std::string line;
    std::string caretLine;
};

bool hasSupportedSourceExtension(std::string_view path);
std::string supportedSourceFileDescription();
std::string formatSourceLocation(const SourceDocument& source, const SourceRange& range);
std::shared_ptr<SourceDocument> makeSourceDocument(std::string filename, std::string text);
SourceExcerpt makeSourceExcerpt(const SourceDocument& source, const SourceRange& range);
