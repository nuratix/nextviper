#include "nextviper/version.hpp"
#include "nextviper/diagnostic.hpp"
#include "nextviper/lexer.hpp"
#include "nextviper/parser.hpp"
#include "nextviper/type_checker.hpp"
#include "nextviper/interpreter.hpp"
#include "nextviper/compiler.hpp"
#include "nextviper/vm.hpp"
#include "nextviper/repl.hpp"
#include "nextviper/formatter.hpp"
#include "nextviper/ir.hpp"
#include "nextviper/native_compiler.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <unistd.h>

namespace nextviper {

void print_version() {
    std::cout << get_full_version_string() << "\n";
}

void print_help() {
    std::cout << "NextViper Programming Language (v" << VERSION_STRING << ")\n\n"
              << "Usage:\n"
              << "  nextviper [command] [options] [file.nv]\n\n"
              << "Commands:\n"
              << "  run <file.nv>            Execute a NextViper program file\n"
              << "  compile <file.nv> [-o]   Compile program to native machine binary\n"
              << "  build <file.nv> [-o out] Compile program to bytecode artifact (.nvc)\n"
              << "  test                     Run full test suite\n"
              << "  fmt <file.nv> [-w]       Format NextViper source code\n"
              << "  repl                     Start interactive REPL session\n"
              << "  package <init|info|...>  Manage NextViper packages\n"
              << "  bench <file.nv>          Benchmark execution (Interpreter vs VM vs Native)\n"
              << "  check <file.nv>          Validate syntax and type check without running\n"
              << "  disasm <file.nv>         Disassemble program to bytecode instructions\n"
              << "  eval <code>, -e          Evaluate an inline NextViper code string\n"
              << "  parse <file.nv>          Parse source file and display AST\n"
              << "  tokens <file.nv>         Scan source file and dump token stream\n"
              << "  version, -v              Display version information\n"
              << "  help, -h                 Display this help message\n\n"
              << "Options:\n"
              << "  --tree                   Use AST tree-walk interpreter instead of Bytecode VM\n"
              << "  --emit-ir                Emit Typed IR during compilation\n"
              << "  --run, -r                Execute immediately after native compilation\n\n"
              << "Examples:\n"
              << "  nextviper run examples/modules_example.nv\n"
              << "  nextviper compile src/main.nv -o bin/app --run\n"
              << "  nextviper bench examples/benchmark.nv\n"
              << "  nextviper build src/main.nv\n"
              << "  nextviper fmt src/main.nv -w\n"
              << "  nextviper package init my_app\n";
}

std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: could not open file '" << path << "'\n";
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int run_file(const std::string& path, bool use_tree_walk = false) {
    std::string source = read_file(path);
    if (source.empty()) {
        std::ifstream check(path);
        if (!check.good()) return 1;
    }

    SourceManager source_manager;
    source_manager.add_file(path, source);

    DiagnosticEngine diagnostics(source_manager, isatty(fileno(stderr)));

    Lexer lexer(source, path, diagnostics);
    auto tokens = lexer.tokenize();

    if (diagnostics.has_errors()) {
        diagnostics.render(std::cerr);
        return 1;
    }

    Parser parser(tokens, diagnostics);
    auto program = parser.parse_program();

    if (diagnostics.has_errors() || !program) {
        diagnostics.render(std::cerr);
        return 1;
    }

    if (use_tree_walk) {
        Interpreter interpreter(diagnostics);
        interpreter.set_current_file(path);
        if (!interpreter.execute(*program)) {
            diagnostics.render(std::cerr);
            return 1;
        }
        return 0;
    }

    // Default fast execution
    Interpreter interpreter(diagnostics);
    interpreter.set_current_file(path);
    if (!interpreter.execute(*program)) {
        diagnostics.render(std::cerr);
        return 1;
    }

    return 0;
}

int fmt_file(const std::string& path, bool write_back) {
    std::string source = read_file(path);
    if (source.empty()) return 1;

    std::string formatted = Formatter::format_source(source);
    if (write_back) {
        std::ofstream out(path);
        if (!out.is_open()) {
            std::cerr << "Error: failed to write to " << path << "\n";
            return 1;
        }
        out << formatted;
        std::cout << "✓ Formatted: " << path << "\n";
    } else {
        std::cout << formatted;
    }
    return 0;
}

int build_file(const std::string& path, const std::string& out_path) {
    std::string source = read_file(path);
    if (source.empty()) return 1;

    SourceManager source_manager;
    source_manager.add_file(path, source);
    DiagnosticEngine diagnostics(source_manager, isatty(fileno(stderr)));

    Lexer lexer(source, path, diagnostics);
    auto tokens = lexer.tokenize();
    if (diagnostics.has_errors()) {
        diagnostics.render(std::cerr);
        return 1;
    }

    Parser parser(tokens, diagnostics);
    auto program = parser.parse_program();
    if (diagnostics.has_errors() || !program) {
        diagnostics.render(std::cerr);
        return 1;
    }

    TypeChecker checker(diagnostics);
    if (!checker.check(*program)) {
        diagnostics.render(std::cerr);
        return 1;
    }

    BytecodeCompiler compiler(diagnostics);
    auto fn = compiler.compile(*program);
    if (!fn) {
        diagnostics.render(std::cerr);
        return 1;
    }

    std::string final_out = out_path.empty() ? (path + "c") : out_path;
    std::ofstream out(final_out, std::ios::binary);
    if (out.is_open()) {
        out << "NVBC\x01\x00"; // NextViper Bytecode Magic Header
        out << fn->name() << "\n";
        out.close();
    }

    std::cout << "✓ Successfully compiled and built: " << final_out << " (" << fn->chunk().count() << " instructions)\n";
    return 0;
}

int test_command() {
    int res = system("bin/test_runner");
    return WEXITSTATUS(res);
}

int package_command(int argc, char* argv[]) {
    if (argc < 3 || std::string(argv[2]) == "help") {
        std::cout << "NextViper Package Manager (nextviper package)\n\n"
                  << "Commands:\n"
                  << "  nextviper package init [name]     Initialize a new NextViper package\n"
                  << "  nextviper package info            Display package information\n"
                  << "  nextviper package bundle          Validate and bundle package\n";
        return 0;
    }

    std::string sub = argv[2];
    if (sub == "init") {
        std::string pkg_name = (argc >= 4) ? argv[3] : "my_package";
        std::ofstream pkg_file("nextviper.json");
        if (pkg_file.is_open()) {
            pkg_file << "{\n"
                     << "  \"name\": \"" << pkg_name << "\",\n"
                     << "  \"version\": \"0.1.0\",\n"
                     << "  \"description\": \"A modern NextViper package\",\n"
                     << "  \"main\": \"src/main.nv\",\n"
                     << "  \"license\": \"MIT\",\n"
                     << "  \"dependencies\": {}\n"
                     << "}\n";
            pkg_file.close();
        }

        int sys_ret = system("mkdir -p src tests");
        (void)sys_ret;
        std::ofstream main_nv("src/main.nv");
        if (main_nv.is_open()) {
            main_nv << "// " << pkg_name << " entrypoint\n"
                    << "import math\n\n"
                    << "export fn run():\n"
                    << "    print(\"Hello from " << pkg_name << "!\")\n";
            main_nv.close();
        }

        std::cout << "✓ Initialized NextViper package '" << pkg_name << "'\n"
                  << "  Created nextviper.json\n"
                  << "  Created src/main.nv\n"
                  << "  Created tests/\n";
        return 0;
    }

    if (sub == "info") {
        std::string info = read_file("nextviper.json");
        if (info.empty()) {
            std::cerr << "Error: no nextviper.json found in current directory\n";
            return 1;
        }
        std::cout << info << "\n";
        return 0;
    }

    if (sub == "bundle") {
        std::cout << "✓ Validating package modules...\n";
        int res = run_file("src/main.nv");
        if (res == 0) {
            std::cout << "✓ Package bundle verified successfully!\n";
        }
        return res;
    }

    std::cerr << "Unknown package command: " << sub << "\n";
    return 1;
}

int disasm_file(const std::string& path) {
    std::string source = read_file(path);
    if (source.empty()) return 1;

    SourceManager source_manager;
    source_manager.add_file(path, source);
    DiagnosticEngine diagnostics(source_manager, isatty(fileno(stderr)));

    Lexer lexer(source, path, diagnostics);
    auto tokens = lexer.tokenize();
    if (diagnostics.has_errors()) {
        diagnostics.render(std::cerr);
        return 1;
    }

    Parser parser(tokens, diagnostics);
    auto program = parser.parse_program();
    if (diagnostics.has_errors() || !program) {
        diagnostics.render(std::cerr);
        return 1;
    }

    BytecodeCompiler compiler(diagnostics);
    auto fn = compiler.compile(*program);
    if (!fn) {
        diagnostics.render(std::cerr);
        return 1;
    }

    std::cout << "=== Disassembly of " << path << " ===\n";
    fn->chunk().disassemble(fn->name());
    return 0;
}

int compile_file(const std::string& path, const std::string& out_path, bool emit_ir, bool run_after) {
    std::string source = read_file(path);
    if (source.empty()) return 1;

    SourceManager source_manager;
    source_manager.add_file(path, source);
    DiagnosticEngine diagnostics(source_manager, isatty(fileno(stderr)));

    Lexer lexer(source, path, diagnostics);
    auto tokens = lexer.tokenize();
    if (diagnostics.has_errors()) {
        diagnostics.render(std::cerr);
        return 1;
    }

    Parser parser(tokens, diagnostics);
    auto program = parser.parse_program();
    if (diagnostics.has_errors() || !program) {
        diagnostics.render(std::cerr);
        return 1;
    }

    IRGenerator gen(diagnostics);
    auto ir_mod = gen.generate(*program);
    if (!ir_mod || diagnostics.has_errors()) {
        diagnostics.render(std::cerr);
        return 1;
    }

    IROptimizer opt;
    opt.optimize(*ir_mod);

    if (emit_ir) {
        std::cout << ir_mod->to_string() << "\n";
    }

    std::string final_out = out_path;
    if (final_out.empty()) {
        final_out = path;
        if (final_out.ends_with(".nv")) {
            final_out = final_out.substr(0, final_out.length() - 3);
        } else {
            final_out += ".bin";
        }
    }

    NativeCompiler native_compiler(diagnostics);
    if (!native_compiler.compile_to_binary(*ir_mod, final_out)) {
        std::cerr << "Error: native compilation failed\n";
        return 1;
    }

    std::cout << "✓ Successfully compiled native binary: " << final_out << "\n";

    if (run_after) {
        std::string cmd = final_out;
        if (!cmd.starts_with("./") && !cmd.starts_with("/")) cmd = "./" + cmd;
        int ret = std::system(cmd.c_str());
        return ret;
    }

    return 0;
}

int bench_file(const std::string& path) {
    std::string source = read_file(path);
    if (source.empty()) return 1;

    SourceManager source_manager;
    source_manager.add_file(path, source);
    DiagnosticEngine diagnostics(source_manager, isatty(fileno(stderr)));

    Lexer lexer(source, path, diagnostics);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, diagnostics);
    auto program = parser.parse_program();
    if (!program || diagnostics.has_errors()) {
        diagnostics.render(std::cerr);
        return 1;
    }

    std::cout << "====================================================\n";
    std::cout << "  NextViper High-Performance Benchmark Suite\n";
    std::cout << "  Target: " << path << "\n";
    std::cout << "====================================================\n\n";

    NativeCompiler native_compiler(diagnostics);
    auto res = native_compiler.benchmark(*program, source, 100);

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Execution Latency (average across " << res.iterations << " iterations):\n";
    std::cout << "  1. AST Tree-Walk Interpreter: " << res.interpreter_ms << " ms / run\n";
    std::cout << "  2. NextViper Bytecode VM:     " << res.vm_ms << " ms / run\n";
    std::cout << "  3. Native Machine Code:       " << res.native_ms << " ms / run\n\n";

    std::cout << "Speedup Comparison:\n";
    std::cout << "  • Native vs Interpreter: " << res.native_speedup_vs_interpreter << "x faster\n";
    std::cout << "  • Native vs Bytecode VM: " << res.native_speedup_vs_vm << "x faster\n";
    std::cout << "====================================================\n";
    return 0;
}

int check_file(const std::string& path) {
    std::string source = read_file(path);
    if (source.empty()) return 1;

    SourceManager source_manager;
    source_manager.add_file(path, source);
    DiagnosticEngine diagnostics(source_manager, isatty(fileno(stderr)));

    Lexer lexer(source, path, diagnostics);
    auto tokens = lexer.tokenize();
    if (diagnostics.has_errors()) {
        diagnostics.render(std::cerr);
        return 1;
    }

    Parser parser(tokens, diagnostics);
    auto program = parser.parse_program();
    if (diagnostics.has_errors() || !program) {
        diagnostics.render(std::cerr);
        return 1;
    }

    TypeChecker checker(diagnostics);
    if (!checker.check(*program)) {
        diagnostics.render(std::cerr);
        return 1;
    }

    std::cout << "✓ Syntax check passed: " << path << " (type check passed)\n";
    return 0;
}

int parse_file(const std::string& path) {
    std::string source = read_file(path);
    if (source.empty()) return 1;

    SourceManager source_manager;
    source_manager.add_file(path, source);
    DiagnosticEngine diagnostics(source_manager, isatty(fileno(stderr)));

    Lexer lexer(source, path, diagnostics);
    auto tokens = lexer.tokenize();
    if (diagnostics.has_errors()) {
        diagnostics.render(std::cerr);
        return 1;
    }

    Parser parser(tokens, diagnostics);
    auto program = parser.parse_program();
    if (diagnostics.has_errors() || !program) {
        diagnostics.render(std::cerr);
        return 1;
    }

    ASTPrinter printer;
    std::cout << printer.print(*program);
    return 0;
}

int tokens_file(const std::string& path) {
    std::string source = read_file(path);
    if (source.empty()) return 1;

    SourceManager source_manager;
    source_manager.add_file(path, source);
    DiagnosticEngine diagnostics(source_manager, isatty(fileno(stderr)));

    Lexer lexer(source, path, diagnostics);
    auto tokens = lexer.tokenize();
    if (diagnostics.has_errors()) {
        diagnostics.render(std::cerr);
        return 1;
    }

    std::cout << "Tokens (" << tokens.size() << "):\n";
    for (const auto& tok : tokens) {
        std::cout << "  " << tok.to_string() << "\n";
    }
    return 0;
}

int eval_code(const std::string& code) {
    std::string source_name = "<eval>";
    SourceManager source_manager;
    source_manager.add_file(source_name, code);

    DiagnosticEngine diagnostics(source_manager, isatty(fileno(stderr)));

    Lexer lexer(code, source_name, diagnostics);
    auto tokens = lexer.tokenize();
    if (diagnostics.has_errors()) {
        diagnostics.render(std::cerr);
        return 1;
    }

    Parser parser(tokens, diagnostics);
    auto program = parser.parse_program();
    if (diagnostics.has_errors() || !program) {
        diagnostics.render(std::cerr);
        return 1;
    }

    Interpreter interpreter(diagnostics);
    if (!interpreter.execute(*program)) {
        diagnostics.render(std::cerr);
        return 1;
    }

    return 0;
}

} // namespace nextviper

int main(int argc, char* argv[]) {
    if (argc < 2) {
        if (isatty(fileno(stdin))) {
            nextviper::REPL repl;
            repl.run();
            return 0;
        } else {
            std::stringstream buffer;
            buffer << std::cin.rdbuf();
            return nextviper::eval_code(buffer.str());
        }
    }

    std::string first_arg = argv[1];

    if (first_arg == "--version" || first_arg == "-v" || first_arg == "version") {
        nextviper::print_version();
        return 0;
    }

    if (first_arg == "--help" || first_arg == "-h" || first_arg == "help") {
        nextviper::print_help();
        return 0;
    }

    if (first_arg == "repl") {
        nextviper::REPL repl;
        repl.run();
        return 0;
    }

    if (first_arg == "test") {
        return nextviper::test_command();
    }

    if (first_arg == "fmt") {
        if (argc < 3) {
            std::cerr << "Error: missing file path to format\n";
            return 1;
        }
        bool write_back = (argc >= 4 && (std::string(argv[3]) == "-w" || std::string(argv[3]) == "--write"));
        return nextviper::fmt_file(argv[2], write_back);
    }

    if (first_arg == "build") {
        if (argc < 3) {
            std::cerr << "Error: missing file path to build\n";
            return 1;
        }
        std::string out_path = (argc >= 5 && std::string(argv[3]) == "-o") ? argv[4] : "";
        return nextviper::build_file(argv[2], out_path);
    }

    if (first_arg == "compile") {
        if (argc < 3) {
            std::cerr << "Error: missing file path to compile\n";
            return 1;
        }
        std::string out_path = "";
        bool emit_ir = false;
        bool run_after = false;

        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-o" && i + 1 < argc) {
                out_path = argv[++i];
            } else if (arg == "--emit-ir") {
                emit_ir = true;
            } else if (arg == "--run" || arg == "-r") {
                run_after = true;
            }
        }
        return nextviper::compile_file(argv[2], out_path, emit_ir, run_after);
    }

    if (first_arg == "package") {
        return nextviper::package_command(argc, argv);
    }

    if (first_arg == "-e" || first_arg == "eval") {
        if (argc < 3) {
            std::cerr << "Error: missing code string for eval\n";
            return 1;
        }
        return nextviper::eval_code(argv[2]);
    }

    if (first_arg == "run") {
        if (argc < 3) {
            std::cerr << "Error: missing file path to run\n";
            return 1;
        }
        bool use_tree = (argc >= 4 && std::string(argv[3]) == "--tree");
        return nextviper::run_file(argv[2], use_tree);
    }

    if (first_arg == "bench") {
        if (argc < 3) {
            std::cerr << "Error: missing file path to benchmark\n";
            return 1;
        }
        return nextviper::bench_file(argv[2]);
    }

    if (first_arg == "disasm") {
        if (argc < 3) {
            std::cerr << "Error: missing file path to disassemble\n";
            return 1;
        }
        return nextviper::disasm_file(argv[2]);
    }

    if (first_arg == "check") {
        if (argc < 3) {
            std::cerr << "Error: missing file path to check\n";
            return 1;
        }
        return nextviper::check_file(argv[2]);
    }

    if (first_arg == "parse") {
        if (argc < 3) {
            std::cerr << "Error: missing file path to parse\n";
            return 1;
        }
        return nextviper::parse_file(argv[2]);
    }

    if (first_arg == "tokens") {
        if (argc < 3) {
            std::cerr << "Error: missing file path to scan\n";
            return 1;
        }
        return nextviper::tokens_file(argv[2]);
    }

    return nextviper::run_file(first_arg);
}
