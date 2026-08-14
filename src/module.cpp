#include "nextviper/module.hpp"
#include "nextviper/interpreter.hpp"
#include "nextviper/lexer.hpp"
#include "nextviper/parser.hpp"
#include <cmath>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <numeric>

namespace nextviper {

namespace fs = std::filesystem;

ModuleManager::ModuleManager(DiagnosticEngine& diagnostics)
    : diagnostics_(diagnostics) {
    search_paths_.push_back(".");
    search_paths_.push_back("./modules");
    search_paths_.push_back("./nextviper_modules");
    search_paths_.push_back("./packages");
    register_builtin_modules();
}

void ModuleManager::add_search_path(const std::string& path) {
    if (std::find(search_paths_.begin(), search_paths_.end(), path) == search_paths_.end()) {
        search_paths_.push_back(path);
    }
}

void ModuleManager::clear_cache() {
    module_cache_.clear();
    loading_modules_.clear();
    loaded_ast_programs_.clear();
    register_builtin_modules();
}

bool ModuleManager::is_builtin(const std::string& name) const {
    return name == "math" || name == "data" || name == "sys" || name == "time";
}

std::optional<Value> ModuleManager::get_builtin_module(const std::string& name) {
    auto it = module_cache_.find(name);
    if (it != module_cache_.end()) {
        return it->second;
    }
    if (name == "math") {
        Value mod = create_math_module();
        module_cache_["math"] = mod;
        return mod;
    }
    if (name == "data") {
        Value mod = create_data_module();
        module_cache_["data"] = mod;
        return mod;
    }
    if (name == "sys") {
        Value mod = create_sys_module();
        module_cache_["sys"] = mod;
        return mod;
    }
    if (name == "time") {
        Value mod = create_time_module();
        module_cache_["time"] = mod;
        return mod;
    }
    return std::nullopt;
}

void ModuleManager::register_builtin_modules() {
    module_cache_["math"] = create_math_module();
    module_cache_["data"] = create_data_module();
    module_cache_["sys"] = create_sys_module();
    module_cache_["time"] = create_time_module();
}

Value ModuleManager::create_math_module() {
    std::map<std::string, Value> exports;

    exports["pi"] = Value::make_float(3.14159265358979323846);
    exports["e"] = Value::make_float(2.71828182845904523536);

    exports["sqrt"] = Value::make_native_fn("sqrt", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        double v = args[0].as_float();
        return Value::make_float(std::sqrt(v));
    });

    exports["sin"] = Value::make_native_fn("sin", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_float(std::sin(args[0].as_float()));
    });

    exports["cos"] = Value::make_native_fn("cos", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_float(std::cos(args[0].as_float()));
    });

    exports["tan"] = Value::make_native_fn("tan", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_float(std::tan(args[0].as_float()));
    });

    exports["pow"] = Value::make_native_fn("pow", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_float(std::pow(args[0].as_float(), args[1].as_float()));
    });

    exports["abs"] = Value::make_native_fn("abs", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (args[0].is_int()) return Value::make_int(std::abs(args[0].as_int()));
        return Value::make_float(std::abs(args[0].as_float()));
    });

    exports["floor"] = Value::make_native_fn("floor", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_int(static_cast<int64_t>(std::floor(args[0].as_float())));
    });

    exports["ceil"] = Value::make_native_fn("ceil", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_int(static_cast<int64_t>(std::ceil(args[0].as_float())));
    });

    exports["round"] = Value::make_native_fn("round", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_int(static_cast<int64_t>(std::round(args[0].as_float())));
    });

    exports["min"] = Value::make_native_fn("min", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (args[0].is_int() && args[1].is_int()) {
            return Value::make_int(std::min(args[0].as_int(), args[1].as_int()));
        }
        return Value::make_float(std::min(args[0].as_float(), args[1].as_float()));
    });

    exports["max"] = Value::make_native_fn("max", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (args[0].is_int() && args[1].is_int()) {
            return Value::make_int(std::max(args[0].as_int(), args[1].as_int()));
        }
        return Value::make_float(std::max(args[0].as_float(), args[1].as_float()));
    });

    exports["log"] = Value::make_native_fn("log", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_float(std::log(args[0].as_float()));
    });

    return Value::make_object(std::move(exports));
}

Value ModuleManager::create_data_module() {
    std::map<std::string, Value> exports;

    exports["mean"] = Value::make_native_fn("mean", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (!args[0].is_array() || args[0].as_array()->empty()) return Value::make_float(0.0);
        const auto& arr = *args[0].as_array();
        double sum = 0.0;
        for (const auto& x : arr) sum += x.as_float();
        return Value::make_float(sum / static_cast<double>(arr.size()));
    });

    exports["sum"] = Value::make_native_fn("sum", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (!args[0].is_array()) return Value::make_int(0);
        const auto& arr = *args[0].as_array();
        bool all_int = true;
        int64_t isum = 0;
        double fsum = 0.0;
        for (const auto& x : arr) {
            if (!x.is_int()) all_int = false;
            isum += x.as_int();
            fsum += x.as_float();
        }
        return all_int ? Value::make_int(isum) : Value::make_float(fsum);
    });

    exports["chunk"] = Value::make_native_fn("chunk", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (!args[0].is_array() || !args[1].is_int()) return Value::make_array({});
        const auto& arr = *args[0].as_array();
        int64_t size = std::max<int64_t>(1, args[1].as_int());
        std::vector<Value> chunks;
        std::vector<Value> cur;
        for (const auto& item : arr) {
            cur.push_back(item);
            if (cur.size() == static_cast<size_t>(size)) {
                chunks.push_back(Value::make_array(std::move(cur)));
                cur.clear();
            }
        }
        if (!cur.empty()) chunks.push_back(Value::make_array(std::move(cur)));
        return Value::make_array(std::move(chunks));
    });

    return Value::make_object(std::move(exports));
}

Value ModuleManager::create_sys_module() {
    std::map<std::string, Value> exports;
    exports["version"] = Value::make_string("0.1.0");
#if defined(_WIN32)
    exports["platform"] = Value::make_string("windows");
#elif defined(__APPLE__)
    exports["platform"] = Value::make_string("macos");
#else
    exports["platform"] = Value::make_string("linux");
#endif
    exports["args"] = Value::make_array({});
    return Value::make_object(std::move(exports));
}

Value ModuleManager::create_time_module() {
    std::map<std::string, Value> exports;

    exports["now"] = Value::make_native_fn("now", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        double sec = std::chrono::duration_cast<std::chrono::duration<double>>(now).count();
        return Value::make_float(sec);
    });

    exports["sleep"] = Value::make_native_fn("sleep", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        int64_t ms = args[0].is_int() ? args[0].as_int() : static_cast<int64_t>(args[0].as_float() * 1000.0);
        if (ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }
        return Value::make_nil();
    });

    return Value::make_object(std::move(exports));
}

std::optional<std::string> ModuleManager::resolve_module_path(const std::string& module_spec, const std::string& current_file) {
    // 1. If builtin module
    if (is_builtin(module_spec)) {
        return module_spec;
    }

    std::vector<std::string> candidate_paths;

    // Check relative to current file's directory
    std::string base_dir = ".";
    if (!current_file.empty() && current_file != "<eval>" && current_file != "<repl>") {
        try {
            fs::path p(current_file);
            if (p.has_parent_path()) {
                base_dir = p.parent_path().string();
            }
        } catch (...) {}
    }

    auto add_candidates = [&](const fs::path& base) {
        fs::path p = base / module_spec;
        candidate_paths.push_back(p.string());
        candidate_paths.push_back((p.string() + ".nv"));
        candidate_paths.push_back((p / "mod.nv").string());
        candidate_paths.push_back((p / "main.nv").string());
    };

    // If explicit relative or absolute path
    if (module_spec.rfind("./", 0) == 0 || module_spec.rfind("../", 0) == 0 || module_spec.rfind("/", 0) == 0) {
        add_candidates(fs::path(base_dir));
    } else {
        // Search in base directory first
        add_candidates(fs::path(base_dir));
        // Search in all configured search paths
        for (const auto& sp : search_paths_) {
            add_candidates(fs::path(sp));
        }
    }

    for (const auto& candidate : candidate_paths) {
        std::error_code ec;
        if (fs::exists(candidate, ec) && !fs::is_directory(candidate, ec)) {
            try {
                return fs::canonical(candidate).string();
            } catch (...) {
                return candidate;
            }
        }
    }

    return std::nullopt;
}

std::optional<Value> ModuleManager::load_module(const std::string& module_spec, const std::string& current_file, Interpreter& interpreter) {
    // 1. Built-in module lookup
    if (is_builtin(module_spec)) {
        return get_builtin_module(module_spec);
    }

    // 2. Resolve safe canonical path
    auto resolved_path = resolve_module_path(module_spec, current_file);
    if (!resolved_path) {
        diagnostics_.error("cannot find module '" + module_spec + "'", SourceSpan{},
                           "ensure the file exists and is located in the search path or current directory");
        return std::nullopt;
    }

    const std::string& target_path = *resolved_path;

    // 3. Check cache
    auto it = module_cache_.find(target_path);
    if (it != module_cache_.end()) {
        return it->second;
    }

    // 4. Circular dependency detection
    if (loading_modules_.count(target_path)) {
        diagnostics_.error("circular dependency detected importing '" + module_spec + "'", SourceSpan{},
                           "module '" + target_path + "' is already in the loading chain");
        return std::nullopt;
    }

    loading_modules_.insert(target_path);

    // 5. Read source code
    std::ifstream file(target_path);
    if (!file.is_open()) {
        loading_modules_.erase(target_path);
        diagnostics_.error("failed to open module file: " + target_path, SourceSpan{});
        return std::nullopt;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    // 6. Tokenize & Parse
    Lexer lexer(source, target_path, diagnostics_);
    auto tokens = lexer.tokenize();
    if (diagnostics_.has_errors()) {
        loading_modules_.erase(target_path);
        return std::nullopt;
    }

    Parser parser(tokens, diagnostics_);
    auto program = parser.parse_program();
    if (diagnostics_.has_errors() || !program) {
        loading_modules_.erase(target_path);
        return std::nullopt;
    }

    loaded_ast_programs_.push_back(std::move(program));
    const auto& loaded_program = *loaded_ast_programs_.back();

    // 7. Execute in isolated module environment
    auto module_env = Environment::create(interpreter.globals());
    auto prev_env = interpreter.environment();
    auto prev_file = interpreter.current_file();
    interpreter.set_environment(module_env);
    interpreter.set_current_file(target_path);

    try {
        interpreter.execute(loaded_program);
    } catch (...) {
        interpreter.set_environment(prev_env);
        interpreter.set_current_file(prev_file);
        loading_modules_.erase(target_path);
        throw;
    }

    interpreter.set_environment(prev_env);
    interpreter.set_current_file(prev_file);
    loading_modules_.erase(target_path);

    // 8. Package all exported symbols from module_env into an Object
    std::map<std::string, Value> exports;
    for (const auto& [name, binding] : module_env->bindings()) {
        // Exclude internal builtins
        if (name.rfind("$", 0) != 0) {
            exports[name] = binding.value;
        }
    }

    Value mod_obj = Value::make_object(std::move(exports));
    module_cache_[target_path] = mod_obj;
    return mod_obj;
}

} // namespace nextviper
