#pragma once

#include "nextviper/common.hpp"
#include "nextviper/diagnostic.hpp"
#include "nextviper/token.hpp"
#include "nextviper/ast.hpp"
#include "nextviper/type_checker.hpp"
#include "nextviper/package_manager.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <iostream>
#include <sstream>

namespace nextviper {

// LSP Range & Position structures (0-indexed per LSP specification)
struct LSPPosition {
    int line = 0;
    int character = 0;

    LSPPosition() = default;
    LSPPosition(int l, int c) : line(l), character(c) {}

    std::string to_json() const {
        return "{\"line\":" + std::to_string(line) + ",\"character\":" + std::to_string(character) + "}";
    }
};

struct LSPRange {
    LSPPosition start;
    LSPPosition end;

    LSPRange() = default;
    LSPRange(LSPPosition s, LSPPosition e) : start(s), end(e) {}
    LSPRange(int s_line, int s_col, int e_line, int e_col) : start(s_line, s_col), end(e_line, e_col) {}

    std::string to_json() const {
        return "{\"start\":" + start.to_json() + ",\"end\":" + end.to_json() + "}";
    }
};

struct LSPLocation {
    std::string uri;
    LSPRange range;

    std::string to_json() const {
        return "{\"uri\":\"" + uri + "\",\"range\":" + range.to_json() + "}";
    }
};

struct LSPDiagnostic {
    LSPRange range;
    int severity = 1; // 1 = Error, 2 = Warning, 3 = Information, 4 = Hint
    std::string code;
    std::string source = "nextviper";
    std::string message;

    std::string to_json() const;
};

struct LSPCompletionItem {
    std::string label;
    int kind = 1; // 1: Text, 2: Method, 3: Function, 6: Variable, 7: Class, 9: Module, 14: Keyword
    std::string detail;
    std::string documentation;
    std::string insert_text;

    std::string to_json() const;
};

struct LSPHover {
    std::string contents;
    LSPRange range;
    bool has_range = false;

    std::string to_json() const;
};

struct LSPDocumentSymbol {
    std::string name;
    std::string detail;
    int kind = 13; // 12: Function, 13: Variable, 2: Module
    LSPRange range;
    LSPRange selection_range;
    std::vector<LSPDocumentSymbol> children;

    std::string to_json() const;
};

struct DocumentState {
    std::string uri;
    std::string path;
    std::string content;
    int version = 0;
    std::vector<Token> tokens;
    std::shared_ptr<Program> ast;
    std::vector<LSPDiagnostic> diagnostics;
};

class LanguageServer {
public:
    LanguageServer(std::istream& in = std::cin, std::ostream& out = std::cout);

    void run();
    void process_message(const std::string& raw_json);

    // Core LSP Handlers
    std::string handle_initialize(int id, const std::string& params_json);
    std::string handle_shutdown(int id);
    void handle_text_document_did_open(const std::string& params_json);
    void handle_text_document_did_change(const std::string& params_json);
    void handle_text_document_did_close(const std::string& params_json);

    // Language Feature Handlers
    std::string handle_completion(int id, const std::string& uri, int line, int character);
    std::string handle_hover(int id, const std::string& uri, int line, int character);
    std::string handle_definition(int id, const std::string& uri, int line, int character);
    std::string handle_references(int id, const std::string& uri, int line, int character);
    std::string handle_document_symbol(int id, const std::string& uri);
    std::string handle_workspace_symbol(int id, const std::string& query);
    std::string handle_formatting(int id, const std::string& uri);

    // Analysis Engine
    void analyze_document(DocumentState& doc);
    void publish_diagnostics(const std::string& uri, const std::vector<LSPDiagnostic>& diags);

    // Helpers
    static std::string uri_to_path(const std::string& uri);
    static std::string path_to_uri(const std::string& path);

    DocumentState* get_document(const std::string& uri);
    void set_workspace_root(const std::string& root_path);

private:
    std::istream& input_stream_;
    std::ostream& output_stream_;
    bool is_initialized_ = false;
    bool is_shutdown_ = false;
    std::string workspace_root_;
    std::unordered_map<std::string, DocumentState> documents_;

    void send_response(const std::string& json_payload);
    void send_notification(const std::string& method, const std::string& params_json);

    std::string find_token_at(const DocumentState& doc, int line, int col, Token* out_token = nullptr);
    std::string get_word_at(const std::string& text, int line, int col, int* out_start_col = nullptr, int* out_end_col = nullptr);
};

} // namespace nextviper
