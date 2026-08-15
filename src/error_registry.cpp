#include "nextviper/error_registry.hpp"

namespace nextviper {

ErrorRegistry::ErrorRegistry() {
    definitions_ = {
        {"NV1001", "unknown-identifier", "Unknown Identifier", "Compiler", "The identifier, variable, or function is not declared in the current scope."},
        {"NV1002", "syntax-error", "Syntax Error", "Compiler", "Source code violates NextViper grammatical rules and cannot be parsed."},
        {"NV1003", "type-mismatch", "Type Mismatch", "Compiler", "Expression type is incompatible with expected type annotation or operator operand."},
        {"NV1004", "invalid-function-call", "Invalid Function Call", "Compiler", "Incorrect number or type of arguments supplied to function call."},
        {"NV1005", "module-not-found", "Module Not Found", "Compiler", "The requested standard module or local file cannot be located."},
        {"NV2001", "package-not-found", "Package Not Found", "Package Manager", "Package is not installed in .nextviper/packages or missing from manifest."},
        {"NV2002", "file-not-found", "File Not Found", "CLI", "The specified source file cannot be opened or does not exist."},
        {"NV2003", "invalid-argument", "Invalid CLI Argument", "CLI", "Unrecognized subcommand or option flag passed to CLI tool."},
        {"NV3001", "dependency-resolution", "Dependency Resolution Error", "Package Manager", "Failed to solve compatible SemVer version set for package dependencies."},
        {"NV3002", "package-integrity", "Package Integrity Checksum Mismatch", "Package Manager", "Downloaded package SHA-256 hash does not match nextviper.lock."},
        {"NV4001", "division-by-zero", "Division by Zero", "Runtime", "Attempted integer or floating-point division by zero at runtime."},
        {"NV4002", "index-out-of-bounds", "Index Out of Bounds", "Runtime", "Array or list index out of valid range [0, length - 1]."},
        {"NV4003", "key-not-found", "Key Not Found", "Runtime", "Key does not exist in target map or dictionary."},
        {"NV4004", "null-reference", "Null Reference", "Runtime", "Attempted member access or invocation on null value."},
        {"NV4005", "file-io-error", "File I/O Error", "Runtime", "Filesystem read or write operation failed during runtime execution."},
        {"NV5001", "compiler-error", "Internal Compiler Error", "System", "Unexpected compiler error during native IR code generation."}
    };

    for (size_t i = 0; i < definitions_.size(); ++i) {
        index_by_code_[definitions_[i].code] = i;
    }
}

const ErrorRegistry& ErrorRegistry::instance() {
    static ErrorRegistry reg;
    return reg;
}

const ErrorCodeDefinition* ErrorRegistry::lookup(const std::string& code) const {
    auto it = index_by_code_.find(code);
    if (it != index_by_code_.end()) {
        return &definitions_[it->second];
    }
    return nullptr;
}

std::string ErrorRegistry::get_doc_slug(const std::string& code) const {
    const auto* def = lookup(code);
    if (def) return def->slug;
    return "";
}

std::string ErrorRegistry::get_doc_url(const std::string& code) const {
    const auto* def = lookup(code);
    if (def && !def->slug.empty()) {
        return std::string(DOCS_BASE_URL) + def->slug;
    }
    return "";
}

} // namespace nextviper
