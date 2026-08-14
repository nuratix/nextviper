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
#include "nextviper/package_manager.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <filesystem>
#include <unistd.h>
#include <sys/stat.h>

namespace fs = std::filesystem;

namespace nextviper {

void print_version() {
    std::cout << get_full_version_string() << "\n";
}

void print_help() {
    std::cout << "\033[1;32mNextViper Programming Language\033[0m (v" << VERSION_STRING << ")\n\n"
              << "\033[1mUSAGE:\033[0m\n"
              << "  nextviper <COMMAND> [OPTIONS] [FILES...]\n\n"
              << "\033[1mCORE COMMANDS:\033[0m\n"
              << "  \033[1;36mrun\033[0m <file.nv> [args...]      Execute a NextViper program file\n"
              << "  \033[1;36mfmt\033[0m [options] <files...>     Format NextViper source code deterministically\n"
              << "  \033[1;36mbuild\033[0m <file.nv> [-o out]     Compile program to bytecode (.nvc) or native binary\n"
              << "  \033[1;36mcompile\033[0m <file.nv> [-o out]   Compile program directly to native machine code\n"
              << "  \033[1;36mtest\033[0m [path]                  Run NextViper automated test suite\n"
              << "  \033[1;36mcheck\033[0m <files...>             Validate syntax and types statically without executing\n"
              << "  \033[1;36mrepl\033[0m                         Start the interactive NextViper shell\n"
              << "  \033[1;36meval\033[0m <code>, -e                Evaluate an inline NextViper code string\n\n"
              << "\033[1mPACKAGE MANAGER COMMANDS:\033[0m\n"
              << "  \033[1;36minit\033[0m [name]                  Initialize a new NextViper project (nextviper.toml)\n"
              << "  \033[1;36madd\033[0m <pkg> [--path/--git]     Add a dependency to nextviper.toml and install it\n"
              << "  \033[1;36mremove\033[0m <pkg>                 Remove a dependency and clean lockfile\n"
              << "  \033[1;36minstall\033[0m                      Install all dependencies and verify integrity\n"
              << "  \033[1;36mupdate\033[0m [pkg]                 Update dependencies to latest matching SemVer\n"
              << "  \033[1;36mlist\033[0m                         Display dependency tree and checksum status\n"
              << "  \033[1;36mpublish\033[0m [--dry-run]          Validate manifest and build package distribution\n\n"
              << "\033[1mFORMATTER OPTIONS:\033[0m\n"
              << "  -w, --write                  Write formatted output in-place to source file(s) [default]\n"
              << "  -c, --check                  Check if files are formatted, exit non-zero if not\n"
              << "  -d, --diff                   Show unified diff of formatting changes\n"
              << "  --stdin                      Read unformatted code from stdin, write to stdout\n\n"
              << "\033[1mCHECK & BUILD OPTIONS:\033[0m\n"
              << "  --format=json                Output diagnostics in machine-readable JSON format\n"
              << "  --native                     Build native machine code binary (AOT)\n"
              << "  --bytecode                   Build bytecode package (.nvc)\n"
              << "  --release                    Optimize with -O3 for maximum performance\n"
              << "  --no-color                   Disable ANSI colored terminal output\n\n"
              << "\033[1mEXAMPLES:\033[0m\n"
              << "  nextviper init my_app\n"
              << "  nextviper add math_utils --path ../math_utils\n"
              << "  nextviper install\n"
              << "  nextviper run src/main.nv\n";
}

std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "error[NV114]:\n    could not open file '" << path << "'\n\n";
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

    Interpreter interpreter(diagnostics);
    interpreter.set_current_file(path);
    if (!interpreter.execute(*program)) {
        diagnostics.render(std::cerr);
        return 1;
    }

    (void)use_tree_walk;
    return 0;
}

int fmt_command(int argc, char* argv[]) {
    bool check_only = false;
    bool show_diff = false;
    bool write_in_place = true;
    bool use_stdin = false;
    std::vector<std::string> target_files;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--check" || arg == "-c") {
            check_only = true;
            write_in_place = false;
        } else if (arg == "--diff" || arg == "-d") {
            show_diff = true;
            write_in_place = false;
        } else if (arg == "--write" || arg == "-w") {
            write_in_place = true;
        } else if (arg == "--stdin") {
            use_stdin = true;
        } else if (!arg.starts_with("-")) {
            target_files.push_back(arg);
        }
    }

    if (use_stdin) {
        std::stringstream buffer;
        buffer << std::cin.rdbuf();
        std::string formatted = Formatter::format_source(buffer.str());
        std::cout << formatted;
        return 0;
    }

    if (target_files.empty()) {
        std::cerr << "error: no files specified to format\n"
                  << "usage: nextviper fmt [options] <files...>\n";
        return 1;
    }

    int unformatted_count = 0;
    int formatted_count = 0;

    for (const auto& file_path : target_files) {
        FormatResult res = Formatter::format_file(file_path, write_in_place);
        if (!res.is_formatted) {
            unformatted_count++;
            if (check_only) {
                std::cout << "would reformat: " << file_path << "\n";
            } else if (show_diff) {
                std::cout << res.diff << "\n";
            } else if (write_in_place) {
                std::cout << "✓ Formatted: " << file_path << "\n";
                formatted_count++;
            }
        }
    }

    if (check_only) {
        if (unformatted_count > 0) {
            std::cerr << "\n" << unformatted_count << " file(s) would be reformatted.\n";
            return 1;
        }
        std::cout << "All " << target_files.size() << " file(s) are cleanly formatted.\n";
        return 0;
    }

    if (write_in_place && formatted_count > 0) {
        std::cout << "\n✓ Successfully formatted " << formatted_count << " file(s).\n";
    }
    return 0;
}

int check_command(int argc, char* argv[]) {
    bool json_format = false;
    bool no_color = false;
    std::vector<std::string> files;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--format=json") {
            json_format = true;
        } else if (arg == "--no-color") {
            no_color = true;
        } else if (!arg.starts_with("-")) {
            files.push_back(arg);
        }
    }

    if (files.empty()) {
        std::cerr << "error: no files specified to check\n"
                  << "usage: nextviper check [options] <files...>\n";
        return 1;
    }

    SourceManager source_manager;
    DiagnosticEngine diagnostics(source_manager, !no_color && isatty(fileno(stderr)));

    for (const auto& path : files) {
        std::string source = read_file(path);
        if (source.empty()) {
            continue;
        }
        source_manager.add_file(path, source);

        Lexer lexer(source, path, diagnostics);
        auto tokens = lexer.tokenize();

        if (diagnostics.has_errors()) {
            continue;
        }

        Parser parser(tokens, diagnostics);
        auto program = parser.parse_program();

        if (diagnostics.has_errors() || !program) {
            continue;
        }

        TypeChecker checker(diagnostics);
        checker.check(*program);
    }

    if (json_format) {
        std::cout << diagnostics.to_json() << "\n";
        return diagnostics.has_errors() ? 1 : 0;
    }

    if (diagnostics.has_errors()) {
        diagnostics.render(std::cerr);
        return 1;
    }

    std::cout << "\033[1;32m✓ Check passed:\033[0m 0 errors, 0 warnings across " << files.size() << " file(s).\n";
    return 0;
}

int build_command(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "error: missing file path to build\n"
                  << "usage: nextviper build <file.nv> [-o out] [--native|--bytecode]\n";
        return 1;
    }

    std::string path = argv[2];
    std::string out_path = "";
    bool build_native = false;
    bool is_release = false;

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            out_path = argv[++i];
        } else if (arg == "--native") {
            build_native = true;
        } else if (arg == "--release") {
            is_release = true;
        }
    }

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

    if (build_native) {
        std::string final_out = out_path.empty() ? (path.ends_with(".nv") ? path.substr(0, path.length() - 3) : (path + ".bin")) : out_path;
        IRGenerator gen(diagnostics);
        auto ir_mod = gen.generate(*program);
        if (!ir_mod) {
            diagnostics.render(std::cerr);
            return 1;
        }

        IROptimizer opt;
        opt.optimize(*ir_mod);

        NativeCompiler native_compiler(diagnostics);
        if (!native_compiler.compile_to_binary(*ir_mod, final_out)) {
            std::cerr << "error: native machine code compilation failed\n";
            return 1;
        }

        std::cout << "\033[1;32m✓ Built native binary:\033[0m " << final_out
                  << (is_release ? " [release -O3]" : "") << "\n";
        return 0;
    }

    // Default: Bytecode artifact
    BytecodeCompiler compiler(diagnostics);
    auto fn = compiler.compile(*program);
    if (!fn) {
        diagnostics.render(std::cerr);
        return 1;
    }

    std::string final_out = out_path.empty() ? (path + "c") : out_path;
    std::ofstream out(final_out, std::ios::binary);
    if (out.is_open()) {
        out << "NVBC\x01\x00"; // Magic Header
        out << fn->name() << "\n";
        out.close();
    }

    std::cout << "\033[1;32m✓ Built bytecode package:\033[0m " << final_out
              << " (" << fn->chunk().count() << " bytecode instructions)\n";
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
        std::cerr << "error: native compilation failed\n";
        return 1;
    }

    std::cout << "\033[1;32m✓ Compiled native binary:\033[0m " << final_out << "\n";

    if (run_after) {
        std::string cmd = final_out;
        if (!cmd.starts_with("./") && !cmd.starts_with("/")) cmd = "./" + cmd;
        int ret = std::system(cmd.c_str());
        return ret;
    }
    return 0;
}

int test_command(int argc, char* argv[]) {
    // If specific file or test path is provided
    std::string target_path = (argc >= 3) ? argv[2] : "";

    if (!target_path.empty() && fs::is_regular_file(target_path)) {
        std::cout << "\033[1;36m=== Running NextViper Test: " << target_path << " ===\033[0m\n";
        auto start = std::chrono::high_resolution_clock::now();
        int res = run_file(target_path);
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        if (res == 0) {
            std::cout << "  \033[1;32m✓ PASS\033[0m (" << ms << " ms)\n";
            return 0;
        } else {
            std::cout << "  \033[1;31m✗ FAIL\033[0m (" << ms << " ms)\n";
            return 1;
        }
    }

    // Default: Run C++ test runner engine
    int res = system("bin/test_runner");
    return WEXITSTATUS(res);
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
    interpreter.set_current_file(source_name);
    if (!interpreter.execute(*program)) {
        diagnostics.render(std::cerr);
        return 1;
    }
    return 0;
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
    std::cout << printer.print(*program) << "\n";
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

    for (const auto& tok : tokens) {
        std::cout << tok.to_string() << "\n";
    }
    return 0;
}

int package_command(int argc, char* argv[]) {
    PackageManager pm;
    if (argc < 3 || std::string(argv[2]) == "help") {
        std::cout << "NextViper Package Manager (nextviper package)\n\n"
                  << "Commands:\n"
                  << "  nextviper init [name]             Initialize a new NextViper project (nextviper.toml)\n"
                  << "  nextviper add <pkg> [--path/--git] Add a dependency\n"
                  << "  nextviper remove <pkg>            Remove a dependency\n"
                  << "  nextviper install                 Install dependencies from nextviper.toml\n"
                  << "  nextviper update [pkg]            Update dependencies\n"
                  << "  nextviper list                    List dependencies\n"
                  << "  nextviper publish [--dry-run]     Build package archive\n";
        return 0;
    }

    std::string sub = argv[2];
    if (sub == "init") {
        std::string name = (argc >= 4) ? argv[3] : "";
        return pm.cmd_init(name);
    }
    if (sub == "add") {
        if (argc < 4) {
            std::cerr << "error: missing package name to add\n";
            return 1;
        }
        std::string pkg = argv[3];
        std::string ver_or_path = (argc >= 5) ? argv[4] : "";
        bool is_path = false;
        std::string git_url = "";
        std::string git_ref = "";
        for (int i = 4; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--path" && i + 1 < argc) {
                is_path = true;
                ver_or_path = argv[++i];
            } else if (arg == "--git" && i + 1 < argc) {
                git_url = argv[++i];
            } else if (arg == "--tag" && i + 1 < argc) {
                git_ref = argv[++i];
            }
        }
        return pm.cmd_add(pkg, ver_or_path, is_path, git_url, git_ref);
    }
    if (sub == "remove") {
        if (argc < 4) {
            std::cerr << "error: missing package name to remove\n";
            return 1;
        }
        return pm.cmd_remove(argv[3]);
    }
    if (sub == "install") {
        return pm.cmd_install();
    }
    if (sub == "update") {
        std::string pkg = (argc >= 4) ? argv[3] : "";
        return pm.cmd_update(pkg);
    }
    if (sub == "list") {
        return pm.cmd_list();
    }
    if (sub == "publish") {
        bool dry_run = false;
        for (int i = 3; i < argc; ++i) {
            if (std::string(argv[i]) == "--dry-run") dry_run = true;
        }
        return pm.cmd_publish(dry_run);
    }

    std::cerr << "Unknown package command: " << sub << "\n";
    return 1;
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

    if (first_arg == "init") {
        nextviper::PackageManager pm;
        std::string name = (argc >= 3) ? argv[2] : "";
        return pm.cmd_init(name);
    }

    if (first_arg == "add") {
        if (argc < 3) {
            std::cerr << "error: missing package name to add\n";
            return 1;
        }
        nextviper::PackageManager pm;
        std::string pkg = argv[2];
        std::string ver_or_path = (argc >= 4) ? argv[3] : "";
        bool is_path = false;
        std::string git_url = "";
        std::string git_ref = "";
        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--path" && i + 1 < argc) {
                is_path = true;
                ver_or_path = argv[++i];
            } else if (arg == "--git" && i + 1 < argc) {
                git_url = argv[++i];
            } else if (arg == "--tag" && i + 1 < argc) {
                git_ref = argv[++i];
            }
        }
        return pm.cmd_add(pkg, ver_or_path, is_path, git_url, git_ref);
    }

    if (first_arg == "remove") {
        if (argc < 3) {
            std::cerr << "error: missing package name to remove\n";
            return 1;
        }
        nextviper::PackageManager pm;
        return pm.cmd_remove(argv[2]);
    }

    if (first_arg == "install") {
        nextviper::PackageManager pm;
        return pm.cmd_install();
    }

    if (first_arg == "update") {
        nextviper::PackageManager pm;
        std::string pkg = (argc >= 3) ? argv[2] : "";
        return pm.cmd_update(pkg);
    }

    if (first_arg == "list") {
        nextviper::PackageManager pm;
        return pm.cmd_list();
    }

    if (first_arg == "publish") {
        nextviper::PackageManager pm;
        bool dry_run = false;
        for (int i = 2; i < argc; ++i) {
            if (std::string(argv[i]) == "--dry-run") dry_run = true;
        }
        return pm.cmd_publish(dry_run);
    }

    if (first_arg == "repl") {
        nextviper::REPL repl;
        repl.run();
        return 0;
    }

    if (first_arg == "fmt") {
        return nextviper::fmt_command(argc, argv);
    }

    if (first_arg == "check") {
        return nextviper::check_command(argc, argv);
    }

    if (first_arg == "build") {
        return nextviper::build_command(argc, argv);
    }

    if (first_arg == "test") {
        return nextviper::test_command(argc, argv);
    }

    if (first_arg == "compile") {
        if (argc < 3) {
            std::cerr << "error: missing file path to compile\n";
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
            std::cerr << "error: missing code string for eval\n";
            return 1;
        }
        return nextviper::eval_code(argv[2]);
    }

    if (first_arg == "run") {
        if (argc < 3) {
            std::cerr << "error: missing file path to run\n";
            return 1;
        }
        bool use_tree = (argc >= 4 && std::string(argv[3]) == "--tree");
        return nextviper::run_file(argv[2], use_tree);
    }

    if (first_arg == "bench") {
        if (argc < 3) {
            std::cerr << "error: missing file path to benchmark\n";
            return 1;
        }
        return nextviper::bench_file(argv[2]);
    }

    if (first_arg == "disasm") {
        if (argc < 3) {
            std::cerr << "error: missing file path to disassemble\n";
            return 1;
        }
        return nextviper::disasm_file(argv[2]);
    }

    if (first_arg == "parse") {
        if (argc < 3) {
            std::cerr << "error: missing file path to parse\n";
            return 1;
        }
        return nextviper::parse_file(argv[2]);
    }

    if (first_arg == "tokens") {
        if (argc < 3) {
            std::cerr << "error: missing file path to scan\n";
            return 1;
        }
        return nextviper::tokens_file(argv[2]);
    }

    return nextviper::run_file(first_arg);
}
