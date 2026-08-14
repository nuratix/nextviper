#include "test_runner.hpp"
#include "nextviper/package_manager.hpp"
#include "nextviper/interpreter.hpp"
#include "nextviper/module.hpp"
#include "nextviper/parser.hpp"
#include "nextviper/lexer.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

using namespace nextviper;
namespace fs = std::filesystem;

NV_TEST(PackageManager, SemVerParsingAndComparison) {
    auto v1 = SemVer::parse("1.2.3");
    NV_ASSERT_TRUE(v1.has_value());
    NV_ASSERT_EQ(v1->major, 1);
    NV_ASSERT_EQ(v1->minor, 2);
    NV_ASSERT_EQ(v1->patch, 3);
    NV_ASSERT_EQ(v1->to_string(), "1.2.3");

    auto v2 = SemVer::parse("v2.0.0-alpha.1+build.100");
    NV_ASSERT_TRUE(v2.has_value());
    NV_ASSERT_EQ(v2->major, 2);
    NV_ASSERT_EQ(v2->prerelease, "alpha.1");
    NV_ASSERT_EQ(v2->build, "build.100");

    SemVer a(1, 0, 0);
    SemVer b(1, 0, 1);
    SemVer c(1, 1, 0);
    SemVer d(2, 0, 0);

    NV_ASSERT_TRUE(a < b);
    NV_ASSERT_TRUE(b < c);
    NV_ASSERT_TRUE(c < d);
    NV_ASSERT_TRUE(d > a);
    NV_ASSERT_TRUE(a == SemVer(1, 0, 0));
}

NV_TEST(PackageManager, VersionRequirementMatching) {
    auto req_caret = VersionRequirement::parse("^1.2.3");
    NV_ASSERT_TRUE(req_caret.matches(SemVer(1, 2, 3)));
    NV_ASSERT_TRUE(req_caret.matches(SemVer(1, 9, 0)));
    NV_ASSERT_FALSE(req_caret.matches(SemVer(1, 2, 2)));
    NV_ASSERT_FALSE(req_caret.matches(SemVer(2, 0, 0)));

    auto req_zero_caret = VersionRequirement::parse("^0.2.3");
    NV_ASSERT_TRUE(req_zero_caret.matches(SemVer(0, 2, 3)));
    NV_ASSERT_TRUE(req_zero_caret.matches(SemVer(0, 2, 9)));
    NV_ASSERT_FALSE(req_zero_caret.matches(SemVer(0, 3, 0)));

    auto req_tilde = VersionRequirement::parse("~1.2.3");
    NV_ASSERT_TRUE(req_tilde.matches(SemVer(1, 2, 3)));
    NV_ASSERT_TRUE(req_tilde.matches(SemVer(1, 2, 8)));
    NV_ASSERT_FALSE(req_tilde.matches(SemVer(1, 3, 0)));

    auto req_any = VersionRequirement::parse("*");
    NV_ASSERT_TRUE(req_any.matches(SemVer(0, 0, 1)));
    NV_ASSERT_TRUE(req_any.matches(SemVer(99, 99, 99)));
}

NV_TEST(PackageManager, ManifestTomlParsingAndSerialization) {
    std::string toml_content = R"(
[project]
name = "my_app"
version = "1.2.0"
description = "High-performance app"
license = "MIT"
main = "src/main.nv"
authors = ["Junaid <junaid@nextviper.org>"]

[dependencies]
math_utils = { path = "../math_utils" }
data = "^1.0.0"
http_client = { git = "https://github.com/example/http.git", tag = "v1.0.0" }

[scripts]
start = "nextviper run src/main.nv"
)";

    std::string err;
    auto manifest_opt = ProjectManifest::parse_toml(toml_content, err);
    NV_ASSERT_TRUE(manifest_opt.has_value());
    
    const auto& m = *manifest_opt;
    NV_ASSERT_EQ(m.name, "my_app");
    NV_ASSERT_EQ(m.version.to_string(), "1.2.0");
    NV_ASSERT_EQ(m.description, "High-performance app");
    NV_ASSERT_EQ(m.dependencies.size(), 3);
    NV_ASSERT_EQ(m.dependencies.at("math_utils").source_type, DependencySourceType::LOCAL_PATH);
    NV_ASSERT_EQ(m.dependencies.at("math_utils").path, "../math_utils");
    NV_ASSERT_EQ(m.dependencies.at("data").source_type, DependencySourceType::REGISTRY);
    NV_ASSERT_EQ(m.dependencies.at("http_client").source_type, DependencySourceType::GIT);
    NV_ASSERT_EQ(m.dependencies.at("http_client").git_ref, "v1.0.0");

    std::string generated_toml = m.to_toml_string();
    auto re_parsed = ProjectManifest::parse_toml(generated_toml, err);
    NV_ASSERT_TRUE(re_parsed.has_value());
    NV_ASSERT_EQ(re_parsed->name, m.name);
    NV_ASSERT_EQ(re_parsed->dependencies.size(), m.dependencies.size());
}

NV_TEST(PackageManager, LockfileGenerationAndParsing) {
    fs::path temp_lock = "/tmp/test_nextviper.lock";
    
    Lockfile lock;
    lock.lockfile_version = 1;

    LockedPackage lp1;
    lp1.name = "math_utils";
    lp1.version = SemVer(0, 5, 0);
    lp1.source = "path:../math_utils";
    lp1.checksum = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    lp1.dependencies = {"tensor_core"};
    lock.packages["math_utils"] = lp1;

    std::string err;
    NV_ASSERT_TRUE(lock.save_to_file(temp_lock, err));

    auto loaded = Lockfile::load_from_file(temp_lock, err);
    NV_ASSERT_TRUE(loaded.has_value());
    NV_ASSERT_EQ(loaded->packages.size(), 1);
    NV_ASSERT_EQ(loaded->packages["math_utils"].name, "math_utils");
    NV_ASSERT_EQ(loaded->packages["math_utils"].version.to_string(), "0.5.0");
    NV_ASSERT_EQ(loaded->packages["math_utils"].checksum, lp1.checksum);

    fs::remove(temp_lock);
}

NV_TEST(PackageManager, PackageIntegrityTreeHashing) {
    fs::path test_dir = "/tmp/nv_test_integrity_pkg";
    fs::remove_all(test_dir);
    fs::create_directories(test_dir / "src");

    std::ofstream f1(test_dir / "nextviper.toml");
    f1 << "[project]\nname = \"pkg_a\"\nversion = \"1.0.0\"\n";
    f1.close();

    std::ofstream f2(test_dir / "src" / "main.nv");
    f2 << "export fn calculate(): return 42\n";
    f2.close();

    std::string hash1 = PackageIntegrity::compute_tree_hash(test_dir);
    NV_ASSERT_FALSE(hash1.empty());
    NV_ASSERT_EQ(hash1.size(), 64);
    NV_ASSERT_TRUE(PackageIntegrity::verify_tree_hash(test_dir, hash1));

    // Modifying a file must change hash
    std::ofstream f2_mod(test_dir / "src" / "main.nv", std::ios::app);
    f2_mod << "// modification\n";
    f2_mod.close();

    std::string hash2 = PackageIntegrity::compute_tree_hash(test_dir);
    NV_ASSERT_TRUE(hash1 != hash2);
    NV_ASSERT_FALSE(PackageIntegrity::verify_tree_hash(test_dir, hash1));

    fs::remove_all(test_dir);
}

NV_TEST(PackageManager, DependencyResolutionAndCycleDetection) {
    fs::path base_test = "/tmp/nv_test_resolution";
    fs::remove_all(base_test);
    
    fs::path pkg_a = base_test / "pkg_a";
    fs::path pkg_b = base_test / "pkg_b";
    fs::create_directories(pkg_a / "src");
    fs::create_directories(pkg_b / "src");

    // Circular: A -> B -> A
    std::ofstream ma(pkg_a / "nextviper.toml");
    ma << "[project]\nname = \"pkg_a\"\nversion = \"1.0.0\"\n[dependencies]\npkg_b = { path = \"../pkg_b\" }\n";
    ma.close();

    std::ofstream mb(pkg_b / "nextviper.toml");
    mb << "[project]\nname = \"pkg_b\"\nversion = \"1.0.0\"\n[dependencies]\npkg_a = { path = \"../pkg_a\" }\n";
    mb.close();

    std::string err;
    auto root_m = ProjectManifest::load_from_file(pkg_a / "nextviper.toml", err);
    NV_ASSERT_TRUE(root_m.has_value());

    auto res = DependencyResolver::resolve(*root_m, pkg_a);
    NV_ASSERT_FALSE(res.success);
    NV_ASSERT_TRUE(res.error_message.find("Circular dependency detected") != std::string::npos);

    fs::remove_all(base_test);
}

NV_TEST(PackageManager, LocalPackageEndToEndWorkflow) {
    fs::path base_test = "/tmp/nv_test_pm_e2e";
    fs::remove_all(base_test);

    // 1. Create a local library: `string_helper`
    fs::path lib_dir = base_test / "string_helper";
    fs::create_directories(lib_dir / "src");

    std::ofstream lib_manifest(lib_dir / "nextviper.toml");
    lib_manifest << "[project]\n"
                 << "name = \"string_helper\"\n"
                 << "version = \"0.2.0\"\n"
                 << "description = \"String helper utility package\"\n"
                 << "license = \"MIT\"\n"
                 << "main = \"src/main.nv\"\n";
    lib_manifest.close();

    std::ofstream lib_code(lib_dir / "src" / "main.nv");
    lib_code << "// string_helper package\n"
             << "import std.string\n\n"
             << "export fn emphasize(s):\n"
             << "    return string.to_upper(s) + \"!\"\n";
    lib_code.close();

    // 2. Initialize application: `demo_app`
    fs::path app_dir = base_test / "demo_app";
    fs::create_directories(app_dir);

    PackageManager pm(app_dir);
    int init_res = pm.cmd_init("demo_app");
    NV_ASSERT_EQ(init_res, 0);
    NV_ASSERT_TRUE(fs::exists(app_dir / "nextviper.toml"));

    // 3. Add `string_helper` dependency via local path
    int add_res = pm.cmd_add("string_helper", "../string_helper", true);
    NV_ASSERT_EQ(add_res, 0);
    NV_ASSERT_TRUE(fs::exists(app_dir / "nextviper.lock"));

    // 4. Write application code importing `string_helper`
    std::ofstream app_code(app_dir / "src" / "main.nv");
    app_code << "import string_helper\n"
             << "let res = string_helper.emphasize(\"nextviper packages work\")\n"
             << "print(res)\n";
    app_code.close();

    // 5. Execute application using Interpreter
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    
    std::string app_src_content = "import string_helper\nlet res = string_helper.emphasize(\"nextviper packages work\")\nprint(res)\n";
    Lexer lexer(app_src_content, (app_dir / "src" / "main.nv").string(), diag);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, diag);
    auto program = parser.parse_program();
    NV_ASSERT_TRUE(program != nullptr);

    Interpreter interp(diag);
    interp.module_manager()->add_search_path((app_dir / "nextviper_modules").string());
    
    std::stringstream ss;
    auto* old_buf = std::cout.rdbuf(ss.rdbuf());
    interp.execute(*program);
    std::cout.rdbuf(old_buf);

    std::string output = ss.str();
    NV_ASSERT_TRUE(output.find("NEXTVIPER PACKAGES WORK!") != std::string::npos);

    // 6. Test `list` and `publish (dry-run)`
    NV_ASSERT_EQ(pm.cmd_list(), 0);
    NV_ASSERT_EQ(pm.cmd_publish(true), 0);

    // 7. Test `remove`
    NV_ASSERT_EQ(pm.cmd_remove("string_helper"), 0);

    fs::remove_all(base_test);
}
