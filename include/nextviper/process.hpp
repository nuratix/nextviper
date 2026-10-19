#pragma once

#include <cstdio>
#include <string>
#include <vector>
#include <stdexcept>
#if !defined(_WIN32)
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
extern "C" char** environ;
#endif

namespace nextviper {
struct ProcessOutput { int exit_code; std::string output; };

// No shell, predictable argv boundaries, anonymous capture file, bounded output.
inline ProcessOutput capture_process(const std::string& executable,
                                     const std::vector<std::string>& arguments,
                                     size_t limit = 16 * 1024 * 1024) {
#if defined(_WIN32)
    throw std::runtime_error("process capture is currently supported on POSIX platforms only");
#else
    FILE* output = std::tmpfile();
    if (!output) throw std::runtime_error("cannot create subprocess capture file");
    std::vector<char*> argv{const_cast<char*>(executable.c_str())};
    for (const auto& argument : arguments) {
        if (argument.find('\0') != std::string::npos) {
            std::fclose(output);
            throw std::runtime_error("subprocess arguments cannot contain NUL bytes");
        }
        argv.push_back(const_cast<char*>(argument.c_str()));
    }
    argv.push_back(nullptr);
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, fileno(output), STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, fileno(output), STDERR_FILENO);
    if (fileno(output) > STDERR_FILENO) posix_spawn_file_actions_addclose(&actions, fileno(output));
    pid_t child;
    int error = posix_spawnp(&child, executable.c_str(), &actions, nullptr, argv.data(), ::environ);
    posix_spawn_file_actions_destroy(&actions);
    if (error) { std::fclose(output); return {-1, "could not start subprocess"}; }
    int status = 0;
    pid_t waited;
    do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
    if (waited < 0) { std::fclose(output); return {-1, "could not wait for subprocess"}; }
    std::rewind(output);
    std::string data;
    char buffer[4096];
    while (size_t count = std::fread(buffer, 1, sizeof(buffer), output)) {
        if (count > limit - data.size()) {
            std::fclose(output);
            throw std::runtime_error("subprocess output exceeds capture limit");
        }
        data.append(buffer, count);
    }
    std::fclose(output);
    return {WIFEXITED(status) ? WEXITSTATUS(status) : -1, std::move(data)};
#endif
}
}
