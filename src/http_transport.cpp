#include "nextviper/transport.hpp"
#include <curl/curl.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

namespace nextviper {
namespace {
Value ok(Value data) { return Value::make_object({{"ok", Value::make_bool(true)}, {"data", std::move(data)}, {"error", Value::make_nil()}}); }
Value fail(const std::string& kind, const std::string& message) {
    return Value::make_object({{"ok", Value::make_bool(false)}, {"data", Value::make_nil()}, {"error", Value::make_object({{"kind", Value::make_string(kind)}, {"message", Value::make_string(message)}, {"status", Value::make_int(0)}})}});
}
Value get(const Value& object, const std::string& key, Value fallback = Value::make_nil()) {
    if (!object.is_object()) return fallback;
    auto it = object.as_object()->find(key);
    return it == object.as_object()->end() ? fallback : it->second;
}
long integer(const Value& cfg, const std::string& key, long fallback, long lo, long hi) {
    auto v = get(cfg, key, Value::make_int(fallback));
    if (!v.is_int() || v.as_int() < lo || v.as_int() > hi) throw std::invalid_argument("Invalid " + key);
    return static_cast<long>(v.as_int());
}
bool controls(const std::string& s) { return std::any_of(s.begin(), s.end(), [](unsigned char c) { return c < 32 || c == 127; }); }
std::string trim(std::string s) {
    auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}
struct Transfer {
    CURL* curl = nullptr;
    bool streaming = false, cancelled = false, limit = false, malformed = false;
    size_t received = 0, header_bytes = 0, maximum = 16 * 1024 * 1024;
    long status = 0;
    std::string body;
    std::map<std::string, Value> headers;
    Value on_chunk, on_headers;
    TransportInvoke invoke;
    SourceSpan span;
    std::exception_ptr exception;
    Value metadata() const {
        return Value::make_object({{"status", Value::make_int(status)}, {"headers", Value::make_object(headers)}});
    }
};
size_t headers_callback(char* raw, size_t size, size_t count, void* user) noexcept {
    auto& t = *static_cast<Transfer*>(user);
    const size_t n = size * count;
    try {
        t.header_bytes += n;
        if (t.header_bytes > 65536) { t.limit = true; return 0; }
        std::string line(raw, n);
        if (line.starts_with("HTTP/")) {
            auto space = line.find(' ');
            if (space == std::string::npos || space + 3 >= line.size()) { t.malformed = true; return 0; }
            t.status = std::stol(line.substr(space + 1, 3));
            t.headers.clear();
        } else if (line == "\r\n" || line == "\n") {
            if (t.status >= 200 && t.on_headers.is_callable()) {
                auto accepted = t.invoke(t.on_headers, {t.metadata()}, t.span);
                if (accepted.is_bool() && !accepted.as_bool()) { t.cancelled = true; return 0; }
            }
        } else {
            auto colon = line.find(':');
            if (colon == std::string::npos) { t.malformed = true; return 0; }
            auto key = line.substr(0, colon), value = trim(line.substr(colon + 1));
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::tolower(c); });
            if (controls(value) || t.headers.size() >= 128) { t.malformed = true; return 0; }
            // Preserve the last value; duplicate Set-Cookie handling is not a cookie jar.
            t.headers[key] = Value::make_string(value);
        }
        return n;
    } catch (...) { t.exception = std::current_exception(); return 0; }
}
size_t body_callback(char* raw, size_t size, size_t count, void* user) noexcept {
    auto& t = *static_cast<Transfer*>(user);
    const size_t n = size * count;
    try {
        if (n > t.maximum - t.received) { t.limit = true; return 0; }
        t.received += n;
        if (!t.streaming) t.body.append(raw, n);
        else if (t.status >= 200 && t.status < 300) {
            auto accepted = t.invoke(t.on_chunk, {Value::make_string(std::string(raw, n))}, t.span);
            if (accepted.is_bool() && !accepted.as_bool()) { t.cancelled = true; return 0; }
        }
        return n;
    } catch (...) { t.exception = std::current_exception(); return 0; }
}
struct CurlList { curl_slist* head = nullptr; ~CurlList() { curl_slist_free_all(head); } };
}

Value http_exchange(const Value& cfg, const Value& on_chunk, const TransportInvoke& invoke, SourceSpan span) {
    if (!cfg.is_object()) return fail("config", "HTTP config must be an object");
    auto url = get(cfg, "url"), method = get(cfg, "method", Value::make_string("GET"));
    auto body = get(cfg, "body", Value::make_string("")), headers = get(cfg, "headers", Value::make_object({}));
    auto on_headers = get(cfg, "on_headers");
    if (!url.is_string() || controls(url.as_string()) || !method.is_string() || !body.is_string() || !headers.is_object())
        return fail("config", "HTTP URL, method, body and headers have invalid types or control characters");
    if (!(url.as_string().starts_with("https://") || url.as_string().starts_with("http://")) ||
        method.as_string().empty() || method.as_string().find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ") != std::string::npos)
        return fail("config", "HTTP requires an HTTP(S) URL and uppercase method");
    if ((!on_chunk.is_nil() && !on_chunk.is_callable()) || (!on_headers.is_nil() && !on_headers.is_callable()))
        return fail("config", "HTTP callbacks must be functions");
    if (body.as_string().size() > 8 * 1024 * 1024) return fail("limit", "HTTP request body exceeds 8 MiB");
    long timeout, maximum;
    try { timeout = integer(cfg, "timeout_ms", 30000, 1, 300000); maximum = integer(cfg, "max_response_bytes", 16777216, 1, 16777216); }
    catch (const std::invalid_argument& e) { return fail("config", e.what()); }
    static std::once_flag initialized;
    static CURLcode init_status;
    std::call_once(initialized, [] { init_status = curl_global_init(CURL_GLOBAL_DEFAULT); });
    if (init_status != CURLE_OK) return fail("network", "Could not initialize HTTP transport");
    std::unique_ptr<CURLU, decltype(&curl_url_cleanup)> parsed(curl_url(), curl_url_cleanup);
    if (!parsed || curl_url_set(parsed.get(), CURLUPART_URL, url.as_string().c_str(), 0) != CURLUE_OK)
        return fail("config", "Invalid HTTP URL");
    char* username = nullptr;
    auto user_status = curl_url_get(parsed.get(), CURLUPART_USER, &username, 0);
    curl_free(username);
    if (user_status == CURLUE_OK) return fail("config", "Put credentials in explicit headers, not the HTTP URL");
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> handle(curl_easy_init(), curl_easy_cleanup);
    if (!handle) return fail("network", "Could not create HTTP transport");
    Transfer transfer;
    transfer.curl = handle.get(); transfer.streaming = !on_chunk.is_nil(); transfer.on_chunk = on_chunk;
    transfer.on_headers = on_headers; transfer.invoke = invoke; transfer.span = span; transfer.maximum = maximum;
    CurlList list;
    for (const auto& [key, value] : *headers.as_object()) {
        if (key.empty() || key.find_first_not_of("!#$%&'*+-.^_`|~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz") != std::string::npos ||
            !value.is_string() || controls(value.as_string())) return fail("config", "Invalid HTTP header");
        std::string header = key + ": " + value.as_string();
        auto next = curl_slist_append(list.head, header.c_str());
        if (!next) return fail("network", "Could not allocate HTTP headers");
        list.head = next;
    }
    auto* h = handle.get();
    curl_easy_setopt(h, CURLOPT_URL, url.as_string().c_str());
    curl_easy_setopt(h, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(h, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(h, CURLOPT_SUPPRESS_CONNECT_HEADERS, 1L);
    curl_easy_setopt(h, CURLOPT_NETRC, CURL_NETRC_IGNORED);
    curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(h, CURLOPT_TIMEOUT_MS, timeout);
    curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT_MS, std::min(timeout, 10000L));
    curl_easy_setopt(h, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(h, CURLOPT_ACCEPT_ENCODING, "");
    // These CA selectors are honored without dumping environment values.
    const char* ca = std::getenv("CURL_CA_BUNDLE");
    if (!ca || !*ca) ca = std::getenv("SSL_CERT_FILE");
    if (ca && *ca) curl_easy_setopt(h, CURLOPT_CAINFO, ca);
    curl_easy_setopt(h, CURLOPT_HTTPHEADER, list.head);
    if (method.as_string() == "HEAD") curl_easy_setopt(h, CURLOPT_NOBODY, 1L);
    else if (!body.as_string().empty() || method.as_string() == "POST" || method.as_string() == "PUT" || method.as_string() == "PATCH") {
        curl_easy_setopt(h, CURLOPT_POSTFIELDS, body.as_string().data());
        curl_easy_setopt(h, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.as_string().size()));
    }
    curl_easy_setopt(h, CURLOPT_CUSTOMREQUEST, method.as_string().c_str());
    curl_easy_setopt(h, CURLOPT_HEADERFUNCTION, headers_callback);
    curl_easy_setopt(h, CURLOPT_HEADERDATA, &transfer);
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, body_callback);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, &transfer);
    const auto result = curl_easy_perform(h);
    if (transfer.exception) std::rethrow_exception(transfer.exception);
    if (transfer.limit) return fail("limit", "HTTP response exceeded its configured size limit");
    if (transfer.malformed) return fail("protocol", "Malformed HTTP response headers");
    if (result != CURLE_OK && !transfer.cancelled)
        return fail(result == CURLE_OPERATION_TIMEDOUT ? "timeout" : "network", "HTTP transfer failed");
    auto data = *transfer.metadata().as_object();
    data["body"] = Value::make_string(std::move(transfer.body));
    data["cancelled"] = Value::make_bool(transfer.cancelled);
    return ok(Value::make_object(std::move(data)));
}
}
