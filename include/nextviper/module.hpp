#pragma once

#include "nextviper/common.hpp"
#include "nextviper/value.hpp"
#include "nextviper/diagnostic.hpp"
#include <string>
#include <vector>
#include <map>
#include <unordered_set>
#include <memory>
#include <optional>
#include <filesystem>

namespace nextviper {

class Interpreter;
class Program;

class ModuleManager {
public:
    explicit ModuleManager(DiagnosticEngine& diagnostics);

    // Set search paths for packages / modules
    void add_search_path(const std::string& path);
    const std::vector<std::string>& search_paths() const { return search_paths_; }

    // Resolve and load module by name or path
    // Returns a Value of type ValueType::OBJECT containing module exports
    std::optional<Value> load_module(const std::string& module_spec, const std::string& current_file, Interpreter& interpreter);

    // Check if a built-in module exists
    bool is_builtin(const std::string& name) const;
    std::optional<Value> get_builtin_module(const std::string& name);

    // Resolve file path safely preventing path traversal vulnerabilities
    std::optional<std::string> resolve_module_path(const std::string& module_spec, const std::string& current_file);

    // Clear caches
    void clear_cache();

private:
    void register_builtin_modules();
    Value create_math_module();
    Value create_data_module();
    Value create_sys_module();
    Value create_time_module();

    DiagnosticEngine& diagnostics_;
    std::vector<std::string> search_paths_;
    std::map<std::string, Value> module_cache_;
    std::unordered_set<std::string> loading_modules_; // For cycle detection
    std::vector<std::unique_ptr<Program>> loaded_ast_programs_;
};

} // namespace nextviper
