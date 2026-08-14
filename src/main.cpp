#include "nextviper/version.hpp"
#include "nextviper/diagnostic.hpp"
#include "nextviper/lexer.hpp"
#include "nextviper/parser.hpp"
#include "nextviper/interpreter.hpp"
#include "nextviper/compiler.hpp"
#include "nextviper/vm.hpp"
#include "nextviper/repl.hpp"
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
              << "  run <file.nv>       Execute a NextViper program file (High-Speed Bytecode VM)\n"
              << "  bench <file.nv>     Benchmark program execution time\n"
              << "  disasm <file.nv>    Disassemble program to bytecode instructions\n"
              << "  eval <code>, -e     Evaluate an inline NextViper code string\n"
              << "  check <file.nv>     Validate syntax and check for errors without running\n"
              << "  parse <file.nv>     Parse source file and display AST\n"
              << "  tokens <file.nv>    Scan source file and dump token stream\n"
              << "  repl                Start interactive REPL session\n"
              << "  version, -v         Display version information\n"
              << "  help, -h            Display this help message\n\n"
              << "Options:\n"
              << "  --tree              Use AST tree-walk interpreter instead of Bytecode VM\n\n"
              << "Examples:\n"
              << "  nextviper run examples/hello_world.nv\n"
              << "  nextviper bench examples/fibonacci.nv\n"
              << "  nextviper disasm examples/basics.nv\n"
              << "  nextviper -e 'let x = 40 + 2; print(x)'\n";
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
        if (!interpreter.execute(*program)) {
            diagnostics.render(std::cerr);
            return 1;
        }
        return 0;
    }

    // Default fast Bytecode VM execution
    VM vm(diagnostics);
    VMResult res = vm.execute(*program);
    if (res != VMResult::OK) {
        diagnostics.render(std::cerr);
        return 1;
    }

    return 0;
}

int disasm_file(const std::string& path) {
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

    BytecodeCompiler compiler(diagnostics);
    auto fn = compiler.compile(*program);

    if (diagnostics.has_errors() || !fn) {
        diagnostics.render(std::cerr);
        return 1;
    }

    fn->chunk().disassemble(path);
    return 0;
}

int bench_file(const std::string& path) {
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

    BytecodeCompiler compiler(diagnostics);
    auto fn = compiler.compile(*program);

    if (diagnostics.has_errors() || !fn) {
        diagnostics.render(std::cerr);
        return 1;
    }

    std::cout << "\033[1;36m=== Benchmarking NextViper VM on " << path << " ===\033[0m\n";

    constexpr int ITERATIONS = 5;
    double total_ms = 0.0;
    double min_ms = 1e9;
    double max_ms = 0.0;

    for (int i = 0; i < ITERATIONS; ++i) {
        VM vm(diagnostics);
        auto t0 = std::chrono::high_resolution_clock::now();
        VMResult res = vm.run(fn);
        auto t1 = std::chrono::high_resolution_clock::now();

        if (res != VMResult::OK) {
            diagnostics.render(std::cerr);
            return 1;
        }

        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        total_ms += ms;
        if (ms < min_ms) min_ms = ms;
        if (ms > max_ms) max_ms = ms;
    }

    std::cout << "\n\033[1;32mResults (" << ITERATIONS << " runs):\033[0m\n";
    std::cout << "  Average: " << (total_ms / ITERATIONS) << " ms\n";
    std::cout << "  Min:     " << min_ms << " ms\n";
    std::cout << "  Max:     " << max_ms << " ms\n";
    return 0;
}

int check_file(const std::string& path) {
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

    std::cout << "\033[1;32m✓\033[0m Syntax check passed: " << path << "\n";
    return 0;
}

int parse_file(const std::string& path) {
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

    ASTPrinter printer;
    std::cout << printer.print(*program);
    return 0;
}

int tokens_file(const std::string& path) {
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

    VM vm(diagnostics);
    if (vm.execute(*program) != VMResult::OK) {
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
            // Read from pipe
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

    // Default: treat first arg as file path if ends in .nv or exists
    return nextviper::run_file(first_arg);
}
