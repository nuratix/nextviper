#include "nextviper/lsp.hpp"
#include "nextviper/lexer.hpp"
#include "nextviper/parser.hpp"
#include "nextviper/type_checker.hpp"
#include "nextviper/formatter.hpp"
#include "nextviper/package_manager.hpp"
#include <sstream>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

namespace nextviper {

// ============================================================================
// JSON Serialization Helpers
// ============================================================================

static std::string escape_json(const std::string& str) {
    std::string out;
    for (char c : str) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\b') out += "\\b";
        else if (c == '\f') out += "\\f";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if ((unsigned char)c < 0x20) {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\u%04x", c);
            out += buf;
        } else {
            out += c;
        }
    }
    return out;
}

static std::string extract_json_string(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) {
        search = "\"" + key + "\": \"";
        pos = json.find(search);
        if (pos == std::string::npos) return "";
    }
    pos += search.length();
    size_t end = json.find("\"", pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

static int extract_json_int(const std::string& json, const std::string& key, int default_val = 0) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) {
        search = "\"" + key + "\": ";
        pos = json.find(search);
        if (pos == std::string::npos) return default_val;
    }
    pos += search.length();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    size_t end = pos;
    while (end < json.size() && (std::isdigit(json[end]) || json[end] == '-')) end++;
    if (pos == end) return default_val;
    try {
        return std::stoi(json.substr(pos, end - pos));
    } catch (...) {
        return default_val;
    }
}

// ============================================================================
// LSP Struct JSON Implementations
// ============================================================================

std::string LSPDiagnostic::to_json() const {
    std::string json = "{";
    json += "\"range\":" + range.to_json() + ",";
    json += "\"severity\":" + std::to_string(severity) + ",";
    if (!code.empty()) json += "\"code\":\"" + escape_json(code) + "\",";
    json += "\"source\":\"" + escape_json(source) + "\",";
    json += "\"message\":\"" + escape_json(message) + "\"";
    json += "}";
    return json;
}

std::string LSPCompletionItem::to_json() const {
    std::string json = "{";
    json += "\"label\":\"" + escape_json(label) + "\",";
    json += "\"kind\":" + std::to_string(kind) + ",";
    if (!detail.empty()) json += "\"detail\":\"" + escape_json(detail) + "\",";
    if (!documentation.empty()) {
        json += "\"documentation\":{\"kind\":\"markdown\",\"value\":\"" + escape_json(documentation) + "\"},";
    }
    if (!insert_text.empty()) {
        json += "\"insertText\":\"" + escape_json(insert_text) + "\",";
    }
    if (json.back() == ',') json.pop_back();
    json += "}";
    return json;
}

std::string LSPHover::to_json() const {
    std::string json = "{\"contents\":{\"kind\":\"markdown\",\"value\":\"" + escape_json(contents) + "\"}";
    if (has_range) {
        json += ",\"range\":" + range.to_json();
    }
    json += "}";
    return json;
}

std::string LSPDocumentSymbol::to_json() const {
    std::string json = "{";
    json += "\"name\":\"" + escape_json(name) + "\",";
    if (!detail.empty()) json += "\"detail\":\"" + escape_json(detail) + "\",";
    json += "\"kind\":" + std::to_string(kind) + ",";
    json += "\"range\":" + range.to_json() + ",";
    json += "\"selectionRange\":" + selection_range.to_json();
    if (!children.empty()) {
        json += ",\"children\":[";
        for (size_t i = 0; i < children.size(); ++i) {
            if (i > 0) json += ",";
            json += children[i].to_json();
        }
        json += "]";
    }
    json += "}";
    return json;
}

// ============================================================================
// URI and Path conversions
// ============================================================================

std::string LanguageServer::uri_to_path(const std::string& uri) {
    if (uri.starts_with("file://")) {
        std::string p = uri.substr(7);
#if defined(_WIN32)
        if (p.size() > 2 && p[0] == '/' && std::isalpha(p[1]) && p[2] == ':') {
            p = p.substr(1);
        }
#endif
        return p;
    }
    return uri;
}

std::string LanguageServer::path_to_uri(const std::string& path) {
    if (path.starts_with("file://")) return path;
    try {
        std::string abs_path = fs::absolute(path).string();
#if defined(_WIN32)
        std::replace(abs_path.begin(), abs_path.end(), '\\', '/');
        return "file:///" + abs_path;
#else
        return "file://" + abs_path;
#endif
    } catch (...) {
        return "file://" + path;
    }
}

// ============================================================================
// LanguageServer Implementation
// ============================================================================

LanguageServer::LanguageServer(std::istream& in, std::ostream& out)
    : input_stream_(in), output_stream_(out) {}

void LanguageServer::set_workspace_root(const std::string& root_path) {
    workspace_root_ = root_path;
}

DocumentState* LanguageServer::get_document(const std::string& uri) {
    auto it = documents_.find(uri);
    if (it != documents_.end()) return &it->second;
    return nullptr;
}

void LanguageServer::send_response(const std::string& json_payload) {
    output_stream_ << "Content-Length: " << json_payload.size() << "\r\n\r\n" << json_payload;
    output_stream_.flush();
}

void LanguageServer::send_notification(const std::string& method, const std::string& params_json) {
    std::string payload = "{\"jsonrpc\":\"2.0\",\"method\":\"" + method + "\",\"params\":" + params_json + "}";
    send_response(payload);
}

void LanguageServer::run() {
    std::string header;
    while (std::getline(input_stream_, header)) {
        if (!header.empty() && header.back() == '\r') header.pop_back();

        if (header.starts_with("Content-Length: ")) {
            size_t content_length = 0;
            try {
                content_length = std::stoull(header.substr(16));
            } catch (...) {
                continue;
            }

            std::string line;
            while (std::getline(input_stream_, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.empty()) break;
            }

            std::vector<char> buffer(content_length);
            input_stream_.read(buffer.data(), content_length);
            std::string message(buffer.data(), content_length);

            process_message(message);

            if (is_shutdown_) {
                break;
            }
        }
    }
}

void LanguageServer::process_message(const std::string& raw_json) {
    std::string method = extract_json_string(raw_json, "method");
    int id = extract_json_int(raw_json, "id", -1);

    if (method == "initialize") {
        std::string response = handle_initialize(id, raw_json);
        send_response(response);
        is_initialized_ = true;
    } else if (method == "initialized") {
        // Client confirmed initialization
    } else if (method == "shutdown") {
        std::string response = handle_shutdown(id);
        send_response(response);
    } else if (method == "exit") {
        is_shutdown_ = true;
    } else if (method == "textDocument/didOpen") {
        handle_text_document_did_open(raw_json);
    } else if (method == "textDocument/didChange") {
        handle_text_document_did_change(raw_json);
    } else if (method == "textDocument/didClose") {
        handle_text_document_did_close(raw_json);
    } else if (method == "textDocument/completion") {
        std::string uri = extract_json_string(raw_json, "uri");
        size_t pos_idx = raw_json.find("\"position\"");
        int line = 0, character = 0;
        if (pos_idx != std::string::npos) {
            std::string sub = raw_json.substr(pos_idx);
            line = extract_json_int(sub, "line", 0);
            character = extract_json_int(sub, "character", 0);
        }
        std::string response = handle_completion(id, uri, line, character);
        send_response(response);
    } else if (method == "textDocument/hover") {
        std::string uri = extract_json_string(raw_json, "uri");
        size_t pos_idx = raw_json.find("\"position\"");
        int line = 0, character = 0;
        if (pos_idx != std::string::npos) {
            std::string sub = raw_json.substr(pos_idx);
            line = extract_json_int(sub, "line", 0);
            character = extract_json_int(sub, "character", 0);
        }
        std::string response = handle_hover(id, uri, line, character);
        send_response(response);
    } else if (method == "textDocument/definition") {
        std::string uri = extract_json_string(raw_json, "uri");
        size_t pos_idx = raw_json.find("\"position\"");
        int line = 0, character = 0;
        if (pos_idx != std::string::npos) {
            std::string sub = raw_json.substr(pos_idx);
            line = extract_json_int(sub, "line", 0);
            character = extract_json_int(sub, "character", 0);
        }
        std::string response = handle_definition(id, uri, line, character);
        send_response(response);
    } else if (method == "textDocument/references") {
        std::string uri = extract_json_string(raw_json, "uri");
        size_t pos_idx = raw_json.find("\"position\"");
        int line = 0, character = 0;
        if (pos_idx != std::string::npos) {
            std::string sub = raw_json.substr(pos_idx);
            line = extract_json_int(sub, "line", 0);
            character = extract_json_int(sub, "character", 0);
        }
        std::string response = handle_references(id, uri, line, character);
        send_response(response);
    } else if (method == "textDocument/documentSymbol") {
        std::string uri = extract_json_string(raw_json, "uri");
        std::string response = handle_document_symbol(id, uri);
        send_response(response);
    } else if (method == "workspace/symbol") {
        std::string query = extract_json_string(raw_json, "query");
        std::string response = handle_workspace_symbol(id, query);
        send_response(response);
    } else if (method == "textDocument/formatting") {
        std::string uri = extract_json_string(raw_json, "uri");
        std::string response = handle_formatting(id, uri);
        send_response(response);
    } else if (id >= 0) {
        std::string response = "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":null}";
        send_response(response);
    }
}

// ============================================================================
// Core LSP Handlers
// ============================================================================

std::string LanguageServer::handle_initialize(int id, const std::string& params_json) {
    std::string root_uri = extract_json_string(params_json, "rootUri");
    if (!root_uri.empty()) {
        workspace_root_ = uri_to_path(root_uri);
    } else {
        std::string root_path = extract_json_string(params_json, "rootPath");
        if (!root_path.empty()) {
            workspace_root_ = root_path;
        }
    }

    std::string result = "{"
        "\"capabilities\":{"
            "\"textDocumentSync\":1,"
            "\"completionProvider\":{\"triggerCharacters\":[\".\",\":\"]},"
            "\"hoverProvider\":true,"
            "\"definitionProvider\":true,"
            "\"referencesProvider\":true,"
            "\"documentSymbolProvider\":true,"
            "\"workspaceSymbolProvider\":true,"
            "\"documentFormattingProvider\":true"
        "},"
        "\"serverInfo\":{"
            "\"name\":\"nextviper-lsp\","
            "\"version\":\"1.0.0\""
        "}"
    "}";

    return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":" + result + "}";
}

std::string LanguageServer::handle_shutdown(int id) {
    is_shutdown_ = true;
    return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":null}";
}

void LanguageServer::handle_text_document_did_open(const std::string& params_json) {
    std::string uri = extract_json_string(params_json, "uri");
    if (uri.empty()) return;

    std::string text = "";
    size_t text_key = params_json.find("\"text\":\"");
    if (text_key != std::string::npos) {
        text_key += 8;
        std::string raw;
        for (size_t i = text_key; i < params_json.size(); ++i) {
            if (params_json[i] == '"' && (i == 0 || params_json[i-1] != '\\')) {
                break;
            }
            if (params_json[i] == '\\' && i + 1 < params_json.size()) {
                char next = params_json[i+1];
                if (next == 'n') { raw += '\n'; i++; }
                else if (next == 'r') { raw += '\r'; i++; }
                else if (next == 't') { raw += '\t'; i++; }
                else if (next == '"') { raw += '"'; i++; }
                else if (next == '\\') { raw += '\\'; i++; }
                else { raw += next; i++; }
            } else {
                raw += params_json[i];
            }
        }
        text = raw;
    }

    DocumentState doc;
    doc.uri = uri;
    doc.path = uri_to_path(uri);
    doc.content = text;
    doc.version = extract_json_int(params_json, "version", 1);

    analyze_document(doc);
    documents_[uri] = std::move(doc);
    publish_diagnostics(uri, documents_[uri].diagnostics);
}

void LanguageServer::handle_text_document_did_change(const std::string& params_json) {
    std::string uri = extract_json_string(params_json, "uri");
    if (uri.empty()) return;

    size_t text_key = params_json.find("\"text\":\"");
    if (text_key == std::string::npos) return;
    text_key += 8;

    std::string text;
    for (size_t i = text_key; i < params_json.size(); ++i) {
        if (params_json[i] == '"' && (i == 0 || params_json[i-1] != '\\')) {
            break;
        }
        if (params_json[i] == '\\' && i + 1 < params_json.size()) {
            char next = params_json[i+1];
            if (next == 'n') { text += '\n'; i++; }
            else if (next == 'r') { text += '\r'; i++; }
            else if (next == 't') { text += '\t'; i++; }
            else if (next == '"') { text += '"'; i++; }
            else if (next == '\\') { text += '\\'; i++; }
            else { text += next; i++; }
        } else {
            text += params_json[i];
        }
    }

    auto it = documents_.find(uri);
    if (it == documents_.end()) {
        DocumentState doc;
        doc.uri = uri;
        doc.path = uri_to_path(uri);
        doc.content = text;
        doc.version = extract_json_int(params_json, "version", 1);
        analyze_document(doc);
        documents_[uri] = std::move(doc);
        publish_diagnostics(uri, documents_[uri].diagnostics);
    } else {
        it->second.content = text;
        it->second.version = extract_json_int(params_json, "version", it->second.version + 1);
        analyze_document(it->second);
        publish_diagnostics(uri, it->second.diagnostics);
    }
}

void LanguageServer::handle_text_document_did_close(const std::string& params_json) {
    std::string uri = extract_json_string(params_json, "uri");
    if (!uri.empty()) {
        publish_diagnostics(uri, {});
        documents_.erase(uri);
    }
}

// ============================================================================
// Document Analysis & Diagnostics
// ============================================================================

void LanguageServer::analyze_document(DocumentState& doc) {
    doc.diagnostics.clear();
    doc.tokens.clear();
    doc.ast = nullptr;

    SourceManager source_manager;
    source_manager.add_file(doc.path, doc.content);
    DiagnosticEngine diag_engine(source_manager, false);

    // 1. Lexical Analysis
    Lexer lexer(doc.content, doc.path, diag_engine);
    doc.tokens = lexer.tokenize();

    // 2. Syntax Analysis (Parser)
    if (!diag_engine.has_errors()) {
        Parser parser(doc.tokens, diag_engine);
        auto p = parser.parse_program();
        if (p) {
            doc.ast = std::shared_ptr<Program>(std::move(p));
        }
    }

    // 3. Static Semantic / Type Checking
    if (doc.ast && !diag_engine.has_errors()) {
        TypeChecker checker(diag_engine);
        checker.check(*doc.ast);
    }

    // Map compiler diagnostics to LSP format (0-indexed lines and characters)
    for (const auto& d : diag_engine.diagnostics()) {
        LSPDiagnostic lsp_d;
        lsp_d.message = d.message;
        lsp_d.code = d.code;
        
        switch (d.level) {
            case DiagnosticLevel::ERROR:
                lsp_d.severity = 1;
                break;
            case DiagnosticLevel::WARNING:
                lsp_d.severity = 2;
                break;
            case DiagnosticLevel::NOTE:
            case DiagnosticLevel::HELP:
                lsp_d.severity = 3;
                break;
        }

        int line = std::max(0, (int)d.span.start.line - 1);
        int start_col = std::max(0, (int)d.span.start.column - 1);
        int end_line = std::max(0, (int)d.span.end.line - 1);
        int end_col = std::max(start_col + 1, (int)d.span.end.column);

        lsp_d.range.start = LSPPosition(line, start_col);
        lsp_d.range.end = LSPPosition(end_line, end_col);
        doc.diagnostics.push_back(lsp_d);
    }
}

void LanguageServer::publish_diagnostics(const std::string& uri, const std::vector<LSPDiagnostic>& diags) {
    std::string params = "{\"uri\":\"" + uri + "\",\"diagnostics\":[";
    for (size_t i = 0; i < diags.size(); ++i) {
        if (i > 0) params += ",";
        params += diags[i].to_json();
    }
    params += "]}";
    send_notification("textDocument/publishDiagnostics", params);
}

// ============================================================================
// Autocompletion
// ============================================================================

std::string LanguageServer::get_word_at(const std::string& text, int line, int col, int* out_start_col, int* out_end_col) {
    std::istringstream stream(text);
    std::string current_line;
    int current_line_num = 0;

    while (std::getline(stream, current_line)) {
        if (current_line_num == line) {
            if (col < 0) col = 0;
            if (col > (int)current_line.size()) col = (int)current_line.size();

            int start = col;
            while (start > 0 && (std::isalnum(current_line[start - 1]) || current_line[start - 1] == '_' || current_line[start - 1] == '.')) {
                start--;
            }
            int end = col;
            while (end < (int)current_line.size() && (std::isalnum(current_line[end]) || current_line[end] == '_')) {
                end++;
            }
            if (out_start_col) *out_start_col = start;
            if (out_end_col) *out_end_col = end;
            return current_line.substr(start, end - start);
        }
        current_line_num++;
    }
    return "";
}

std::string LanguageServer::handle_completion(int id, const std::string& uri, int line, int character) {
    auto it = documents_.find(uri);
    std::vector<LSPCompletionItem> items;

    std::string prefix = "";
    if (it != documents_.end()) {
        prefix = get_word_at(it->second.content, line, character);
    }

    // 1. Language Keywords
    static const std::vector<std::pair<std::string, std::string>> KEYWORDS = {
        {"let", "Declare variable: let name = value"},
        {"fn", "Define function: fn name(args):"},
        {"return", "Return value from function"},
        {"if", "Conditional branch: if condition:"},
        {"elif", "Else-if conditional branch"},
        {"else", "Else conditional branch"},
        {"while", "While loop: while condition:"},
        {"for", "For-in loop: for item in collection:"},
        {"in", "Membership operator in for-loops"},
        {"import", "Import module: import std.io"},
        {"export", "Export module symbol: export fn foo():"},
        {"break", "Break loop execution"},
        {"continue", "Continue to next loop iteration"},
        {"true", "Boolean true literal"},
        {"false", "Boolean false literal"},
        {"null", "Null reference literal"}
    };

    for (const auto& [kw, doc] : KEYWORDS) {
        if (prefix.empty() || kw.starts_with(prefix) || prefix.find('.') == std::string::npos) {
            items.push_back({kw, 14, "keyword", doc, kw});
        }
    }

    // 2. Built-in Functions
    static const std::vector<std::tuple<std::string, std::string, std::string>> BUILTINS = {
        {"print", "fn print(value)", "Prints value to standard output with newline"},
        {"len", "fn len(collection) -> int", "Returns length of string, list, or map"},
        {"str", "fn str(val) -> string", "Converts value to string representation"},
        {"int", "fn int(val) -> int", "Converts string or float to 64-bit integer"},
        {"float", "fn float(val) -> float", "Converts string or integer to 64-bit float"},
        {"type", "fn type(val) -> string", "Returns type name of the specified value"},
        {"range", "fn range(start, stop, step=1) -> list", "Generates range list"},
        {"abs", "fn abs(num) -> num", "Returns absolute value of number"}
    };

    for (const auto& [name, sig, doc] : BUILTINS) {
        items.push_back({name, 3, sig, doc, name});
    }

    // 3. Member Completion (e.g., io., fs., math., tensor., data., ai.)
    std::string preceding_line_prefix = "";
    if (it != documents_.end()) {
        std::istringstream stream(it->second.content);
        std::string current_line;
        int l = 0;
        while (std::getline(stream, current_line)) {
            if (l == line) {
                if (character <= (int)current_line.size()) {
                    preceding_line_prefix = current_line.substr(0, character);
                }
                break;
            }
            l++;
        }
    }

    if (preceding_line_prefix.ends_with("std.") || prefix.starts_with("std.")) {
        items.clear();
        items.push_back({"io", 9, "module std.io", "Standard Input/Output module", "io"});
        items.push_back({"fs", 9, "module std.fs", "Filesystem I/O module", "fs"});
        items.push_back({"math", 9, "module std.math", "Mathematical functions module", "math"});
        items.push_back({"net", 9, "module std.net", "Networking and HTTP module", "net"});
        items.push_back({"json", 9, "module std.json", "JSON encoding and decoding module", "json"});
        items.push_back({"time", 9, "module std.time", "Time and timer utilities module", "time"});
        items.push_back({"string", 9, "module std.string", "String manipulation module", "string"});
        items.push_back({"crypto", 9, "module std.crypto", "Cryptographic hash functions", "crypto"});
        items.push_back({"process", 9, "module std.process", "Subprocess execution", "process"});
    } else if (preceding_line_prefix.ends_with("io.") || prefix.starts_with("io.")) {
        items.clear();
        items.push_back({"print", 3, "fn print(val)", "Print line to stdout", "print"});
        items.push_back({"print_raw", 3, "fn print_raw(val)", "Print without trailing newline", "print_raw"});
        items.push_back({"read_line", 3, "fn read_line() -> string", "Read line from stdin", "read_line"});
        items.push_back({"print_error", 3, "fn print_error(val)", "Print to stderr", "print_error"});
    } else if (preceding_line_prefix.ends_with("fs.") || prefix.starts_with("fs.")) {
        items.clear();
        items.push_back({"read", 3, "fn read(path: string) -> string", "Reads complete file into string", "read"});
        items.push_back({"write", 3, "fn write(path: string, content: string)", "Writes string content to file", "write"});
        items.push_back({"append", 3, "fn append(path: string, content: string)", "Appends string content to file", "append"});
        items.push_back({"exists", 3, "fn exists(path: string) -> bool", "Checks if file/dir exists", "exists"});
        items.push_back({"list", 3, "fn list(dir: string) -> list", "Lists entries in directory", "list"});
        items.push_back({"remove", 3, "fn remove(path: string)", "Removes file or empty directory", "remove"});
    } else if (preceding_line_prefix.ends_with("math.") || prefix.starts_with("math.")) {
        items.clear();
        items.push_back({"PI", 6, "float = 3.141592653589793", "Pi constant", "PI"});
        items.push_back({"E", 6, "float = 2.718281828459045", "Euler's constant", "E"});
        items.push_back({"sqrt", 3, "fn sqrt(x: float) -> float", "Square root", "sqrt"});
        items.push_back({"pow", 3, "fn pow(base: float, exp: float) -> float", "Power exponentiation", "pow"});
        items.push_back({"sin", 3, "fn sin(x: float) -> float", "Sine function (radians)", "sin"});
        items.push_back({"cos", 3, "fn cos(x: float) -> float", "Cosine function (radians)", "cos"});
        items.push_back({"tan", 3, "fn tan(x: float) -> float", "Tangent function", "tan"});
        items.push_back({"log", 3, "fn log(x: float) -> float", "Natural logarithm", "log"});
        items.push_back({"floor", 3, "fn floor(x: float) -> float", "Floor integer", "floor"});
        items.push_back({"ceil", 3, "fn ceil(x: float) -> float", "Ceil integer", "ceil"});
    } else if (preceding_line_prefix.ends_with("tensor.") || prefix.starts_with("tensor.")) {
        items.clear();
        items.push_back({"zeros", 3, "fn zeros(shape, device=\"cpu\") -> Tensor", "Zero-filled N-D tensor", "zeros"});
        items.push_back({"ones", 3, "fn ones(shape, device=\"cpu\") -> Tensor", "One-filled N-D tensor", "ones"});
        items.push_back({"randn", 3, "fn randn(shape, device=\"cpu\") -> Tensor", "Normally distributed random tensor", "randn"});
        items.push_back({"matmul", 3, "fn matmul(a: Tensor, b: Tensor) -> Tensor", "Matrix multiplication (GEMM)", "matmul"});
        items.push_back({"tensor", 3, "fn tensor(data, requires_grad=false) -> Tensor", "Create tensor from list", "tensor"});
    } else if (preceding_line_prefix.ends_with("data.") || prefix.starts_with("data.")) {
        items.clear();
        items.push_back({"read_csv", 3, "fn read_csv(path: string) -> DataFrame", "Load dataset from CSV file", "read_csv"});
        items.push_back({"DataFrame", 7, "class DataFrame", "Tabular dataset structure", "DataFrame"});
    } else if (preceding_line_prefix.ends_with("ai.") || prefix.starts_with("ai.")) {
        items.clear();
        items.push_back({"Sequential", 7, "class Sequential(layers)", "Sequential neural network container", "Sequential"});
        items.push_back({"Linear", 7, "class Linear(in_features, out_features)", "Dense linear layer", "Linear"});
        items.push_back({"Conv2D", 7, "class Conv2D(in_channels, out_channels, kernel_size)", "2D Convolution layer", "Conv2D"});
        items.push_back({"ReLU", 7, "class ReLU()", "Rectified Linear Unit activation", "ReLU"});
        items.push_back({"Sigmoid", 7, "class Sigmoid()", "Sigmoid activation", "Sigmoid"});
        items.push_back({"Adam", 7, "class Adam(params, lr=0.001)", "Adam optimizer", "Adam"});
        items.push_back({"CrossEntropyLoss", 7, "class CrossEntropyLoss()", "Cross entropy classification loss", "CrossEntropyLoss"});
        items.push_back({"MSELoss", 7, "class MSELoss()", "Mean squared error regression loss", "MSELoss"});
    }

    // 4. In-scope Document Symbols (Functions, Variables)
    if (it != documents_.end() && it->second.ast) {
        for (const auto& stmt : it->second.ast->statements()) {
            if (auto* fn = dynamic_cast<FnDeclStmt*>(stmt.get())) {
                std::string sig = "fn " + fn->name() + "(";
                for (size_t p = 0; p < fn->params().size(); ++p) {
                    if (p > 0) sig += ", ";
                    sig += fn->params()[p].name;
                }
                sig += ")";
                items.push_back({fn->name(), 3, sig, "User-defined function in this document", fn->name()});
            } else if (auto* var = dynamic_cast<LetStmt*>(stmt.get())) {
                items.push_back({var->name(), 6, "let " + var->name(), "Variable in document scope", var->name()});
            }
        }
    }

    std::string result = "[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) result += ",";
        result += items[i].to_json();
    }
    result += "]";

    return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":" + result + "}";
}

// ============================================================================
// Hover Information
// ============================================================================

std::string LanguageServer::handle_hover(int id, const std::string& uri, int line, int character) {
    auto it = documents_.find(uri);
    if (it == documents_.end()) {
        return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":null}";
    }

    int start_col = 0, end_col = 0;
    std::string word = get_word_at(it->second.content, line, character, &start_col, &end_col);
    if (word.empty()) {
        return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":null}";
    }

    LSPHover hover;
    hover.range.start = LSPPosition(line, start_col);
    hover.range.end = LSPPosition(line, end_col);
    hover.has_range = true;

    // Standard Library & Builtins Hover
    if (word == "print") {
        hover.contents = "```nextviper\nfn print(val: any)\n```\nPrints value to standard output followed by a newline.";
    } else if (word == "len") {
        hover.contents = "```nextviper\nfn len(collection: list | map | string) -> int\n```\nReturns the total number of elements in the collection.";
    } else if (word == "range") {
        hover.contents = "```nextviper\nfn range(start: int, stop: int, step: int = 1) -> list[int]\n```\nGenerates an integer sequence list.";
    } else if (word == "sqrt") {
        hover.contents = "```nextviper\nfn std.math.sqrt(x: float) -> float\n```\nComputes the principal square root of `x`.";
    } else if (word == "read" || word == "fs.read") {
        hover.contents = "```nextviper\nfn std.fs.read(path: string) -> string\n```\nReads the entire contents of a file as a UTF-8 string.";
    } else if (word == "write" || word == "fs.write") {
        hover.contents = "```nextviper\nfn std.fs.write(path: string, content: string)\n```\nWrites string content to a file, replacing existing contents.";
    } else if (word == "randn" || word == "tensor.randn") {
        hover.contents = "```nextviper\nfn tensor.randn(shape: list[int], device: string = \"cpu\") -> Tensor\n```\nAllocates an N-dimensional Tensor initialized with standard normal random values.";
    } else if (word == "matmul" || word == "tensor.matmul") {
        hover.contents = "```nextviper\nfn tensor.matmul(a: Tensor, b: Tensor) -> Tensor\n```\nComputes matrix multiplication with Vulkan GPU compute acceleration.";
    } else if (word == "read_csv" || word == "data.read_csv") {
        hover.contents = "```nextviper\nfn data.read_csv(path: string) -> DataFrame\n```\nParses a CSV file into high-speed columnar DataFrame format.";
    } else if (word == "Sequential" || word == "ai.Sequential") {
        hover.contents = "```nextviper\nclass ai.Sequential(layers: list)\n```\nContainer for cascading neural network layers in sequence.";
    } else if (word == "Linear" || word == "ai.Linear") {
        hover.contents = "```nextviper\nclass ai.Linear(in_features: int, out_features: int)\n```\nDense affine transformation layer ($y = xA^T + b$).";
    } else if (word == "Adam" || word == "ai.Adam") {
        hover.contents = "```nextviper\nclass ai.Adam(parameters: list, lr: float = 0.001)\n```\nAdaptive Moment Estimation optimizer for training neural networks.";
    } else if (word == "let") {
        hover.contents = "```nextviper\nkeyword let\n```\nDeclares a local or module-level variable with type inference.";
    } else if (word == "fn") {
        hover.contents = "```nextviper\nkeyword fn\n```\nDeclares a named function with parameters and return statements.";
    } else {
        bool found = false;
        if (it->second.ast) {
            for (const auto& stmt : it->second.ast->statements()) {
                if (auto* fn = dynamic_cast<FnDeclStmt*>(stmt.get())) {
                    if (fn->name() == word) {
                        std::string sig = "fn " + fn->name() + "(";
                        for (size_t p = 0; p < fn->params().size(); ++p) {
                            if (p > 0) sig += ", ";
                            sig += fn->params()[p].name;
                        }
                        sig += ")";
                        hover.contents = "```nextviper\n" + sig + "\n```\nUser function declared in `" + fs::path(it->second.path).filename().string() + "`.";
                        found = true;
                        break;
                    }
                } else if (auto* var = dynamic_cast<LetStmt*>(stmt.get())) {
                    if (var->name() == word) {
                        hover.contents = "```nextviper\nlet " + var->name() + "\n```\nVariable in `" + fs::path(it->second.path).filename().string() + "`.";
                        found = true;
                        break;
                    }
                }
            }
        }

        if (!found) {
            return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":null}";
        }
    }

    return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":" + hover.to_json() + "}";
}

// ============================================================================
// Go to Definition
// ============================================================================

std::string LanguageServer::handle_definition(int id, const std::string& uri, int line, int character) {
    auto it = documents_.find(uri);
    if (it == documents_.end() || !it->second.ast) {
        return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":null}";
    }

    std::string word = get_word_at(it->second.content, line, character);
    if (word.empty()) {
        return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":null}";
    }

    for (const auto& stmt : it->second.ast->statements()) {
        if (auto* fn = dynamic_cast<FnDeclStmt*>(stmt.get())) {
            if (fn->name() == word) {
                LSPLocation loc;
                loc.uri = uri;
                int start_line = std::max(0, (int)fn->span().start.line - 1);
                int start_col = std::max(0, (int)fn->span().start.column - 1);
                loc.range.start = LSPPosition(start_line, start_col);
                loc.range.end = LSPPosition(start_line, start_col + (int)fn->name().size());
                return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":" + loc.to_json() + "}";
            }
        } else if (auto* var = dynamic_cast<LetStmt*>(stmt.get())) {
            if (var->name() == word) {
                LSPLocation loc;
                loc.uri = uri;
                int start_line = std::max(0, (int)var->span().start.line - 1);
                int start_col = std::max(0, (int)var->span().start.column - 1);
                loc.range.start = LSPPosition(start_line, start_col);
                loc.range.end = LSPPosition(start_line, start_col + (int)var->name().size());
                return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":" + loc.to_json() + "}";
            }
        }
    }

    return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":null}";
}

// ============================================================================
// Find References
// ============================================================================

std::string LanguageServer::handle_references(int id, const std::string& uri, int line, int character) {
    auto it = documents_.find(uri);
    if (it == documents_.end()) {
        return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":[]}";
    }

    std::string word = get_word_at(it->second.content, line, character);
    if (word.empty()) {
        return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":[]}";
    }

    std::vector<LSPLocation> references;
    for (const auto& token : it->second.tokens) {
        if (token.text == word) {
            LSPLocation loc;
            loc.uri = uri;
            int t_line = std::max(0, (int)token.line() - 1);
            int t_col = std::max(0, (int)token.column() - 1);
            loc.range.start = LSPPosition(t_line, t_col);
            loc.range.end = LSPPosition(t_line, t_col + (int)token.text.size());
            references.push_back(loc);
        }
    }

    std::string result = "[";
    for (size_t i = 0; i < references.size(); ++i) {
        if (i > 0) result += ",";
        result += references[i].to_json();
    }
    result += "]";

    return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":" + result + "}";
}

// ============================================================================
// Document Symbols & Workspace Symbols
// ============================================================================

std::string LanguageServer::handle_document_symbol(int id, const std::string& uri) {
    auto it = documents_.find(uri);
    if (it == documents_.end() || !it->second.ast) {
        return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":[]}";
    }

    std::vector<LSPDocumentSymbol> symbols;
    for (const auto& stmt : it->second.ast->statements()) {
        if (auto* fn = dynamic_cast<FnDeclStmt*>(stmt.get())) {
            LSPDocumentSymbol sym;
            sym.name = fn->name();
            sym.kind = 12; // Function (12)
            sym.detail = "fn (" + std::to_string(fn->params().size()) + " params)";
            int start_line = std::max(0, (int)fn->span().start.line - 1);
            int end_line = std::max(start_line, (int)fn->span().end.line - 1);
            sym.range.start = LSPPosition(start_line, 0);
            sym.range.end = LSPPosition(end_line, 999);
            sym.selection_range.start = LSPPosition(start_line, (int)fn->span().start.column - 1);
            sym.selection_range.end = LSPPosition(start_line, (int)fn->span().start.column - 1 + (int)fn->name().size());
            symbols.push_back(sym);
        } else if (auto* var = dynamic_cast<LetStmt*>(stmt.get())) {
            LSPDocumentSymbol sym;
            sym.name = var->name();
            sym.kind = 13; // Variable (13)
            sym.detail = "let";
            int start_line = std::max(0, (int)var->span().start.line - 1);
            sym.range.start = LSPPosition(start_line, 0);
            sym.range.end = LSPPosition(start_line, 999);
            sym.selection_range.start = LSPPosition(start_line, (int)var->span().start.column - 1);
            sym.selection_range.end = LSPPosition(start_line, (int)var->span().start.column - 1 + (int)var->name().size());
            symbols.push_back(sym);
        } else if (auto* imp = dynamic_cast<ImportStmt*>(stmt.get())) {
            LSPDocumentSymbol sym;
            sym.name = imp->module_name();
            sym.kind = 2; // Module (2)
            sym.detail = "import";
            int start_line = std::max(0, (int)imp->span().start.line - 1);
            sym.range.start = LSPPosition(start_line, 0);
            sym.range.end = LSPPosition(start_line, 999);
            sym.selection_range = sym.range;
            symbols.push_back(sym);
        }
    }

    std::string result = "[";
    for (size_t i = 0; i < symbols.size(); ++i) {
        if (i > 0) result += ",";
        result += symbols[i].to_json();
    }
    result += "]";

    return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":" + result + "}";
}

std::string LanguageServer::handle_workspace_symbol(int id, const std::string& query) {
    std::string q = query;
    std::transform(q.begin(), q.end(), q.begin(), ::tolower);

    std::string result = "[";
    bool first = true;

    for (const auto& [uri, doc] : documents_) {
        if (!doc.ast) continue;
        for (const auto& stmt : doc.ast->statements()) {
            std::string name = "";
            int kind = 13;
            LSPRange range;

            if (auto* fn = dynamic_cast<FnDeclStmt*>(stmt.get())) {
                name = fn->name();
                kind = 12;
                int start_line = std::max(0, (int)fn->span().start.line - 1);
                range.start = LSPPosition(start_line, 0);
                range.end = LSPPosition(start_line, (int)name.size());
            } else if (auto* var = dynamic_cast<LetStmt*>(stmt.get())) {
                name = var->name();
                kind = 13;
                int start_line = std::max(0, (int)var->span().start.line - 1);
                range.start = LSPPosition(start_line, 0);
                range.end = LSPPosition(start_line, (int)name.size());
            }

            if (!name.empty()) {
                std::string name_lower = name;
                std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
                if (q.empty() || name_lower.find(q) != std::string::npos) {
                    if (!first) result += ",";
                    first = false;
                    result += "{\"name\":\"" + escape_json(name) + "\",\"kind\":" + std::to_string(kind) + ",\"location\":{\"uri\":\"" + uri + "\",\"range\":" + range.to_json() + "}}";
                }
            }
        }
    }

    result += "]";
    return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":" + result + "}";
}

// ============================================================================
// Formatting
// ============================================================================

std::string LanguageServer::handle_formatting(int id, const std::string& uri) {
    auto it = documents_.find(uri);
    if (it == documents_.end()) {
        return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":[]}";
    }

    std::string formatted = Formatter::format_source(it->second.content);
    if (formatted == it->second.content) {
        return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":[]}";
    }

    int line_count = 0;
    for (char c : it->second.content) {
        if (c == '\n') line_count++;
    }

    std::string edit = "{"
        "\"range\":{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":" + std::to_string(line_count + 1) + ",\"character\":0}},"
        "\"newText\":\"" + escape_json(formatted) + "\""
    "}";

    return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":[" + edit + "]}";
}

} // namespace nextviper
