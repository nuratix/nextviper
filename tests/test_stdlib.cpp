#include "test_runner.hpp"
#include "nextviper/interpreter.hpp"
#include "nextviper/parser.hpp"
#include "nextviper/lexer.hpp"
#include "nextviper/native_compiler.hpp"
#include <iostream>
#include <sstream>
#include <filesystem>

using namespace nextviper;
namespace fs = std::filesystem;

namespace {

std::string run_script(const std::string& source) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Lexer lexer(source, "<test_stdlib>", diag);
    auto tokens = lexer.tokenize();
    if (diag.has_errors()) throw std::runtime_error("Lexer error");

    Parser parser(tokens, diag);
    auto program = parser.parse_program();
    if (diag.has_errors() || !program) throw std::runtime_error("Parser error");

    Interpreter interp(diag);
    std::stringstream ss;
    auto* old_buf = std::cout.rdbuf(ss.rdbuf());
    interp.execute(*program);
    std::cout.rdbuf(old_buf);
    return ss.str();
}

} // anonymous namespace

NV_TEST(StdLib, StdFsOperations) {
    std::string code = R"nv(
        import std.fs
        let test_dir = "/tmp/nv_test_fs_dir"
        fs.make_dir(test_dir)
        if (!fs.is_dir(test_dir)) { print("FAIL_DIR"); }

        let file_path = test_dir + "/sample.txt"
        fs.write_text(file_path, "Hello NextViper FS!")
        if (!fs.exists(file_path)) { print("FAIL_EXISTS"); }
        if (!fs.is_file(file_path)) { print("FAIL_IS_FILE"); }

        let content = fs.read_text(file_path)
        if (content != "Hello NextViper FS!") { print("FAIL_CONTENT"); }

        fs.append_text(file_path, "\nAppended line.")
        let updated = fs.read_text(file_path)
        if (updated != "Hello NextViper FS!\nAppended line.") { print("FAIL_APPEND"); }

        let sz = fs.size(file_path)
        if (sz < 10) { print("FAIL_SIZE"); }

        let files = fs.list(test_dir)
        if (files.len() == 0) { print("FAIL_LIST"); }

        let copy_path = test_dir + "/sample_copy.txt"
        fs.copy(file_path, copy_path)
        if (!fs.exists(copy_path)) { print("FAIL_COPY"); }

        fs.remove(copy_path)
        if (fs.exists(copy_path)) { print("FAIL_REMOVE"); }

        fs.remove_dir(test_dir)
        if (fs.exists(test_dir)) { print("FAIL_REMOVE_DIR"); }
        print("FS_OK")
    )nv";

    std::string out = run_script(code);
    NV_ASSERT_TRUE(out.find("FS_OK") != std::string::npos);
}

NV_TEST(StdLib, StdPathOperations) {
    std::string code = R"nv(
        import std.path
        let joined = path.join("home", "user", "docs", "file.nv")
        if (joined != "home/user/docs/file.nv") { print("FAIL_JOIN"); }

        let dir = path.dirname("home/user/file.nv")
        if (dir != "home/user") { print("FAIL_DIRNAME"); }

        let base = path.basename("home/user/file.nv")
        if (base != "file.nv") { print("FAIL_BASENAME"); }

        let ext = path.extname("home/user/file.nv")
        if (ext != ".nv") { print("FAIL_EXT"); }

        if (!path.is_absolute("/usr/local/bin")) { print("FAIL_IS_ABS"); }
        if (path.is_absolute("relative/path")) { print("FAIL_NOT_ABS"); }

        let norm = path.normalize("a/b/../c/./d")
        if (norm != "a/c/d") { print("FAIL_NORM"); }
        print("PATH_OK")
    )nv";

    std::string out = run_script(code);
    NV_ASSERT_TRUE(out.find("PATH_OK") != std::string::npos);
}

NV_TEST(StdLib, StdStringOperations) {
    std::string code = R"nv(
        import std.string
        let s = "  NextViper Language  "
        if (string.trim(s) != "NextViper Language") { print("FAIL_TRIM"); }
        if (string.trim_start(s) != "NextViper Language  ") { print("FAIL_TRIM_START"); }
        if (string.trim_end(s) != "  NextViper Language") { print("FAIL_TRIM_END"); }

        if (string.to_upper("abc") != "ABC") { print("FAIL_UPPER"); }
        if (string.to_lower("XYZ") != "xyz") { print("FAIL_LOWER"); }

        if (!string.starts_with("hello world", "hello")) { print("FAIL_STARTS"); }
        if (!string.ends_with("hello world", "world")) { print("FAIL_ENDS"); }
        if (!string.contains("hello world", "lo wo")) { print("FAIL_CONTAINS"); }
        if (string.index_of("hello world", "world") != 6) { print("FAIL_INDEX"); }

        let parts = string.split("a,b,c,d", ",")
        if (parts.len() != 4 || parts[1] != "b") { print("FAIL_SPLIT"); }

        let joined = string.join(["alpha", "beta", "gamma"], "-")
        if (joined != "alpha-beta-gamma") { print("FAIL_JOIN"); }

        let replaced = string.replace("banana", "a", "o")
        if (replaced != "bonono") { print("FAIL_REPLACE"); }
        if (string.len("12345") != 5) { print("FAIL_LEN"); }
        print("STRING_OK")
    )nv";

    std::string out = run_script(code);
    NV_ASSERT_TRUE(out.find("STRING_OK") != std::string::npos);
}

NV_TEST(StdLib, StdCollectionsOperations) {
    std::string code = R"nv(
        import std.collections
        let chunks = collections.chunk([1, 2, 3, 4, 5], 2)
        if (chunks.len() != 3) { print("FAIL_CHUNK_LEN"); }
        if (chunks[0] != [1, 2]) { print("FAIL_CHUNK_0"); }
        if (chunks[2] != [5]) { print("FAIL_CHUNK_2"); }

        let flat = collections.flatten([[1, 2], [3, 4], 5])
        if (flat != [1, 2, 3, 4, 5]) { print("FAIL_FLAT"); }

        let unq = collections.unique([1, 2, 2, 3, 1, 4])
        if (unq != [1, 2, 3, 4]) { print("FAIL_UNQ"); }

        let rev = collections.reverse([1, 2, 3])
        if (rev != [3, 2, 1]) { print("FAIL_REV"); }

        let srt = collections.sort([5, 2, 8, 1])
        if (srt != [1, 2, 5, 8]) { print("FAIL_SORT"); }

        let zipped = collections.zip(["a", "b"], [10, 20])
        if (zipped[0] != ["a", 10] || zipped[1] != ["b", 20]) { print("FAIL_ZIP"); }

        let m1 = {"a": 1, "b": 2}
        let m2 = {"b": 20, "c": 30}
        let merged = collections.merge(m1, m2)
        if (merged["a"] != 1 || merged["b"] != 20 || merged["c"] != 30) { print("FAIL_MERGE"); }

        let ks = collections.keys(m1)
        if (ks.len() != 2) { print("FAIL_KEYS"); }
        let vs = collections.values(m1)
        if (vs.len() != 2) { print("FAIL_VALUES"); }
        print("COLLECTIONS_OK")
    )nv";

    std::string out = run_script(code);
    NV_ASSERT_TRUE(out.find("COLLECTIONS_OK") != std::string::npos);
}

NV_TEST(StdLib, StdMathOperations) {
    std::string code = R"nv(
        import std.math
        if (math.abs(-42) != 42) { print("FAIL_ABS_INT"); }
        if (math.abs(-3.14) != 3.14) { print("FAIL_ABS_FLOAT"); }
        if (math.sqrt(100) != 10.0) { print("FAIL_SQRT"); }
        if (math.cbrt(27) != 3.0) { print("FAIL_CBRT"); }
        if (math.pow(2, 8) != 256.0) { print("FAIL_POW"); }
        if (math.floor(3.7) != 3) { print("FAIL_FLOOR"); }
        if (math.ceil(3.2) != 4) { print("FAIL_CEIL"); }
        if (math.round(3.5) != 4) { print("FAIL_ROUND"); }
        if (math.min(10, 20) != 10) { print("FAIL_MIN"); }
        if (math.max(10, 20) != 20) { print("FAIL_MAX"); }
        if (math.clamp(15, 0, 10) != 10.0) { print("FAIL_CLAMP"); }
        if (math.pi <= 3.14 || math.pi >= 3.15) { print("FAIL_PI"); }
        if (math.e <= 2.71 || math.e >= 2.72) { print("FAIL_E"); }
        print("MATH_OK")
    )nv";

    std::string out = run_script(code);
    NV_ASSERT_TRUE(out.find("MATH_OK") != std::string::npos);
}

NV_TEST(StdLib, StdJsonOperations) {
    std::string code = R"nv(
        import std.json
        let user = {
            "name": "Junaid",
            "age": 15,
            "skills": ["C++", "NextViper", "AI"],
            "active": true
        }

        let json_text = json.stringify(user)
        if (json_text.len() == 0) { print("FAIL_STRINGIFY"); }

        let parsed = json.parse(json_text)
        if (parsed["name"] != "Junaid") { print("FAIL_PARSE_NAME"); }
        if (parsed["age"] != 15) { print("FAIL_PARSE_AGE"); }
        if (parsed["skills"][1] != "NextViper") { print("FAIL_PARSE_SKILL"); }
        if (parsed["active"] != true) { print("FAIL_PARSE_ACTIVE"); }

        let pretty_json = json.stringify(user, 2)
        if (pretty_json.len() <= json_text.len()) { print("FAIL_PRETTY_LEN"); }
        let parsed_pretty = json.parse(pretty_json)
        if (parsed_pretty["name"] != "Junaid") { print("FAIL_PRETTY_MATCH"); }
        print("JSON_OK")
    )nv";

    std::string out = run_script(code);
    NV_ASSERT_TRUE(out.find("JSON_OK") != std::string::npos);
}

NV_TEST(StdLib, StdCsvOperations) {
    std::string code = R"nv(
        import std.csv
        let csv_data = "name,age,city\nAlice,30,London\nBob,25,Paris"
        let rows = csv.parse(csv_data)
        if (rows.len() != 3) { print("FAIL_CSV_ROWS"); }
        if (rows[0] != ["name", "age", "city"]) { print("FAIL_CSV_HEADER"); }
        if (rows[1] != ["Alice", "30", "London"]) { print("FAIL_CSV_ROW1"); }

        let serialized = csv.stringify(rows)
        let reparsed = csv.parse(serialized)
        if (reparsed[2][0] != "Bob") { print("FAIL_CSV_REPARSE"); }

        let test_path = "/tmp/nv_test_data.csv"
        import std.fs
        fs.write_text(test_path, csv_data)
        let file_rows = csv.read(test_path)
        if (file_rows.len() != 3) { print("FAIL_CSV_READ"); }
        fs.remove(test_path)
        print("CSV_OK")
    )nv";

    std::string out = run_script(code);
    NV_ASSERT_TRUE(out.find("CSV_OK") != std::string::npos);
}

NV_TEST(StdLib, StdTimeOperations) {
    std::string code = R"nv(
        import std.time
        let start = time.now()
        if (start < 1500000000.0) { print("FAIL_TIME_NOW"); }

        let ms = time.now_ms()
        if (ms < 1500000000000) { print("FAIL_TIME_NOW_MS"); }

        time.sleep(10)
        let elapsed = time.elapsed(start)
        if (elapsed < 0.0) { print("FAIL_TIME_ELAPSED"); }

        let fmt = time.format(start, "%Y")
        if (fmt.len() != 4) { print("FAIL_TIME_FMT"); }
        print("TIME_OK")
    )nv";

    std::string out = run_script(code);
    NV_ASSERT_TRUE(out.find("TIME_OK") != std::string::npos);
}

NV_TEST(StdLib, StdProcessOperations) {
    std::string code = R"nv(
        import std.process
        let res = process.exec("echo hello_process")
        if (res["exit_code"] != 0) { print("FAIL_PROC_EXIT"); }
        if (res["stdout"].len() == 0) { print("FAIL_PROC_STDOUT"); }

        let dir = process.cwd()
        if (dir.len() == 0) { print("FAIL_PROC_CWD"); }

        let p = process.pid()
        if (p <= 0) { print("FAIL_PROC_PID"); }
        print("PROCESS_OK")
    )nv";

    std::string out = run_script(code);
    NV_ASSERT_TRUE(out.find("PROCESS_OK") != std::string::npos);
}

NV_TEST(StdLib, StdCryptoOperations) {
    std::string code = R"nv(
        import std.crypto
        let h256 = crypto.sha256("hello")
        if (h256 != "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824") { print("FAIL_SHA256"); }

        let hmd5 = crypto.md5("hello")
        if (hmd5 != "5d41402abc4b2a76b9719d911017c592") { print("FAIL_MD5"); }

        let b64 = crypto.base64_encode("NextViper")
        let dec = crypto.base64_decode(b64)
        if (dec != "NextViper") { print("FAIL_BASE64"); }

        let rnd = crypto.random_bytes(16)
        if (rnd.len() != 32) { print("FAIL_RAND_BYTES"); }
        print("CRYPTO_OK")
    )nv";

    std::string out = run_script(code);
    NV_ASSERT_TRUE(out.find("CRYPTO_OK") != std::string::npos);
}

NV_TEST(StdLib, StdRegexOperations) {
    std::string code = R"nv(
        import std.regex
        if (!regex.test("^[0-9]+$", "12345")) { print("FAIL_RE_TEST_TRUE"); }
        if (regex.test("^[0-9]+$", "123a45")) { print("FAIL_RE_TEST_FALSE"); }

        let matches = regex.match("([a-z]+)@([a-z]+)", "user@domain.com")
        if (matches == nil) { print("FAIL_RE_MATCH_NIL"); }
        if (matches[1] != "user" || matches[2] != "domain") { print("FAIL_RE_MATCH_GROUPS"); }

        let words = regex.find_all("[0-9]+", "apple 10 banana 20 cherry 30")
        if (words != ["10", "20", "30"]) { print("FAIL_RE_FIND_ALL"); }

        let rep = regex.replace("[0-9]", "#", "v1.2.3")
        if (rep != "v#.#.#") { print("FAIL_RE_REPLACE"); }
        print("REGEX_OK")
    )nv";

    std::string out = run_script(code);
    NV_ASSERT_TRUE(out.find("REGEX_OK") != std::string::npos);
}

NV_TEST(StdLib, StdRandomOperations) {
    std::string code = R"nv(
        import std.random
        random.seed(42)
        let r = random.random()
        if (r < 0.0 || r >= 1.0) { print("FAIL_RAND"); }

        let ri = random.randint(10, 20)
        if (ri < 10 || ri > 20) { print("FAIL_RANDINT"); }

        let choices = [10, 20, 30, 40]
        let ch = random.choice(choices)
        if (ch != 10 && ch != 20 && ch != 30 && ch != 40) { print("FAIL_CHOICE"); }

        let shuf = random.shuffle([1, 2, 3, 4, 5])
        if (shuf.len() != 5) { print("FAIL_SHUFFLE"); }
        print("RANDOM_OK")
    )nv";

    std::string out = run_script(code);
    NV_ASSERT_TRUE(out.find("RANDOM_OK") != std::string::npos);
}

NV_TEST(StdLib, StdConcurrencyOperations) {
    std::string code = R"nv(
        import std.concurrency
        let ch = concurrency.channel(10)
        ch.send("msg1")
        ch.send("msg2")
        if (ch.len() != 2) { print("FAIL_CHAN_LEN"); }

        let m1 = ch.recv()
        if (m1 != "msg1") { print("FAIL_CHAN_RECV"); }
        let m2 = ch.try_recv()
        if (m2 != "msg2") { print("FAIL_CHAN_TRY_RECV"); }
        if (ch.try_recv() != nil) { print("FAIL_CHAN_EMPTY"); }

        ch.close()
        print("CONCURRENCY_OK")
    )nv";

    std::string out = run_script(code);
    NV_ASSERT_TRUE(out.find("CONCURRENCY_OK") != std::string::npos);
}

NV_TEST(StdLib, StdHttpMockAndResponse) {
    std::string code = R"nv(
        import std.http
        let res = http.get("http://127.0.0.1:9999/non_existent_mock")
        if (res.status == nil) { print("FAIL_HTTP_STATUS"); }
        if (res.text == nil) { print("FAIL_HTTP_TEXT"); }
        print("HTTP_OK")
    )nv";

    std::string out = run_script(code);
    NV_ASSERT_TRUE(out.find("HTTP_OK") != std::string::npos);
}

NV_TEST(StdLib, InterpreterAndNativeStdlibEquivalence) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    NativeCompiler compiler(diag);

    std::string code = 
        "let a = 16\n"
        "let b = 81\n"
        "let c = 120\n"
        "let d = 90\n"
        "let total = a + b + c + d\n"
        "print(total)\n";

    // 1. Interpreter Output
    std::string interp_out = run_script(code);

    // 2. Native Compilation & Execution
    Lexer lexer(code, "<test_eq>", diag);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, diag);
    auto program = parser.parse_program();
    NV_ASSERT_TRUE(program != nullptr);

    auto [exit_code, native_out] = compiler.compile_and_run(*program, true);
    NV_ASSERT_EQ(exit_code, 0);

    // 3. Assert Equivalence (16 + 81 + 120 + 90 = 307)
    NV_ASSERT_EQ(interp_out, "307\n");
    NV_ASSERT_EQ(native_out, interp_out);
}
