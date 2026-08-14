#pragma once

#include <string_view>
#include <string>

namespace nextviper {

constexpr int VERSION_MAJOR = 1;
constexpr int VERSION_MINOR = 0;
constexpr int VERSION_PATCH = 0;
constexpr std::string_view VERSION_STRING = "1.0.0";
constexpr std::string_view LANGUAGE_NAME = "NextViper";
constexpr std::string_view RELEASE_CODENAME = "Apex";

inline std::string get_full_version_string() {
    return std::string(LANGUAGE_NAME) + " " + std::string(VERSION_STRING) + " (" + std::string(RELEASE_CODENAME) + ")";
}

} // namespace nextviper
