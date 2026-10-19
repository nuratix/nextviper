#include "nextviper/native_compiler.hpp"
#include "nextviper/native_runtime.hpp"
#include "nextviper/type_checker.hpp"
#include <set>
#include <iomanip>
#include "nextviper/interpreter.hpp"
#include "nextviper/compiler.hpp"
#include "nextviper/vm.hpp"
#include <sstream>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <array>

namespace nextviper {

namespace {
static std::string c_escape_string(const std::string& str) {
    std::string out;
    for (char c : str) {
        if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c == '\\') out += "\\\\";
        else if (c == '"') out += "\\\"";
        else if (static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) >= 127) {
            char escaped[5]; std::snprintf(escaped, sizeof(escaped), "\\%03o", static_cast<unsigned char>(c)); out += escaped;
        } else out += c;
    }
    return out;
}
}

#if !defined(_WIN32)
#include <sys/wait.h>
#include <spawn.h>
#include <unistd.h>
extern "C" char** environ;

static int safe_run_process(const std::string& exe, const std::vector<std::string>& args) {
    std::vector<char*> c_args;
    c_args.push_back(const_cast<char*>(exe.c_str()));
    for (const auto& a : args) {
        c_args.push_back(const_cast<char*>(a.c_str()));
    }
    c_args.push_back(nullptr);

    pid_t pid;
    int status = posix_spawnp(&pid, exe.c_str(), nullptr, nullptr, c_args.data(), environ);
    if (status != 0) return -1;
    if (waitpid(pid, &status, 0) == -1) return -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
}
#endif

NativeCompiler::NativeCompiler(DiagnosticEngine& diag) : diag_(diag) {}

std::string NativeCompiler::find_c_compiler() const {
    const char* path_env = std::getenv("PATH");
    std::vector<std::string> path_dirs;
    if (path_env) {
        std::stringstream ss(path_env);
        std::string dir;
        while (std::getline(ss, dir, ':')) {
            if (!dir.empty()) path_dirs.push_back(dir);
        }
    } else {
        path_dirs = {"/usr/bin", "/usr/local/bin", "/bin"};
    }

    std::vector<std::string> candidates = {"g++", "clang++", "c++"};
    for (const auto& c : candidates) {
        for (const auto& dir : path_dirs) {
            std::error_code ec;
            std::filesystem::path p = std::filesystem::path(dir) / c;
            if (std::filesystem::exists(p, ec) && !std::filesystem::is_directory(p, ec)) {
                return c;
            }
        }
    }
    return "c++";
}

std::string NativeCompiler::emit_native_source(const IRModule& module) const {
    std::ostringstream out;
    out << NATIVE_RUNTIME_SOURCE << "\n";
    std::map<std::string, const IRFunction*> functions;
    std::map<std::string, std::set<int>> defined, cells, captures;
    std::map<std::string, std::set<std::string>> references;
    const std::set<std::string> builtins = {"array_new","object_new","print","println","io_print","print_raw",
        "len","string_len","push","append","insert","get","set","set_prop","pop","range","clock",
        "time_now","time_now_ms","time_sleep","sqrt","cbrt","floor","ceil","round","abs","sin","cos",
        "pow","min","max","str","assert","math_sqrt","math_cbrt","math_floor","math_ceil","math_round",
        "math_abs","math_sin","math_cos","math_pow","math_min","math_max"};
    auto function_symbol = [](std::string symbol) {
        const std::string old_prefix = "(int64_t)(uintptr_t)";
        if (symbol.starts_with(old_prefix)) symbol = symbol.substr(old_prefix.size());
        if (symbol.starts_with("nv_fn_")) return symbol.substr(6);
        return symbol;
    };
    for (const auto& function : module.functions) {
        if (functions.count(function->name)) {
            diag_.error("native compilation requires unique function names", SourceSpan{});
            return {};
        }
        functions[function->name] = function.get();
        for (const auto& block : function->blocks) for (const auto& instruction : block->instructions) {
            if (instruction.dest_reg >= 0) defined[function->name].insert(instruction.dest_reg);
            if (instruction.opcode == IROpcode::ALLOCA) cells[function->name].insert(instruction.dest_reg);
        }
    }
    for (const auto& function : module.functions) for (const auto& block : function->blocks)
        for (const auto& instruction : block->instructions) {
            for (const auto& arg : instruction.args) {
                if (arg.kind == OperandKind::REGISTER && !defined[function->name].count(arg.reg_id))
                    captures[function->name].insert(arg.reg_id);
                if (arg.kind == OperandKind::SYMBOL) {
                    auto name = function_symbol(arg.str_val);
                    if (functions.count(name)) references[function->name].insert(name);
                }
            }
            if (instruction.opcode == IROpcode::CALL && instruction.args[0].kind == OperandKind::SYMBOL) {
                const auto& name = instruction.args[0].str_val;
                if (!functions.count(name) && !builtins.count(name)) {
                    diag_.error("native builtin is not supported: " + name, instruction.span,
                                "run this program with the interpreter");
                    return {};
                }
            }
        }
    // A closure must retain transitive free cells needed to construct inner closures.
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& [name, deps] : references) for (const auto& dep : deps)
            for (int reg : captures[dep]) if (!defined[name].count(reg))
                changed |= captures[name].insert(reg).second;
    }
    if (!captures["__nv_entry_main"].empty()) {
        diag_.error("invalid native capture outside entry scope", SourceSpan{});
        return {};
    }
    for (const auto& [name, function] : functions) {
        (void)function;
        out << "static NV nv_fn_" << name << "(const NVArgs&, const NVCells&);\n";
    }
    for (const auto& function : module.functions) {
        const auto& name = function->name;
        auto closure = [&](const std::string& function_name) {
            const auto& free_cells = captures[function_name];
            std::ostringstream text;
            text << "nv_function([";
            bool first = true;
            for (int reg : free_cells) { if (!first) text << ','; first=false; text << "r" << reg; }
            text << "](const NVArgs& a){return nv_fn_" << function_name << "(a,{";
            first=true;
            for (int reg : free_cells) { if (!first) text << ','; first=false; text << "r" << reg; }
            text << "});}," << functions.at(function_name)->params.size() << ",\"" << c_escape_string(function_name) << "\")";
            return text.str();
        };
        auto operand = [&](const IROperand& arg) -> std::string {
            switch (arg.kind) {
                case OperandKind::REGISTER: return "r" + std::to_string(arg.reg_id);
                case OperandKind::CONSTANT_INT: return "nv_i(" + std::to_string(arg.int_val) + "LL)";
                case OperandKind::CONSTANT_FLOAT: {
                    std::ostringstream value; value << std::setprecision(17) << arg.float_val;
                    return "nv_f(" + value.str() + ")";
                }
                case OperandKind::CONSTANT_BOOL: return arg.bool_val ? "nv_b(true)" : "nv_b(false)";
                case OperandKind::CONSTANT_STRING: return "nv_s(std::string(\"" + c_escape_string(arg.str_val) + "\"," + std::to_string(arg.str_val.size()) + "))";
                case OperandKind::SYMBOL: {
                    for (size_t i=0; i<function->params.size(); ++i)
                        if (function->params[i].first == arg.str_val) return "args.at(" + std::to_string(i) + ")";
                    auto symbol = function_symbol(arg.str_val);
                    if (functions.count(symbol)) return closure(symbol);
                    diag_.error("unsupported native value: " + arg.str_val, SourceSpan{});
                    return "NV{}";
                }
                default: return "NV{}";
            }
        };
        out << "static NV nv_fn_" << name << "(const NVArgs& args, const NVCells& captured) {\n";
        out << "  (void)args; (void)captured;\n";
        for (size_t i=0; i<function->param_annotations.size(); ++i)
            out << "nv_checked(args.at(" << i << "),\"" << c_escape_string(function->param_annotations[i]) << "\");\n";
        size_t index=0;
        for (int reg : captures[name]) out << "  auto r" << reg << " = captured.at(" << index++ << ");\n";
        for (int reg : defined[name]) {
            if (cells[name].count(reg)) out << "  auto r" << reg << " = std::make_shared<NV>();\n";
            else out << "  NV r" << reg << ";\n";
        }
        for (const auto& block : function->blocks) {
            out << block->label << ":;\n";
            for (const auto& instruction : block->instructions) {
                auto dest = "r" + std::to_string(instruction.dest_reg);
                const auto& args = instruction.args;
                switch (instruction.opcode) {
                    case IROpcode::ALLOCA: break;
                    case IROpcode::CONST_INT: case IROpcode::CONST_FLOAT:
                    case IROpcode::CONST_BOOL: case IROpcode::CONST_STRING:
                        out << dest << " = " << operand(args[0]) << ";\n"; break;
                    case IROpcode::CONST_NIL: out << dest << " = NV{};\n"; break;
                    case IROpcode::LOAD: out << dest << " = *" << operand(args[0]) << ";\n"; break;
                    case IROpcode::STORE: out << '*' << operand(args[0]) << " = " << operand(args[1]) << ";\n"; break;
                    case IROpcode::NEG:
                        out << dest << " = nv_binary(\"sub\",nv_i(0)," << operand(args[0]) << ");\n"; break;
                    case IROpcode::NOT:
                        out << dest << " = nv_b(!nv_truth(" << operand(args[0]) << "));\n"; break;
                    case IROpcode::JMP: out << "goto " << args[0].str_val << ";\n"; break;
                    case IROpcode::JMP_IF_TRUE: case IROpcode::JMP_IF_FALSE:
                        out << "if (" << (instruction.opcode==IROpcode::JMP_IF_FALSE?"!":"") << "nv_truth(" << operand(args[0]) << ")) goto " << args[1].str_val << ";\n"; break;
                    case IROpcode::CALL: {
                        if (args[0].kind==OperandKind::SYMBOL && !functions.count(args[0].str_val))
                            out << dest << " = nv_builtin(\"" << c_escape_string(args[0].str_val) << "\",{";
                        else out << dest << " = nv_call(" << operand(args[0]) << ",{";
                        for (size_t i=1;i<args.size();++i) { if(i>1)out<<',';out<<operand(args[i]); }
                        out << "});\n"; break;
                    }
                    case IROpcode::PRINT:
                        out << "nv_builtin(\"print\",{";
                        for (size_t i=0;i<args.size();++i) { if(i)out<<',';out<<operand(args[i]); }
                        out << "});\n"; break;
                    case IROpcode::RET:
                        out << "return nv_checked(" << (args.empty()?"NV{}":operand(args[0])) << ",\"" << c_escape_string(function->return_annotation) << "\");\n"; break;
                    default:
                        out << dest << " = nv_binary(\"" << ir_opcode_to_string(instruction.opcode) << "\"," << operand(args[0]) << ',' << operand(args[1]) << ");\n";
                        break;
                }
            }
        }
        out << "return NV{};\n}\n";
    }
    out << "int main() { try { nv_fn___nv_entry_main({},{}); return 0; } catch(const std::exception& e) { std::cerr << \"runtime error: \" << e.what() << '\\n'; return 1; } }\n";
    return diag_.has_errors() ? std::string{} : out.str();
}

bool NativeCompiler::compile_to_binary(const IRModule& module, const std::string& output_binary_path, bool debug) {
    std::string c_src = emit_native_source(module);
    if (c_src.empty() || diag_.has_errors()) return false;
    std::string temp_c = output_binary_path + ".tmp.cpp";

    std::ofstream out(temp_c);
    if (!out.is_open()) return false;
    out << c_src;
    out.close();

    std::string compiler = find_c_compiler();

#if !defined(_WIN32)
    std::vector<std::string> args = {"-std=c++20", debug ? "-O0" : "-O2", temp_c, "-o", output_binary_path, "-lstdc++", "-lm", "-pthread"};
    if (debug) args.push_back("-g");
    int ret = safe_run_process(compiler, args);
#else
    (void)debug;
    diag_.error("native compilation currently requires a POSIX platform", SourceSpan{});
    int ret = -1;
#endif

    std::remove(temp_c.c_str());
    return ret == 0;
}

bool NativeCompiler::compile_program(const Program& program, const std::string& output_binary_path, bool optimize) {
    TypeChecker checker(diag_);
    if (!checker.check(program)) return false;
    IRGenerator gen(diag_);
    auto ir_module = gen.generate(program);
    if (!ir_module || diag_.has_errors()) return false;

    if (optimize) {
        IROptimizer opt;
        opt.optimize(*ir_module);
    }

    return compile_to_binary(*ir_module, output_binary_path);
}

std::pair<int, std::string> NativeCompiler::compile_and_run(const Program& program, bool optimize) {
    std::string temp_bin = "/tmp/nextviper_native_exec_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    if (!compile_program(program, temp_bin, optimize)) {
        return {-1, "Compilation failed"};
    }

#if !defined(_WIN32)
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        std::remove(temp_bin.c_str());
        return {-1, "Failed to create execution pipe"};
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addclose(&actions, pipefd[0]);
    posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipefd[1]);

    char* argv[] = {const_cast<char*>(temp_bin.c_str()), nullptr};
    pid_t pid;
    int spawn_res = posix_spawn(&pid, temp_bin.c_str(), &actions, nullptr, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(pipefd[1]);

    if (spawn_res != 0) {
        close(pipefd[0]);
        std::remove(temp_bin.c_str());
        return {-1, "Failed to spawn compiled binary"};
    }

    std::string output;
    char buffer[256];
    ssize_t bytes_read;
    while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        output += buffer;
    }
    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    std::remove(temp_bin.c_str());
    return {exit_code, output};
#else
    std::string output;
    std::array<char, 256> buffer;
    FILE* pipe = popen(("\"" + temp_bin + "\"").c_str(), "r");
    if (!pipe) {
        std::remove(temp_bin.c_str());
        return {-1, "Failed to run compiled binary"};
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        output += buffer.data();
    }

    int exit_code = pclose(pipe);
    std::remove(temp_bin.c_str());
    return {exit_code, output};
#endif
}

BenchmarkResult NativeCompiler::benchmark(const Program& program, const std::string& source_code, int iterations) {
    (void)source_code;
    BenchmarkResult res;
    res.iterations = iterations;

    // 1. Benchmark AST Interpreter
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            SourceManager sm;
            DiagnosticEngine diag(sm, false);
            Interpreter interp(diag);
            interp.execute(program);
        }
        auto end = std::chrono::high_resolution_clock::now();
        res.interpreter_ms = std::chrono::duration<double, std::milli>(end - start).count() / iterations;
    }

    // 2. Benchmark Bytecode VM
    {
        SourceManager sm;
        DiagnosticEngine diag(sm, false);
        BytecodeCompiler compiler(diag);
        auto fn = compiler.compile(program);

        if (fn) {
            auto start = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < iterations; ++i) {
                VM vm(diag);
                vm.run(fn);
            }
            auto end = std::chrono::high_resolution_clock::now();
            res.vm_ms = std::chrono::duration<double, std::milli>(end - start).count() / iterations;
        }
    }

    // 3. Benchmark Native Compiled Machine Code
    {
        std::string temp_bin = "/tmp/nextviper_bench_bin";
        if (compile_program(program, temp_bin, true)) {
            auto start = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < iterations; ++i) {
#if !defined(_WIN32)
                std::vector<std::string> bench_args;
                safe_run_process(temp_bin, bench_args);
#else
                int r = std::system(("\"" + temp_bin + "\" > NUL 2>&1").c_str());
                (void)r;
#endif
            }
            auto end = std::chrono::high_resolution_clock::now();
            res.native_ms = std::chrono::duration<double, std::milli>(end - start).count() / iterations;
            std::remove(temp_bin.c_str());
        }
    }

    if (res.native_ms > 0.0) {
        res.native_speedup_vs_interpreter = res.interpreter_ms / res.native_ms;
        res.native_speedup_vs_vm = res.vm_ms / res.native_ms;
    }

    return res;
}

} // namespace nextviper
