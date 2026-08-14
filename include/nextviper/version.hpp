#pragma once

#include <string_view>
#include <string>

namespace nextviper {

constexpr int VERSION_MAJOR = 0;
constexpr int VERSION_MINOR = 1;
constexpr int VERSION_PATCH = 0;
constexpr std::string_view VERSION_STRING = "0.1.0";
constexpr std::string_view LANGUAGE_NAME = "NextViper";
constexpr std::string_view RELEASE_CODENAME = "Vipera Genesis";

inline std::string get_full_version_string() {
    return std::string(LANGUAGE_NAME) + " " + std::string(VERSION_STRING) + " (" + std::string(RELEASE_CODENAME) + ")";
}

} // namespace nextviper
