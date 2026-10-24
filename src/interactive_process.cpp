#include "nextviper/transport.hpp"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#if !defined(_WIN32)
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace nextviper {
namespace {
Value ok(Value data = Value::make_nil()) { return Value::make_object({{"ok", Value::make_bool(true)}, {"data", std::move(data)}, {"error", Value::make_nil()}}); }
Value fail(const std::string& kind, const std::string& message) {
    return Value::make_object({{"ok", Value::make_bool(false)}, {"data", Value::make_nil()}, {"error", Value::make_object({{"kind", Value::make_string(kind)}, {"message", Value::make_string(message)}, {"status", Value::make_int(0)}})}});
}
Value get(const Value& cfg, const std::string& key, Value fallback = Value::make_nil()) {
    if (!cfg.is_object()) return fallback;
    auto it = cfg.as_object()->find(key);
    return it == cfg.as_object()->end() ? fallback : it->second;
}
bool clean(const std::string& value) { return value.find('\0') == std::string::npos; }
#if !defined(_WIN32)
using Clock = std::chrono::steady_clock;
int remaining(Clock::time_point deadline) {
    return static_cast<int>(std::max<int64_t>(0, std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count()));
}
void close_fd(int& fd) { if (fd >= 0) { ::close(fd); fd = -1; } }
bool make_pipe(int (&fds)[2]) {
    if (::pipe(fds) != 0) return false;
    for (int i = 0; i < 2; ++i) {
        int safe = fcntl(fds[i], F_DUPFD_CLOEXEC, 3);
        if (safe < 0) { close_fd(fds[0]); close_fd(fds[1]); return false; }
        ::close(fds[i]); fds[i] = safe;
    }
    return true;
}
// Block SIGPIPE only on this thread while writing, preserving preexisting signals.
struct PipeSignalGuard {
    sigset_t mask{}, old{}, pending{};
    bool installed = false, was_pending = false;
    PipeSignalGuard() {
        sigemptyset(&mask); sigaddset(&mask, SIGPIPE);
        installed = pthread_sigmask(SIG_BLOCK, &mask, &old) == 0;
        if (installed) { sigpending(&pending); was_pending = sigismember(&pending, SIGPIPE) == 1; }
    }
    ~PipeSignalGuard() {
        if (installed) {
            if (!was_pending) { timespec zero{}; while (sigtimedwait(&mask, nullptr, &zero) < 0 && errno == EINTR) {} }
            pthread_sigmask(SIG_SETMASK, &old, nullptr);
        }
    }
};
struct Child {
    pid_t pid = -1;
    int input = -1, output = -1, error = -1;
    bool closed = false, stderr_truncated = false, overflow = false;
    int exit_code = -1;
    std::string buffered, diagnostics;
    size_t stderr_bytes = 0, line_limit = 1048576;
    ~Child() { shutdown(); }
    void shutdown() noexcept {
        if (closed) return;
        closed = true;
        close_fd(input); close_fd(output); close_fd(error);
        if (pid > 0) {
            // Each child has its own process group. Its PID is not reaped until here,
            // so this group identifier cannot be recycled while this handle is live.
            kill(-pid, SIGTERM);
            int status = 0;
            for (int i = 0; i < 40; ++i) {
                siginfo_t observed{};
                if (waitid(P_PID, static_cast<id_t>(pid), &observed, WEXITED | WNOHANG | WNOWAIT) == 0 && observed.si_pid == pid) break;
                timespec pause{0, 5000000}; nanosleep(&pause, nullptr);
            }
            // Kill the group while its leader still owns its PID, then reap it.
            kill(-pid, SIGKILL);
            pid_t waited;
            do { waited = waitpid(pid, &status, 0); } while (waited < 0 && errno == EINTR);
            if (waited == pid) exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -WTERMSIG(status);
            pid = -1;
        }
    }
    bool drain(int& fd, bool stderr_stream) {
        if (fd < 0) return true;
        char bytes[4096];
        for (int batch = 0; batch < 16; ++batch) {
            auto n = ::read(fd, bytes, sizeof(bytes));
            if (n == 0) { close_fd(fd); return true; }
            if (n < 0) {
                if (errno == EINTR) continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) return true;
                close_fd(fd); return false;
            }
            if (stderr_stream) {
                stderr_bytes += static_cast<size_t>(n);
                size_t keep = std::min(static_cast<size_t>(n), size_t(65536) - diagnostics.size());
                diagnostics.append(bytes, keep);
                if (keep != static_cast<size_t>(n)) stderr_truncated = true;
                if (stderr_bytes > 16 * 1024 * 1024) { overflow = true; return false; }
            } else {
                if (static_cast<size_t>(n) > line_limit - buffered.size()) { overflow = true; return false; }
                buffered.append(bytes, static_cast<size_t>(n));
            }
        }
        return true;
    }
    Value read_line(int timeout) {
        if (closed) return fail("closed", "Subprocess is closed");
        auto deadline = Clock::now() + std::chrono::milliseconds(timeout);
        while (true) {
            auto newline = buffered.find('\n');
            if (newline != std::string::npos) {
                auto line = buffered.substr(0, newline);
                buffered.erase(0, newline + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                return ok(Value::make_string(std::move(line)));
            }
            if (output < 0) {
                if (buffered.empty()) return ok();
                auto tail = std::move(buffered); buffered.clear();
                return ok(Value::make_string(std::move(tail)));
            }
            int wait = remaining(deadline);
            if (wait <= 0) return fail("timeout", "Subprocess read timed out");
            pollfd fds[]{{output, POLLIN, 0}, {error, POLLIN, 0}};
            int count = ::poll(fds, 2, wait);
            if (count < 0 && errno == EINTR) continue;
            if (count < 0) return fail("process", "Subprocess polling failed");
            if (count == 0) return fail("timeout", "Subprocess read timed out");
            if ((fds[0].revents && !drain(output, false)) || (fds[1].revents && !drain(error, true))) {
                shutdown(); return fail(overflow ? "limit" : "process", "Subprocess output limit or pipe failure");
            }
        }
    }
    Value write(const std::string& text, int timeout) {
        if (closed || input < 0) return fail("closed", "Subprocess input is closed");
        if (text.size() > 1048576) return fail("limit", "Subprocess write exceeds 1 MiB");
        PipeSignalGuard signal;
        if (!signal.installed) return fail("process", "Could not protect pipe write");
        auto deadline = Clock::now() + std::chrono::milliseconds(timeout);
        size_t sent = 0;
        while (sent < text.size()) {
            int wait = remaining(deadline);
            if (wait <= 0) return fail("timeout", "Subprocess write timed out");
            pollfd fds[]{{input, POLLOUT, 0}, {output, POLLIN, 0}, {error, POLLIN, 0}};
            int ready = ::poll(fds, 3, wait);
            if (ready < 0 && errno == EINTR) continue;
            if (ready < 0) return fail("process", "Subprocess polling failed");
            if (ready == 0) return fail("timeout", "Subprocess write timed out");
            if ((fds[1].revents && !drain(output, false)) || (fds[2].revents && !drain(error, true))) {
                shutdown(); return fail(overflow ? "limit" : "process", "Subprocess output limit or pipe failure");
            }
            if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) { close_fd(input); return fail("closed", "Subprocess closed its input"); }
            if (fds[0].revents & POLLOUT) {
                auto n = ::write(input, text.data() + sent, text.size() - sent);
                if (n > 0) sent += static_cast<size_t>(n);
                else if (n < 0 && errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
                    close_fd(input); return fail("closed", "Subprocess closed its input");
                }
            }
        }
        return ok(Value::make_int(sent));
    }
};
int timeout_arg(const std::vector<Value>& args, size_t index, int fallback) {
    if (args.size() <= index) return fallback;
    if (!args[index].is_int() || args[index].as_int() < 1 || args[index].as_int() > 300000) return -1;
    return static_cast<int>(args[index].as_int());
}
#endif
}

Value spawn_interactive(const Value& cfg, SourceSpan) {
#if defined(_WIN32)
    (void)cfg;
    return fail("unsupported_platform", "Interactive process transport currently requires POSIX");
#else
    if (!cfg.is_object()) return fail("config", "process.spawn requires a config object");
    if (!get(cfg, "cwd").is_nil()) return fail("config", "cwd is not supported; use explicit executable and argument paths");
    auto command = get(cfg, "command"), arguments = get(cfg, "args", Value::make_array({}));
    if (!command.is_string() || command.as_string().empty() || !clean(command.as_string()) || !arguments.is_array() || arguments.as_array()->size() > 256)
        return fail("config", "Set a command and an argument array without NUL bytes");
    auto env = get(cfg, "env", Value::make_object({})), inherit = get(cfg, "inherit_env", Value::make_array({}));
    if (!env.is_object() || !inherit.is_array()) return fail("config", "env must be an object and inherit_env an array");
    std::map<std::string, std::string> environment{{"PATH", "/usr/bin:/bin"}, {"LANG", "C.UTF-8"}};
    auto valid_name = [](const std::string& name) { return !name.empty() && name.find_first_not_of("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz") == std::string::npos && !std::isdigit(static_cast<unsigned char>(name[0])); };
    for (const auto& name : *inherit.as_array()) {
        if (!name.is_string() || !valid_name(name.as_string())) return fail("config", "Invalid inherited environment variable name");
        const char* value = std::getenv(name.as_string().c_str());
        if (value) environment[name.as_string()] = value;
    }
    for (const auto& [name, value] : *env.as_object()) {
        if (!valid_name(name) || !value.is_string() || !clean(value.as_string())) return fail("config", "Invalid explicit subprocess environment");
        environment[name] = value.as_string();
    }
    if (environment.size() > 128) return fail("limit", "Too many subprocess environment variables");
    std::vector<std::string> values{command.as_string()}, environment_values;
    size_t bytes = command.as_string().size();
    for (const auto& value : *arguments.as_array()) {
        if (!value.is_string() || !clean(value.as_string())) return fail("config", "Subprocess arguments must be strings without NUL bytes");
        bytes += value.as_string().size(); values.push_back(value.as_string());
    }
    for (const auto& [name, value] : environment) { bytes += name.size() + value.size(); environment_values.push_back(name + "=" + value); }
    if (bytes > 131072) return fail("limit", "Subprocess arguments and environment exceed 128 KiB");
    auto executable = command.as_string();
    if (executable.find('/') == std::string::npos) {
        bool found = false;
        const std::string& path = environment["PATH"];
        size_t start = 0;
        while (start <= path.size()) {
            auto end = path.find(':', start);
            auto directory = path.substr(start, end == std::string::npos ? path.size() - start : end - start);
            if (!directory.empty() && directory[0] == '/') {
                auto candidate = directory + "/" + executable;
                if (access(candidate.c_str(), X_OK) == 0) { executable = std::move(candidate); found = true; break; }
            }
            if (end == std::string::npos) break;
            start = end + 1;
        }
        if (!found) return fail("process", "Command was not found in the configured PATH");
    }
    auto limit = get(cfg, "max_line_bytes", Value::make_int(1048576));
    if (!limit.is_int() || limit.as_int() < 1024 || limit.as_int() > 1048576) return fail("config", "max_line_bytes must be between 1024 and 1048576");
    int pipes[3][2]{{-1,-1},{-1,-1},{-1,-1}};
    auto cleanup = [&] { for (auto& pair : pipes) { close_fd(pair[0]); close_fd(pair[1]); } };
    for (auto& pair : pipes) if (!make_pipe(pair)) { cleanup(); return fail("process", "Could not create subprocess pipes"); }
    std::vector<char*> argv, envp;
    for (auto& value : values) argv.push_back(value.data());
    for (auto& value : environment_values) envp.push_back(value.data());
    argv.push_back(nullptr); envp.push_back(nullptr);
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attributes;
    if (posix_spawn_file_actions_init(&actions) != 0) { cleanup(); return fail("process", "Could not initialize subprocess actions"); }
    if (posix_spawnattr_init(&attributes) != 0) { posix_spawn_file_actions_destroy(&actions); cleanup(); return fail("process", "Could not initialize subprocess attributes"); }
    int error = posix_spawn_file_actions_adddup2(&actions, pipes[0][0], STDIN_FILENO);
    error |= posix_spawn_file_actions_adddup2(&actions, pipes[1][1], STDOUT_FILENO);
    error |= posix_spawn_file_actions_adddup2(&actions, pipes[2][1], STDERR_FILENO);
    for (auto& pair : pipes) for (int fd : pair) error |= posix_spawn_file_actions_addclose(&actions, fd);
#if defined(__GLIBC__)
    error |= posix_spawn_file_actions_addclosefrom_np(&actions, 3);
#endif
    error |= posix_spawnattr_setpgroup(&attributes, 0);
    sigset_t empty{}, defaults{}; sigemptyset(&empty); sigemptyset(&defaults); sigaddset(&defaults, SIGPIPE);
    error |= posix_spawnattr_setsigmask(&attributes, &empty);
    error |= posix_spawnattr_setsigdefault(&attributes, &defaults);
    error |= posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);
    auto child = std::make_shared<Child>();
    if (!error) error = posix_spawn(&child->pid, executable.c_str(), &actions, &attributes, argv.data(), envp.data());
    posix_spawn_file_actions_destroy(&actions); posix_spawnattr_destroy(&attributes);
    if (error) { child->pid = -1; cleanup(); return fail("process", "Could not start subprocess"); }
    close_fd(pipes[0][0]); close_fd(pipes[1][1]); close_fd(pipes[2][1]);
    child->input = pipes[0][1]; child->output = pipes[1][0]; child->error = pipes[2][0]; child->line_limit = limit.as_int();
    for (int fd : {child->input, child->output, child->error}) {
        int flags = fcntl(fd, F_GETFL);
        if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) { child->shutdown(); return fail("process", "Could not configure subprocess pipes"); }
    }
    std::map<std::string, Value> handle;
    handle["pid"] = Value::make_int(child->pid);
    handle["write"] = Value::make_native_fn("write", -1, [child](const std::vector<Value>& args, SourceSpan) {
        int timeout = timeout_arg(args, 1, 30000);
        if (args.empty() || args.size() > 2 || !args[0].is_string() || timeout < 0) return fail("config", "write requires text and an optional timeout of 1-300000 ms");
        return child->write(args[0].as_string(), timeout);
    });
    handle["read_line"] = Value::make_native_fn("read_line", 1, [child](const std::vector<Value>& args, SourceSpan) {
        int timeout = timeout_arg(args, 0, 30000);
        if (timeout < 0) return fail("config", "read_line requires a timeout of 1-300000 ms");
        return child->read_line(timeout);
    });
    handle["stderr"] = Value::make_native_fn("stderr", 0, [child](const std::vector<Value>&, SourceSpan) {
        if (!child->closed && !child->drain(child->error, true)) { child->shutdown(); return fail("limit", "Subprocess diagnostic limit or pipe failure"); }
        return ok(Value::make_object({{"text", Value::make_string(child->diagnostics)}, {"truncated", Value::make_bool(child->stderr_truncated)}}));
    });
    handle["close"] = Value::make_native_fn("close", 0, [child](const std::vector<Value>&, SourceSpan) {
        child->shutdown(); return ok(Value::make_object({{"exit_code", Value::make_int(child->exit_code)}}));
    });
    return ok(Value::make_object(std::move(handle)));
#endif
}
}
