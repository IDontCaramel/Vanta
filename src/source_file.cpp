#include "source_file.h"

#include <filesystem>

bool hasSupportedSourceExtension(std::string_view path) {
    return std::filesystem::path(path).extension() == kSourceFileExtension;
}

std::string supportedSourceFileDescription() {
    return std::string("*") + std::string(kSourceFileExtension);
}
