#include "test_runner.hpp"
#include "nextviper/lsp.hpp"
#include <sstream>
#include <iostream>

using namespace nextviper;

static std::string escape_for_json(const std::string& str) {
    std::string out;
    for (char c : str) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

NV_TEST(LSP, InitializeHandshake) {
    std::stringstream in;
    std::stringstream out;
    LanguageServer server(in, out);

    std::string init_req = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"rootUri\":\"file:///workspace\"}}";
    server.process_message(init_req);

    std::string resp = out.str();
    NV_ASSERT_TRUE(resp.find("\"completionProvider\"") != std::string::npos);
    NV_ASSERT_TRUE(resp.find("\"hoverProvider\":true") != std::string::npos);
    NV_ASSERT_TRUE(resp.find("\"definitionProvider\":true") != std::string::npos);
    NV_ASSERT_TRUE(resp.find("\"documentFormattingProvider\":true") != std::string::npos);
}

NV_TEST(LSP, DocumentDidOpenAndDiagnostics) {
    std::stringstream in;
    std::stringstream out;
    LanguageServer server(in, out);

    // Open valid document
    std::string valid_code = "import std.io\nfn add(a, b):\n    return a + b\nlet res = add(10, 20)\nio.print(res)\n";
    DocumentState doc;
    doc.uri = "file:///workspace/main.nv";
    doc.path = "/workspace/main.nv";
    doc.content = valid_code;

    server.analyze_document(doc);
    NV_ASSERT_EQ(doc.diagnostics.size(), 0);

    // Open document with syntax error
    std::string err_code = "let x = \n";
    DocumentState err_doc;
    err_doc.uri = "file:///workspace/err.nv";
    err_doc.path = "/workspace/err.nv";
    err_doc.content = err_code;

    server.analyze_document(err_doc);
    NV_ASSERT_TRUE(err_doc.diagnostics.size() > 0);
    NV_ASSERT_EQ(err_doc.diagnostics[0].severity, 1);
}

NV_TEST(LSP, Autocompletion) {
    std::stringstream in;
    std::stringstream out;
    LanguageServer server(in, out);

    std::string code = "import std.io\nlet count = 42\nfn compute(x):\n    return x * 2\n";
    std::string open_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":\"file:///test.nv\",\"version\":1,\"text\":\"" + escape_for_json(code) + "\"}}}";
    server.process_message(open_msg);

    // 1. Completion for keywords & builtins
    std::string comp_res = server.handle_completion(2, "file:///test.nv", 4, 0);
    NV_ASSERT_TRUE(comp_res.find("\"label\":\"let\"") != std::string::npos);
    NV_ASSERT_TRUE(comp_res.find("\"label\":\"fn\"") != std::string::npos);
    NV_ASSERT_TRUE(comp_res.find("\"label\":\"print\"") != std::string::npos);

    // 2. Member completion for std.
    std::string std_open = "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":\"file:///std_test.nv\",\"version\":1,\"text\":\"import std.\"}}}";
    server.process_message(std_open);

    std::string std_comp = server.handle_completion(3, "file:///std_test.nv", 0, 11);
    NV_ASSERT_TRUE(std_comp.find("\"label\":\"io\"") != std::string::npos);
    NV_ASSERT_TRUE(std_comp.find("\"label\":\"fs\"") != std::string::npos);
    NV_ASSERT_TRUE(std_comp.find("\"label\":\"math\"") != std::string::npos);

    // 3. Member completion for io.
    std::string io_open = "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":\"file:///io_test.nv\",\"version\":1,\"text\":\"io.\"}}}";
    server.process_message(io_open);
    std::string io_comp = server.handle_completion(4, "file:///io_test.nv", 0, 3);
    NV_ASSERT_TRUE(io_comp.find("\"label\":\"print\"") != std::string::npos);
    NV_ASSERT_TRUE(io_comp.find("\"label\":\"read_line\"") != std::string::npos);
}

NV_TEST(LSP, HoverInformation) {
    std::stringstream in;
    std::stringstream out;
    LanguageServer server(in, out);

    std::string code = "import std.io\nfn calculate_total(price, tax):\n    return price + tax\nlet val = print(calculate_total(100, 20))\n";
    std::string open_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":\"file:///hover.nv\",\"version\":1,\"text\":\"" + escape_for_json(code) + "\"}}}";
    server.process_message(open_msg);

    // Hover over print builtin
    std::string hover_print = server.handle_hover(5, "file:///hover.nv", 3, 11);
    NV_ASSERT_TRUE(hover_print.find("fn print(val: any)") != std::string::npos);

    // Hover over user function calculate_total
    std::string hover_fn = server.handle_hover(6, "file:///hover.nv", 3, 20);
    NV_ASSERT_TRUE(hover_fn.find("fn calculate_total(price, tax)") != std::string::npos);
}

NV_TEST(LSP, DefinitionAndReferences) {
    std::stringstream in;
    std::stringstream out;
    LanguageServer server(in, out);

    std::string code = "let total = 100\nfn calculate():\n    return total * 2\nlet result = calculate()\n";
    std::string open_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":\"file:///def.nv\",\"version\":1,\"text\":\"" + escape_for_json(code) + "\"}}}";
    server.process_message(open_msg);

    // Definition of calculate
    std::string def_res = server.handle_definition(7, "file:///def.nv", 3, 15);
    NV_ASSERT_TRUE(def_res.find("\"uri\":\"file:///def.nv\"") != std::string::npos);
    NV_ASSERT_TRUE(def_res.find("\"line\":1") != std::string::npos);

    // References of total
    std::string ref_res = server.handle_references(8, "file:///def.nv", 0, 5);
    NV_ASSERT_TRUE(ref_res.find("\"line\":0") != std::string::npos);
    NV_ASSERT_TRUE(ref_res.find("\"line\":2") != std::string::npos);
}

NV_TEST(LSP, DocumentSymbolsAndFormatting) {
    std::stringstream in;
    std::stringstream out;
    LanguageServer server(in, out);

    std::string code = "import std.io\nlet greeting = \"hello\"\nfn greet(name):\n    io.print(greeting + name)\n";
    std::string open_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":\"file:///sym.nv\",\"version\":1,\"text\":\"" + escape_for_json(code) + "\"}}}";
    server.process_message(open_msg);

    // Document symbols
    std::string sym_res = server.handle_document_symbol(9, "file:///sym.nv");
    NV_ASSERT_TRUE(sym_res.find("\"name\":\"greet\"") != std::string::npos);
    NV_ASSERT_TRUE(sym_res.find("\"name\":\"greeting\"") != std::string::npos);
    NV_ASSERT_TRUE(sym_res.find("\"name\":\"std.io\"") != std::string::npos);

    // Formatting
    std::string unformatted = "let a=1+2\nfn foo(x,y):\n  return x+y\n";
    std::string un_open = "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":\"file:///fmt.nv\",\"version\":1,\"text\":\"" + escape_for_json(unformatted) + "\"}}}";
    server.process_message(un_open);

    std::string fmt_res = server.handle_formatting(10, "file:///fmt.nv");
    NV_ASSERT_TRUE(fmt_res.find("let a = 1 + 2") != std::string::npos);
}
