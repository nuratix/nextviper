#include "nextviper/lsp.hpp"
#include "nextviper/version.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc >= 2) {
        std::string arg = argv[1];
        if (arg == "--version" || arg == "-v") {
            std::cout << "nextviper-lsp v" << nextviper::VERSION_STRING << "\n";
            return 0;
        }
        if (arg == "--help" || arg == "-h") {
            std::cout << "NextViper Language Server (nextviper-lsp)\n\n"
                      << "USAGE:\n"
                      << "  nextviper-lsp [options]\n"
                      << "  nextviper lsp\n\n"
                      << "OPTIONS:\n"
                      << "  --version, -v   Display version information\n"
                      << "  --help, -h      Display this help message\n\n"
                      << "The NextViper Language Server communicates using JSON-RPC 2.0 over standard I/O.\n";
            return 0;
        }
    }

    nextviper::LanguageServer server(std::cin, std::cout);
    server.run();
    return 0;
}
