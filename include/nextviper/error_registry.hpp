#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nextviper {

struct ErrorCodeDefinition {
    std::string code;
    std::string slug;
    std::string title;
    std::string category;
    std::string default_message;
};

class ErrorRegistry {
public:
    static const ErrorRegistry& instance();

    const ErrorCodeDefinition* lookup(const std::string& code) const;
    std::string get_doc_slug(const std::string& code) const;
    std::string get_doc_url(const std::string& code) const;
    const std::vector<ErrorCodeDefinition>& all_definitions() const { return definitions_; }

    static constexpr const char* DOCS_BASE_URL = "https://nextviper.nuratix.com/docs/errors/";

private:
    ErrorRegistry();
    std::vector<ErrorCodeDefinition> definitions_;
    std::unordered_map<std::string, size_t> index_by_code_;
};

inline std::string get_error_doc_slug(const std::string& code) {
    return ErrorRegistry::instance().get_doc_slug(code);
}

inline std::string get_error_doc_url(const std::string& code) {
    return ErrorRegistry::instance().get_doc_url(code);
}

} // namespace nextviper
