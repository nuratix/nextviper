#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <filesystem>
#include <iostream>

namespace nextviper {

// ============================================================================
// Semantic Versioning (SemVer 2.0.0)
// ============================================================================

struct SemVer {
    int major = 0;
    int minor = 0;
    int patch = 0;
    std::string prerelease; // e.g. "alpha.1", "beta"
    std::string build;      // e.g. "build.123"

    SemVer() = default;
    SemVer(int maj, int min, int pat, std::string pre = "", std::string bld = "")
        : major(maj), minor(min), patch(pat), prerelease(std::move(pre)), build(std::move(bld)) {}

    static std::optional<SemVer> parse(const std::string& str);
    std::string to_string() const;

    bool operator==(const SemVer& other) const;
    bool operator!=(const SemVer& other) const { return !(*this == other); }
    bool operator<(const SemVer& other) const;
    bool operator<=(const SemVer& other) const { return *this < other || *this == other; }
    bool operator>(const SemVer& other) const { return !(*this <= other); }
    bool operator>=(const SemVer& other) const { return !(*this < other); }
};

enum class VersionOp {
    EXACT,       // =1.2.3 or 1.2.3
    CARET,       // ^1.2.3 (compatible updates)
    TILDE,       // ~1.2.3 (patch updates)
    GREATER,     // >1.2.3
    GREATER_EQ,  // >=1.2.3
    LESS,        // <1.2.3
    LESS_EQ,     // <=1.2.3
    ANY          // *
};

struct VersionConstraint {
    VersionOp op = VersionOp::EXACT;
    SemVer version;

    bool matches(const SemVer& ver) const;
    std::string to_string() const;
};

struct VersionRequirement {
    std::vector<VersionConstraint> constraints;
    std::string raw_string;

    static VersionRequirement parse(const std::string& req_str);
    static VersionRequirement any() {
        VersionRequirement r;
        r.constraints.push_back({VersionOp::ANY, SemVer(0, 0, 0)});
        r.raw_string = "*";
        return r;
    }

    bool matches(const SemVer& ver) const;
    std::string to_string() const { return raw_string.empty() ? "*" : raw_string; }
};

// ============================================================================
// Dependency Specification & Source Types
// ============================================================================

enum class DependencySourceType {
    LOCAL_PATH,
    GIT,
    REGISTRY
};

struct DependencySpec {
    std::string name;
    VersionRequirement version_req = VersionRequirement::any();
    DependencySourceType source_type = DependencySourceType::LOCAL_PATH;
    std::string path;      // For local path dependencies
    std::string git_url;   // For git dependencies
    std::string git_ref;   // Git tag / branch / commit

    std::string to_toml_value() const;
};

// ============================================================================
// Manifest (nextviper.toml)
// ============================================================================

struct ProjectManifest {
    std::string name = "my_project";
    SemVer version = SemVer(0, 1, 0);
    std::string description;
    std::vector<std::string> authors;
    std::string license = "MIT";
    std::string repository;
    std::string homepage;
    std::string documentation;
    std::string category = "general";
    std::vector<std::string> keywords;
    std::string main_file = "src/main.nv";

    std::map<std::string, DependencySpec> dependencies;
    std::map<std::string, DependencySpec> dev_dependencies;
    std::map<std::string, std::string> scripts;

    static std::optional<ProjectManifest> load_from_file(const std::filesystem::path& file_path, std::string& error_msg);
    static std::optional<ProjectManifest> parse_toml(const std::string& content, std::string& error_msg);

    bool save_to_file(const std::filesystem::path& file_path, std::string& error_msg) const;
    std::string to_toml_string() const;
};

// ============================================================================
// Lockfile (nextviper.lock)
// ============================================================================

struct LockedPackage {
    std::string name;
    SemVer version;
    std::string source;      // e.g. "path:../my_lib", "git:https://...#v0.1.0"
    std::string checksum;    // SHA-256 tree checksum
    std::vector<std::string> dependencies; // names of resolved sub-dependencies
};

struct Lockfile {
    int lockfile_version = 1;
    std::map<std::string, LockedPackage> packages;

    static std::optional<Lockfile> load_from_file(const std::filesystem::path& file_path, std::string& error_msg);
    bool save_to_file(const std::filesystem::path& file_path, std::string& error_msg) const;
    std::string to_lock_string() const;
};

// ============================================================================
// Package Integrity & Checksum Engine
// ============================================================================

class PackageIntegrity {
public:
    static std::string compute_tree_hash(const std::filesystem::path& package_dir);
    static bool verify_tree_hash(const std::filesystem::path& package_dir, const std::string& expected_hash);
    static std::string sha256_hex(const std::string& data);
};

// ============================================================================
// Dependency Resolution Graph
// ============================================================================

struct ResolvedPackageNode {
    std::string name;
    SemVer version;
    DependencySourceType source_type;
    std::string source_location;
    std::string checksum;
    std::filesystem::path local_package_path;
    std::vector<std::string> dependency_names;
};

class DependencyResolver {
public:
    struct ResolutionResult {
        bool success = false;
        std::map<std::string, ResolvedPackageNode> resolved_graph;
        std::vector<std::string> resolution_order;
        std::string error_message;
    };

    static ResolutionResult resolve(const ProjectManifest& root_manifest,
                                    const std::filesystem::path& root_dir,
                                    const std::optional<Lockfile>& existing_lock = std::nullopt);
};

// ============================================================================
// Package Manager Engine
// ============================================================================

class PackageManager {
public:
    explicit PackageManager(std::filesystem::path project_root = std::filesystem::current_path())
        : project_root_(std::move(project_root)) {}

    // CLI Commands
    int cmd_init(const std::string& name = "");
    int cmd_add(const std::string& pkg_name, const std::string& version_or_path, bool is_path = false, const std::string& git_url = "", const std::string& git_ref = "");
    int cmd_remove(const std::string& pkg_name);
    int cmd_install();
    int cmd_update(const std::string& pkg_name = "");
    int cmd_list();
    int cmd_publish(bool dry_run = false,
                    const std::string& access = "",
                    const std::string& token = "",
                    const std::string& user = "",
                    const std::string& registry_url = "");

    std::filesystem::path manifest_path() const { return project_root_ / "nextviper.toml"; }
    std::filesystem::path lockfile_path() const { return project_root_ / "nextviper.lock"; }
    std::filesystem::path packages_dir() const { return project_root_ / ".nextviper" / "packages"; }
    std::filesystem::path modules_dir() const { return project_root_ / "nextviper_modules"; }

private:
    std::filesystem::path project_root_;

    bool ensure_module_symlink(const std::string& pkg_name, const std::filesystem::path& target_dir, std::string& err);
};

} // namespace nextviper
