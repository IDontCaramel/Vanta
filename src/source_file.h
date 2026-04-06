#pragma once

#include <string>
#include <string_view>

constexpr std::string_view kSourceFileExtension = ".vt";

bool hasSupportedSourceExtension(std::string_view path);
std::string supportedSourceFileDescription();
