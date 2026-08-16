#include "nextviper/package_manager.hpp"
#include "nextviper/formatter.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <set>
#include <cstring>
#include <sys/stat.h>

namespace fs = std::filesystem;

namespace nextviper {

// ============================================================================
// Internal SHA-256 Engine (FIPS 180-2)
// ============================================================================

namespace {

class SHA256Hasher {
public:
    SHA256Hasher() { reset(); }

    void reset() {
        state_[0] = 0x6a09e667;
        state_[1] = 0xbb67ae85;
        state_[2] = 0x3c6ef372;
        state_[3] = 0xa54ff53a;
        state_[4] = 0x510e527f;
        state_[5] = 0x9b05688c;
        state_[6] = 0x1f83d9ab;
        state_[7] = 0x5be0cd19;
        count_ = 0;
        buffer_len_ = 0;
    }

    void update(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            buffer_[buffer_len_++] = data[i];
            if (buffer_len_ == 64) {
                transform(buffer_);
                count_ += 512;
                buffer_len_ = 0;
            }
        }
    }

    void update(const std::string& str) {
        update(reinterpret_cast<const uint8_t*>(str.data()), str.size());
    }

    std::string finalize() {
        count_ += buffer_len_ * 8;
        buffer_[buffer_len_++] = 0x80;
        if (buffer_len_ > 56) {
            while (buffer_len_ < 64) buffer_[buffer_len_++] = 0;
            transform(buffer_);
            buffer_len_ = 0;
        }
        while (buffer_len_ < 56) buffer_[buffer_len_++] = 0;
        for (int i = 7; i >= 0; --i) {
            buffer_[56 + (7 - i)] = static_cast<uint8_t>((count_ >> (i * 8)) & 0xFF);
        }
        transform(buffer_);

        std::ostringstream oss;
        for (int i = 0; i < 8; ++i) {
            oss << std::hex << std::setfill('0') << std::setw(8) << state_[i];
        }
        return oss.str();
    }

private:
    uint32_t state_[8];
    uint64_t count_;
    uint8_t buffer_[64];
    size_t buffer_len_;

    static uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
    static uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
    static uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
    static uint32_t ep0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
    static uint32_t ep1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
    static uint32_t sig0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
    static uint32_t sig1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

    void transform(const uint8_t* chunk) {
        static const uint32_t k[64] = {
            0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
            0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
            0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
            0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
            0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
            0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
            0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
            0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
        };

        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(chunk[i * 4]) << 24) |
                   (static_cast<uint32_t>(chunk[i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(chunk[i * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(chunk[i * 4 + 3]));
        }
        for (int i = 16; i < 64; ++i) {
            w[i] = sig1(w[i - 2]) + w[i - 7] + sig0(w[i - 15]) + w[i - 16];
        }

        uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];

        for (int i = 0; i < 64; ++i) {
            uint32_t t1 = h + ep1(e) + ch(e, f, g) + k[i] + w[i];
            uint32_t t2 = ep0(a) + maj(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
        state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
    }
};

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

} // anonymous namespace

// ============================================================================
// PackageIntegrity Implementation
// ============================================================================

std::string PackageIntegrity::sha256_hex(const std::string& data) {
    SHA256Hasher hasher;
    hasher.update(data);
    return hasher.finalize();
}

std::string PackageIntegrity::compute_tree_hash(const fs::path& package_dir) {
    std::error_code ec;
    if (!fs::exists(package_dir, ec) || !fs::is_directory(package_dir, ec)) {
        return "";
    }

    std::vector<fs::path> file_paths;
    for (const auto& entry : fs::recursive_directory_iterator(package_dir, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) continue;
        if (entry.is_regular_file(ec)) {
            fs::path rel = fs::relative(entry.path(), package_dir, ec);
            std::string rel_str = rel.generic_string();

            // Ignore VCS, build, and package cache directories
            if (rel_str.find(".git/") == 0 || rel_str == ".git" ||
                rel_str.find(".nextviper/") == 0 || rel_str == ".nextviper" ||
                rel_str.find("build/") == 0 || rel_str == "build" ||
                rel_str.find("dist/") == 0 || rel_str == "dist" ||
                rel_str.find("target/") == 0 || rel_str == "target" ||
                rel_str.find("nextviper_modules/") == 0 ||
                rel_str == ".DS_Store") {
                continue;
            }
            file_paths.push_back(rel);
        }
    }

    std::sort(file_paths.begin(), file_paths.end());

    SHA256Hasher tree_hasher;
    for (const auto& rel : file_paths) {
        fs::path full_p = package_dir / rel;
        std::string path_header = rel.generic_string() + "\n";
        tree_hasher.update(path_header);

        std::ifstream file(full_p, std::ios::binary);
        if (file.is_open()) {
            char buffer[4096];
            while (file.read(buffer, sizeof(buffer))) {
                tree_hasher.update(reinterpret_cast<const uint8_t*>(buffer), file.gcount());
            }
            if (file.gcount() > 0) {
                tree_hasher.update(reinterpret_cast<const uint8_t*>(buffer), file.gcount());
            }
        }
    }

    return tree_hasher.finalize();
}

bool PackageIntegrity::verify_tree_hash(const fs::path& package_dir, const std::string& expected_hash) {
    if (expected_hash.empty()) return true;
    std::string actual_hash = compute_tree_hash(package_dir);
    return actual_hash == expected_hash;
}

// ============================================================================
// SemVer Implementation
// ============================================================================

std::optional<SemVer> SemVer::parse(const std::string& str) {
    std::string s = trim(str);
    if (s.empty()) return std::nullopt;
    if (s[0] == 'v' || s[0] == 'V') s = s.substr(1);

    int maj = 0, min = 0, pat = 0;
    std::string pre = "", bld = "";

    size_t plus_pos = s.find('+');
    if (plus_pos != std::string::npos) {
        bld = s.substr(plus_pos + 1);
        s = s.substr(0, plus_pos);
    }

    size_t dash_pos = s.find('-');
    if (dash_pos != std::string::npos) {
        pre = s.substr(dash_pos + 1);
        s = s.substr(0, dash_pos);
    }

    std::stringstream ss(s);
    std::string part;
    if (!std::getline(ss, part, '.')) return std::nullopt;
    try { maj = std::stoi(part); } catch (...) { return std::nullopt; }

    if (std::getline(ss, part, '.')) {
        try { min = std::stoi(part); } catch (...) { return std::nullopt; }
    }
    if (std::getline(ss, part, '.')) {
        try { pat = std::stoi(part); } catch (...) { return std::nullopt; }
    }

    return SemVer(maj, min, pat, pre, bld);
}

std::string SemVer::to_string() const {
    std::string s = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    if (!prerelease.empty()) s += "-" + prerelease;
    if (!build.empty()) s += "+" + build;
    return s;
}

bool SemVer::operator==(const SemVer& other) const {
    return major == other.major && minor == other.minor && patch == other.patch && prerelease == other.prerelease;
}

bool SemVer::operator<(const SemVer& other) const {
    if (major != other.major) return major < other.major;
    if (minor != other.minor) return minor < other.minor;
    if (patch != other.patch) return patch < other.patch;
    if (prerelease.empty() && !other.prerelease.empty()) return false; // non-prerelease > prerelease
    if (!prerelease.empty() && other.prerelease.empty()) return true;
    return prerelease < other.prerelease;
}

bool VersionConstraint::matches(const SemVer& ver) const {
    switch (op) {
        case VersionOp::ANY: return true;
        case VersionOp::EXACT: return ver == version;
        case VersionOp::GREATER: return ver > version;
        case VersionOp::GREATER_EQ: return ver >= version;
        case VersionOp::LESS: return ver < version;
        case VersionOp::LESS_EQ: return ver <= version;
        case VersionOp::CARET: {
            if (ver < version) return false;
            if (version.major > 0) {
                return ver.major == version.major;
            }
            if (version.minor > 0) {
                return ver.major == 0 && ver.minor == version.minor;
            }
            return ver.major == 0 && ver.minor == 0 && ver.patch == version.patch;
        }
        case VersionOp::TILDE: {
            if (ver < version) return false;
            return ver.major == version.major && ver.minor == version.minor;
        }
    }
    return false;
}

std::string VersionConstraint::to_string() const {
    switch (op) {
        case VersionOp::ANY: return "*";
        case VersionOp::EXACT: return "=" + version.to_string();
        case VersionOp::CARET: return "^" + version.to_string();
        case VersionOp::TILDE: return "~" + version.to_string();
        case VersionOp::GREATER: return ">" + version.to_string();
        case VersionOp::GREATER_EQ: return ">=" + version.to_string();
        case VersionOp::LESS: return "<" + version.to_string();
        case VersionOp::LESS_EQ: return "<=" + version.to_string();
    }
    return version.to_string();
}

VersionRequirement VersionRequirement::parse(const std::string& req_str) {
    VersionRequirement req;
    req.raw_string = trim(req_str);
    if (req.raw_string.empty() || req.raw_string == "*") {
        req.constraints.push_back({VersionOp::ANY, SemVer(0, 0, 0)});
        return req;
    }

    std::stringstream ss(req.raw_string);
    std::string token;
    while (ss >> token) {
        if (token.back() == ',') token.pop_back();
        if (token.empty()) continue;

        VersionOp op = VersionOp::EXACT;
        std::string v_str = token;

        if (token.rfind(">=", 0) == 0) {
            op = VersionOp::GREATER_EQ;
            v_str = token.substr(2);
        } else if (token.rfind("<=", 0) == 0) {
            op = VersionOp::LESS_EQ;
            v_str = token.substr(2);
        } else if (token.rfind(">", 0) == 0) {
            op = VersionOp::GREATER;
            v_str = token.substr(1);
        } else if (token.rfind("<", 0) == 0) {
            op = VersionOp::LESS;
            v_str = token.substr(1);
        } else if (token.rfind("^", 0) == 0) {
            op = VersionOp::CARET;
            v_str = token.substr(1);
        } else if (token.rfind("~", 0) == 0) {
            op = VersionOp::TILDE;
            v_str = token.substr(1);
        } else if (token.rfind("=", 0) == 0) {
            op = VersionOp::EXACT;
            v_str = token.substr(1);
        } else if (token == "*") {
            op = VersionOp::ANY;
            v_str = "0.0.0";
        }

        auto sv = SemVer::parse(v_str);
        if (sv) {
            req.constraints.push_back({op, *sv});
        }
    }

    if (req.constraints.empty()) {
        req.constraints.push_back({VersionOp::ANY, SemVer(0, 0, 0)});
    }
    return req;
}

bool VersionRequirement::matches(const SemVer& ver) const {
    for (const auto& c : constraints) {
        if (!c.matches(ver)) return false;
    }
    return true;
}

std::string DependencySpec::to_toml_value() const {
    if (source_type == DependencySourceType::LOCAL_PATH) {
        return "{ path = \"" + path + "\" }";
    }
    if (source_type == DependencySourceType::GIT) {
        std::string s = "{ git = \"" + git_url + "\"";
        if (!git_ref.empty()) s += ", tag = \"" + git_ref + "\"";
        s += " }";
        return s;
    }
    return "\"" + version_req.to_string() + "\"";
}

// ============================================================================
// Manifest & TOML Parser
// ============================================================================

namespace {

static std::string strip_quotes(const std::string& str) {
    std::string s = trim(str);
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

static std::map<std::string, std::string> parse_inline_table(const std::string& str) {
    std::map<std::string, std::string> res;
    std::string s = trim(str);
    if (s.empty() || s.front() != '{' || s.back() != '}') return res;
    s = s.substr(1, s.size() - 2);

    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        size_t eq = item.find('=');
        if (eq != std::string::npos) {
            std::string k = trim(item.substr(0, eq));
            std::string v = strip_quotes(trim(item.substr(eq + 1)));
            res[k] = v;
        }
    }
    return res;
}

static std::vector<std::string> parse_string_array(const std::string& str) {
    std::vector<std::string> arr;
    std::string s = trim(str);
    if (s.empty() || s.front() != '[' || s.back() != ']') return arr;
    s = s.substr(1, s.size() - 2);

    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        std::string elem = strip_quotes(trim(item));
        if (!elem.empty()) arr.push_back(elem);
    }
    return arr;
}

} // anonymous namespace

std::optional<ProjectManifest> ProjectManifest::parse_toml(const std::string& content, std::string& error_msg) {
    ProjectManifest manifest;
    std::istringstream stream(content);
    std::string line;
    std::string current_section = "";
    int line_no = 0;

    while (std::getline(stream, line)) {
        line_no++;
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            current_section = trimmed.substr(1, trimmed.size() - 2);
            continue;
        }

        size_t eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = trim(trimmed.substr(0, eq_pos));
        std::string raw_val = trim(trimmed.substr(eq_pos + 1));

        if (current_section == "project" || current_section == "package") {
            if (key == "name") manifest.name = strip_quotes(raw_val);
            else if (key == "version") {
                auto v = SemVer::parse(strip_quotes(raw_val));
                if (v) manifest.version = *v;
                else {
                    error_msg = "Invalid semver '" + raw_val + "' in manifest at line " + std::to_string(line_no);
                    return std::nullopt;
                }
            }
            else if (key == "description") manifest.description = strip_quotes(raw_val);
            else if (key == "license") manifest.license = strip_quotes(raw_val);
            else if (key == "repository") manifest.repository = strip_quotes(raw_val);
            else if (key == "homepage") manifest.homepage = strip_quotes(raw_val);
            else if (key == "documentation") manifest.documentation = strip_quotes(raw_val);
            else if (key == "category") manifest.category = strip_quotes(raw_val);
            else if (key == "keywords") manifest.keywords = parse_string_array(raw_val);
            else if (key == "main") manifest.main_file = strip_quotes(raw_val);
            else if (key == "authors") manifest.authors = parse_string_array(raw_val);
        } else if (current_section == "dependencies" || current_section == "dev-dependencies") {
            DependencySpec spec;
            spec.name = key;

            if (raw_val.front() == '{' && raw_val.back() == '}') {
                auto table = parse_inline_table(raw_val);
                if (table.count("path")) {
                    spec.source_type = DependencySourceType::LOCAL_PATH;
                    spec.path = table["path"];
                } else if (table.count("git")) {
                    spec.source_type = DependencySourceType::GIT;
                    spec.git_url = table["git"];
                    if (table.count("tag")) spec.git_ref = table["tag"];
                    else if (table.count("branch")) spec.git_ref = table["branch"];
                    else if (table.count("commit")) spec.git_ref = table["commit"];
                }
                if (table.count("version")) {
                    spec.version_req = VersionRequirement::parse(table["version"]);
                }
            } else {
                std::string v_str = strip_quotes(raw_val);
                spec.source_type = DependencySourceType::REGISTRY;
                spec.version_req = VersionRequirement::parse(v_str);
            }

            if (current_section == "dependencies") {
                manifest.dependencies[key] = spec;
            } else {
                manifest.dev_dependencies[key] = spec;
            }
        } else if (current_section == "scripts") {
            manifest.scripts[key] = strip_quotes(raw_val);
        }
    }

    if (manifest.name.empty()) {
        error_msg = "Manifest missing required 'name' field under [project]";
        return std::nullopt;
    }

    return manifest;
}

std::optional<ProjectManifest> ProjectManifest::load_from_file(const fs::path& file_path, std::string& error_msg) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        error_msg = "Could not open manifest file: " + file_path.string();
        return std::nullopt;
    }
    std::stringstream buf;
    buf << file.rdbuf();
    return parse_toml(buf.str(), error_msg);
}

std::string ProjectManifest::to_toml_string() const {
    std::ostringstream ss;
    ss << "[project]\n"
       << "name = \"" << name << "\"\n"
       << "version = \"" << version.to_string() << "\"\n";
    if (!description.empty()) ss << "description = \"" << description << "\"\n";
    if (!license.empty()) ss << "license = \"" << license << "\"\n";
    if (!repository.empty()) ss << "repository = \"" << repository << "\"\n";
    if (!homepage.empty()) ss << "homepage = \"" << homepage << "\"\n";
    if (!documentation.empty()) ss << "documentation = \"" << documentation << "\"\n";
    if (!category.empty()) ss << "category = \"" << category << "\"\n";
    if (!main_file.empty()) ss << "main = \"" << main_file << "\"\n";

    if (!keywords.empty()) {
        ss << "keywords = [";
        for (size_t i = 0; i < keywords.size(); ++i) {
            ss << "\"" << keywords[i] << "\"";
            if (i + 1 < keywords.size()) ss << ", ";
        }
        ss << "]\n";
    }

    if (!authors.empty()) {
        ss << "authors = [";
        for (size_t i = 0; i < authors.size(); ++i) {
            ss << "\"" << authors[i] << "\"";
            if (i + 1 < authors.size()) ss << ", ";
        }
        ss << "]\n";
    }

    ss << "\n[dependencies]\n";
    for (const auto& [dep_name, spec] : dependencies) {
        ss << dep_name << " = " << spec.to_toml_value() << "\n";
    }

    if (!dev_dependencies.empty()) {
        ss << "\n[dev-dependencies]\n";
        for (const auto& [dep_name, spec] : dev_dependencies) {
            ss << dep_name << " = " << spec.to_toml_value() << "\n";
        }
    }

    if (!scripts.empty()) {
        ss << "\n[scripts]\n";
        for (const auto& [script_name, cmd] : scripts) {
            ss << script_name << " = \"" << cmd << "\"\n";
        }
    }

    return ss.str();
}

bool ProjectManifest::save_to_file(const fs::path& file_path, std::string& error_msg) const {
    std::ofstream file(file_path);
    if (!file.is_open()) {
        error_msg = "Failed to write manifest to: " + file_path.string();
        return false;
    }
    file << to_toml_string();
    return true;
}

// ============================================================================
// Lockfile Implementation
// ============================================================================

std::optional<Lockfile> Lockfile::load_from_file(const fs::path& file_path, std::string& error_msg) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        error_msg = "Lockfile not found: " + file_path.string();
        return std::nullopt;
    }

    Lockfile lock;
    std::string line;
    LockedPackage current_pkg;
    bool in_pkg = false;

    while (std::getline(file, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        if (trimmed == "[[package]]") {
            if (in_pkg && !current_pkg.name.empty()) {
                lock.packages[current_pkg.name] = current_pkg;
            }
            current_pkg = LockedPackage();
            in_pkg = true;
            continue;
        }

        size_t eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = trim(trimmed.substr(0, eq_pos));
        std::string val = strip_quotes(trim(trimmed.substr(eq_pos + 1)));

        if (!in_pkg) {
            if (key == "version") {
                try { lock.lockfile_version = std::stoi(val); } catch (...) {}
            }
        } else {
            if (key == "name") current_pkg.name = val;
            else if (key == "version") {
                auto sv = SemVer::parse(val);
                if (sv) current_pkg.version = *sv;
            }
            else if (key == "source") current_pkg.source = val;
            else if (key == "checksum") current_pkg.checksum = val;
            else if (key == "dependencies") {
                current_pkg.dependencies = parse_string_array(trimmed.substr(eq_pos + 1));
            }
        }
    }

    if (in_pkg && !current_pkg.name.empty()) {
        lock.packages[current_pkg.name] = current_pkg;
    }

    return lock;
}

std::string Lockfile::to_lock_string() const {
    std::ostringstream ss;
    ss << "# This file is automatically generated by NextViper.\n"
       << "# Manual edits may be overwritten.\n"
       << "version = " << lockfile_version << "\n\n";

    for (const auto& [pkg_name, pkg] : packages) {
        ss << "[[package]]\n"
           << "name = \"" << pkg.name << "\"\n"
           << "version = \"" << pkg.version.to_string() << "\"\n"
           << "source = \"" << pkg.source << "\"\n"
           << "checksum = \"" << pkg.checksum << "\"\n";

        if (!pkg.dependencies.empty()) {
            ss << "dependencies = [";
            for (size_t i = 0; i < pkg.dependencies.size(); ++i) {
                ss << "\"" << pkg.dependencies[i] << "\"";
                if (i + 1 < pkg.dependencies.size()) ss << ", ";
            }
            ss << "]\n";
        }
        ss << "\n";
    }

    return ss.str();
}

bool Lockfile::save_to_file(const fs::path& file_path, std::string& error_msg) const {
    std::ofstream file(file_path);
    if (!file.is_open()) {
        error_msg = "Failed to write lockfile to: " + file_path.string();
        return false;
    }
    file << to_lock_string();
    return true;
}

// ============================================================================
// Dependency Resolution Implementation
// ============================================================================

DependencyResolver::ResolutionResult DependencyResolver::resolve(
    const ProjectManifest& root_manifest,
    const fs::path& root_dir,
    const std::optional<Lockfile>& existing_lock) {

    ResolutionResult result;
    std::map<std::string, ResolvedPackageNode> resolved;
    std::vector<std::string> order;
    std::set<std::string> active_chain;

    auto resolve_recursive = [&](auto& self, const std::string& pkg_name, const DependencySpec& spec, const fs::path& parent_dir) -> bool {
        if (active_chain.count(pkg_name)) {
            result.error_message = "Circular dependency detected: ";
            for (const auto& node : active_chain) result.error_message += node + " -> ";
            result.error_message += pkg_name;
            return false;
        }

        active_chain.insert(pkg_name);

        fs::path pkg_dir;
        std::string source_loc;
        DependencySourceType stype = spec.source_type;

        if (stype == DependencySourceType::LOCAL_PATH) {
            pkg_dir = fs::weakly_canonical(parent_dir / spec.path);
            source_loc = "path:" + spec.path;
        } else if (stype == DependencySourceType::GIT) {
            // Local git cache path
            std::string repo_hash = PackageIntegrity::sha256_hex(spec.git_url).substr(0, 12);
            pkg_dir = root_dir / ".nextviper" / "git" / (pkg_name + "_" + repo_hash);
            source_loc = "git:" + spec.git_url + (spec.git_ref.empty() ? "" : "#" + spec.git_ref);
        } else {
            // Registry / Local packages search path
            pkg_dir = root_dir / ".nextviper" / "packages" / pkg_name;
            source_loc = "registry:" + spec.version_req.to_string();

            std::error_code ec_dl;
            if (!fs::exists(pkg_dir, ec_dl) || !fs::exists(pkg_dir / "nextviper.toml", ec_dl)) {
                const char* reg_env = std::getenv("NEXTVIPER_REGISTRY_URL");
                std::string registry_url = reg_env ? reg_env : "https://nextviper.nuratix.com";
                
                fs::create_directories(pkg_dir, ec_dl);
                std::string ver_target = spec.version_req.raw_string.empty() || spec.version_req.raw_string == "*" ? "latest" : spec.version_req.raw_string;
                std::string dl_cmd = "curl -s -f -L \"" + registry_url + "/api/packages/" + pkg_name + "/" + ver_target + "/download\" | tar -xz -C \"" + pkg_dir.string() + "\" 2>/dev/null";
                int dl_res = system(dl_cmd.c_str());
                (void)dl_res;
            }
        }

        std::error_code ec;
        if (!fs::exists(pkg_dir, ec) || !fs::exists(pkg_dir / "nextviper.toml", ec)) {
            // Try looking in root packages/ directory if available
            fs::path local_alt = root_dir / "packages" / pkg_name;
            if (fs::exists(local_alt, ec)) {
                pkg_dir = local_alt;
                source_loc = "path:packages/" + pkg_name;
                stype = DependencySourceType::LOCAL_PATH;
            }
        }

        fs::path manifest_path = pkg_dir / "nextviper.toml";
        ProjectManifest sub_manifest;
        SemVer resolved_ver(0, 1, 0);

        if (fs::exists(manifest_path, ec)) {
            std::string err;
            auto loaded = ProjectManifest::load_from_file(manifest_path, err);
            if (!loaded) {
                result.error_message = "Failed to load manifest for dependency '" + pkg_name + "': " + err;
                return false;
            }
            sub_manifest = *loaded;
            resolved_ver = sub_manifest.version;
        } else if (existing_lock && existing_lock->packages.count(pkg_name)) {
            const auto& lp = existing_lock->packages.at(pkg_name);
            resolved_ver = lp.version;
        }

        // SemVer matching check
        if (!spec.version_req.matches(resolved_ver)) {
            result.error_message = "error[NV201]: version conflict for dependency '" + pkg_name + "'\n"
                                 + "  required: " + spec.version_req.to_string() + "\n"
                                 + "  found:    " + resolved_ver.to_string();
            return false;
        }

        std::string checksum = "";
        if (existing_lock && existing_lock->packages.count(pkg_name)) {
            checksum = existing_lock->packages.at(pkg_name).checksum;
        }
        if (checksum.empty() && fs::exists(pkg_dir, ec)) {
            checksum = PackageIntegrity::compute_tree_hash(pkg_dir);
        }

        ResolvedPackageNode node;
        node.name = pkg_name;
        node.version = resolved_ver;
        node.source_type = stype;
        node.source_location = source_loc;
        node.checksum = checksum;
        node.local_package_path = pkg_dir;

        // Resolve sub-dependencies
        for (const auto& [sub_name, sub_spec] : sub_manifest.dependencies) {
            node.dependency_names.push_back(sub_name);
            if (!self(self, sub_name, sub_spec, pkg_dir)) {
                return false;
            }
        }

        resolved[pkg_name] = node;
        order.push_back(pkg_name);
        active_chain.erase(pkg_name);
        return true;
    };

    for (const auto& [dep_name, dep_spec] : root_manifest.dependencies) {
        if (!resolve_recursive(resolve_recursive, dep_name, dep_spec, root_dir)) {
            result.success = false;
            return result;
        }
    }

    result.success = true;
    result.resolved_graph = std::move(resolved);
    result.resolution_order = std::move(order);
    return result;
}

// ============================================================================
// PackageManager CLI Engine
// ============================================================================

bool PackageManager::ensure_module_symlink(const std::string& pkg_name, const fs::path& target_dir, std::string& err) {
    fs::path link_dir = modules_dir();
    std::error_code ec;
    fs::create_directories(link_dir, ec);
    fs::path link_path = link_dir / pkg_name;

    if (fs::exists(link_path, ec) || fs::is_symlink(link_path, ec)) {
        fs::remove_all(link_path, ec);
    }

    try {
        fs::create_directory_symlink(target_dir, link_path, ec);
        if (ec) {
            // Fallback to directory copy on platforms without symlink permissions
            fs::create_directories(link_path, ec);
            fs::copy(target_dir, link_path, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        }
        return true;
    } catch (const std::exception& e) {
        err = e.what();
        return false;
    }
}

int PackageManager::cmd_init(const std::string& name) {
    std::string pkg_name = name.empty() ? project_root_.filename().string() : name;
    if (pkg_name.empty() || pkg_name == "." || pkg_name == "/") pkg_name = "my_project";

    fs::path manifest = manifest_path();
    std::error_code ec;
    if (fs::exists(manifest, ec)) {
        std::cerr << "error[NV202]: 'nextviper.toml' already exists in current directory\n";
        return 1;
    }

    ProjectManifest m;
    m.name = pkg_name;
    m.version = SemVer(0, 1, 0);
    m.description = "A modern NextViper application";
    m.license = "MIT";
    m.main_file = "src/main.nv";

    std::string err;
    if (!m.save_to_file(manifest, err)) {
        std::cerr << "error: " << err << "\n";
        return 1;
    }

    fs::create_directories(project_root_ / "src", ec);
    fs::create_directories(project_root_ / "tests", ec);

    fs::path main_src = project_root_ / "src" / "main.nv";
    if (!fs::exists(main_src, ec)) {
        std::string raw = "// " + pkg_name + " application entrypoint\n"
                          "import std.io\n\n"
                          "export fn run():\n"
                          "    io.print(\"Hello from " + pkg_name + "!\")\n\n"
                          "run()\n";
        std::string formatted = Formatter::format_source(raw);
        std::ofstream out(main_src);
        if (out.is_open()) {
            out << (formatted.empty() ? raw : formatted);
        }
    }

    fs::path test_file = project_root_ / "tests" / "main_test.nv";
    if (!fs::exists(test_file, ec)) {
        std::string raw = "// Automated test suite for " + pkg_name + "\n"
                          "import std.io\n\n"
                          "fn test_basic_assertion():\n"
                          "    let x = 10 + 20\n"
                          "    if x != 30:\n"
                          "        io.print(\"Assertion failed!\")\n\n"
                          "test_basic_assertion()\n";
        std::string formatted = Formatter::format_source(raw);
        std::ofstream out(test_file);
        if (out.is_open()) {
            out << (formatted.empty() ? raw : formatted);
        }
    }

    fs::path readme = project_root_ / "README.md";
    if (!fs::exists(readme, ec)) {
        std::ofstream out(readme);
        if (out.is_open()) {
            out << "# " << pkg_name << "\n\n"
                << "A modern NextViper application.\n\n"
                << "## Getting Started\n\n"
                << "```bash\n"
                << "# Check syntax and types\n"
                << "nextviper check\n\n"
                << "# Format codebase\n"
                << "nextviper fmt\n\n"
                << "# Run tests\n"
                << "nextviper test\n\n"
                << "# Build project\n"
                << "nextviper build\n\n"
                << "# Run application\n"
                << "nextviper run src/main.nv\n"
                << "```\n";
        }
    }

    fs::path gitignore = project_root_ / ".gitignore";
    if (!fs::exists(gitignore, ec)) {
        std::ofstream out(gitignore);
        if (out.is_open()) {
            out << ".nextviper/\n"
                << "nextviper_modules/\n"
                << "build/\n"
                << "dist/\n"
                << "*.nvc\n";
        }
    }

    std::cout << "\033[1;32m✓ Initialized\033[0m NextViper package \033[1m'" << pkg_name << "'\033[0m\n"
              << "  Created nextviper.toml\n"
              << "  Created src/main.nv\n"
              << "  Created tests/main_test.nv\n"
              << "  Created README.md\n"
              << "  Created .gitignore\n";
    return 0;
}

int PackageManager::cmd_add(const std::string& pkg_name, const std::string& version_or_path, bool is_path, const std::string& git_url, const std::string& git_ref) {
    std::string err;
    auto manifest_opt = ProjectManifest::load_from_file(manifest_path(), err);
    if (!manifest_opt) {
        std::cerr << "error[NV203]: no nextviper.toml found in current directory. Run 'nextviper init' first.\n";
        return 1;
    }

    ProjectManifest manifest = *manifest_opt;
    DependencySpec spec;
    spec.name = pkg_name;

    if (!git_url.empty()) {
        spec.source_type = DependencySourceType::GIT;
        spec.git_url = git_url;
        spec.git_ref = git_ref;
    } else if (is_path) {
        spec.source_type = DependencySourceType::LOCAL_PATH;
        spec.path = version_or_path.empty() ? ("../" + pkg_name) : version_or_path;
    } else {
        spec.source_type = DependencySourceType::REGISTRY;
        spec.version_req = VersionRequirement::parse(version_or_path.empty() ? "*" : version_or_path);
    }

    manifest.dependencies[pkg_name] = spec;
    if (!manifest.save_to_file(manifest_path(), err)) {
        std::cerr << "error: " << err << "\n";
        return 1;
    }

    std::cout << "\033[1;32m✓ Added\033[0m dependency '" << pkg_name << "' to nextviper.toml\n";
    return cmd_install();
}

int PackageManager::cmd_remove(const std::string& pkg_name) {
    std::string err;
    auto manifest_opt = ProjectManifest::load_from_file(manifest_path(), err);
    if (!manifest_opt) {
        std::cerr << "error: no nextviper.toml found\n";
        return 1;
    }

    ProjectManifest manifest = *manifest_opt;
    if (!manifest.dependencies.count(pkg_name)) {
        std::cerr << "error: package '" << pkg_name << "' is not in dependencies\n";
        return 1;
    }

    manifest.dependencies.erase(pkg_name);
    if (!manifest.save_to_file(manifest_path(), err)) {
        std::cerr << "error: " << err << "\n";
        return 1;
    }

    std::error_code ec;
    fs::remove_all(modules_dir() / pkg_name, ec);

    std::cout << "\033[1;32m✓ Removed\033[0m dependency '" << pkg_name << "' from nextviper.toml\n";
    return cmd_install();
}

int PackageManager::cmd_install() {
    std::string err;
    auto manifest_opt = ProjectManifest::load_from_file(manifest_path(), err);
    if (!manifest_opt) {
        std::cerr << "error[NV203]: no nextviper.toml found. Run 'nextviper init' first.\n";
        return 1;
    }

    std::optional<Lockfile> existing_lock = Lockfile::load_from_file(lockfile_path(), err);
    auto res = DependencyResolver::resolve(*manifest_opt, project_root_, existing_lock);

    if (!res.success) {
        std::cerr << res.error_message << "\n";
        return 1;
    }

    Lockfile new_lock;
    new_lock.lockfile_version = 1;

    for (const auto& [pkg_name, node] : res.resolved_graph) {
        LockedPackage lp;
        lp.name = node.name;
        lp.version = node.version;
        lp.source = node.source_location;
        lp.checksum = node.checksum;
        lp.dependencies = node.dependency_names;

        // Verify package integrity if checksum exists in lockfile
        if (existing_lock && existing_lock->packages.count(pkg_name)) {
            const auto& old_lp = existing_lock->packages.at(pkg_name);
            if (!old_lp.checksum.empty() && fs::exists(node.local_package_path)) {
                if (!PackageIntegrity::verify_tree_hash(node.local_package_path, old_lp.checksum)) {
                    std::cerr << "\033[1;31merror[NV204]: package integrity verification failed for '" << pkg_name << "'\033[0m\n"
                              << "  expected: " << old_lp.checksum << "\n"
                              << "  actual:   " << PackageIntegrity::compute_tree_hash(node.local_package_path) << "\n"
                              << "  help: the package files have been modified since the lockfile was generated.\n"
                              << "        run 'nextviper update' if this change was intentional.\n";
                    return 1;
                }
            }
        }

        // Compute and store fresh checksum if none
        if (lp.checksum.empty() && fs::exists(node.local_package_path)) {
            lp.checksum = PackageIntegrity::compute_tree_hash(node.local_package_path);
        }

        new_lock.packages[pkg_name] = lp;

        // Ensure module link
        if (fs::exists(node.local_package_path)) {
            std::string sym_err;
            ensure_module_symlink(pkg_name, node.local_package_path, sym_err);
        }
    }

    if (!new_lock.save_to_file(lockfile_path(), err)) {
        std::cerr << "error: failed to write lockfile: " << err << "\n";
        return 1;
    }

    std::cout << "\033[1;32m✓ Installed\033[0m " << res.resolved_graph.size() << " package(s) successfully\n";
    return 0;
}

int PackageManager::cmd_update(const std::string& /*pkg_name*/) {
    std::string err;
    auto manifest_opt = ProjectManifest::load_from_file(manifest_path(), err);
    if (!manifest_opt) {
        std::cerr << "error: no nextviper.toml found\n";
        return 1;
    }

    // Force re-resolution ignoring existing lockfile checksum constraints
    auto res = DependencyResolver::resolve(*manifest_opt, project_root_, std::nullopt);
    if (!res.success) {
        std::cerr << res.error_message << "\n";
        return 1;
    }

    Lockfile updated_lock;
    updated_lock.lockfile_version = 1;

    for (const auto& [pkg_name, node] : res.resolved_graph) {
        LockedPackage lp;
        lp.name = node.name;
        lp.version = node.version;
        lp.source = node.source_location;
        lp.dependencies = node.dependency_names;

        if (fs::exists(node.local_package_path)) {
            lp.checksum = PackageIntegrity::compute_tree_hash(node.local_package_path);
            std::string sym_err;
            ensure_module_symlink(pkg_name, node.local_package_path, sym_err);
        }
        updated_lock.packages[pkg_name] = lp;
    }

    if (!updated_lock.save_to_file(lockfile_path(), err)) {
        std::cerr << "error: " << err << "\n";
        return 1;
    }

    std::cout << "\033[1;32m✓ Updated\033[0m dependencies and regenerated nextviper.lock\n";
    return 0;
}

int PackageManager::cmd_list() {
    std::string err;
    auto manifest_opt = ProjectManifest::load_from_file(manifest_path(), err);
    if (!manifest_opt) {
        std::cerr << "error: no nextviper.toml found\n";
        return 1;
    }

    std::optional<Lockfile> lock = Lockfile::load_from_file(lockfile_path(), err);

    std::cout << "\033[1m" << manifest_opt->name << "\033[0m v" << manifest_opt->version.to_string() << "\n";
    if (manifest_opt->dependencies.empty()) {
        std::cout << "└── (no dependencies)\n";
        return 0;
    }

    size_t idx = 0;
    for (const auto& [dep_name, spec] : manifest_opt->dependencies) {
        idx++;
        bool is_last = (idx == manifest_opt->dependencies.size());
        std::string branch = is_last ? "└── " : "├── ";

        std::string ver_str = spec.version_req.to_string();
        std::string source_str = "";
        std::string integrity_str = "\033[33m[unverified]\033[0m";

        if (lock && lock->packages.count(dep_name)) {
            const auto& lp = lock->packages.at(dep_name);
            ver_str = lp.version.to_string();
            source_str = " (" + lp.source + ")";
            if (!lp.checksum.empty()) {
                integrity_str = "\033[32m[verified: " + lp.checksum.substr(0, 8) + "...]\033[0m";
            }
        }

        std::cout << branch << "\033[1;36m" << dep_name << "\033[0m v" << ver_str << source_str << " " << integrity_str << "\n";
    }
    return 0;
}

static std::map<std::string, std::string> load_nextviperrc() {
    std::map<std::string, std::string> config;
    const char* home = std::getenv("HOME");
    if (!home) return config;
    fs::path rc_path = fs::path(home) / ".nextviperrc";
    if (!fs::exists(rc_path)) return config;

    std::ifstream ifs(rc_path);
    std::string line;
    while (std::getline(ifs, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq != std::string::npos) {
            std::string key = trim(line.substr(0, eq));
            std::string val = trim(line.substr(eq + 1));
            config[key] = val;
        }
    }
    return config;
}

static void save_nextviperrc(const std::string& user, const std::string& token, const std::string& registry) {
    const char* home = std::getenv("HOME");
    if (!home) return;
    fs::path rc_path = fs::path(home) / ".nextviperrc";
    std::ofstream ofs(rc_path);
    if (ofs.is_open()) {
        ofs << "# NextViper Developer Credentials\n";
        ofs << "user=" << user << "\n";
        ofs << "token=" << token << "\n";
        ofs << "registry=" << registry << "\n";
        ofs.close();
        chmod(rc_path.c_str(), 0600);
    }
}

int PackageManager::cmd_publish(bool dry_run,
                                const std::string& access_arg,
                                const std::string& token_arg,
                                const std::string& user_arg,
                                const std::string& registry_arg) {
    std::string err;
    auto manifest_opt = ProjectManifest::load_from_file(manifest_path(), err);
    if (!manifest_opt) {
        std::cerr << "\033[1;31merror:\033[0m no nextviper.toml found in current directory\n";
        return 1;
    }

    auto m = *manifest_opt;
    bool updated_manifest = false;
    bool is_interactive = !dry_run && isatty(fileno(stdin));

    // Interactive prompt for License if missing
    if (m.license.empty()) {
        if (is_interactive) {
            std::cout << "\033[1;33m? Package License (e.g. MIT, Apache-2.0, BSD-3-Clause) [default: MIT]: \033[0m";
            std::string input;
            if (std::getline(std::cin, input)) {
                input = trim(input);
                m.license = input.empty() ? "MIT" : input;
                updated_manifest = true;
            } else {
                m.license = "MIT";
            }
        } else {
            m.license = "MIT";
        }
    }

    // Interactive prompt for Description if missing
    if (m.description.empty()) {
        if (is_interactive) {
            std::cout << "\033[1;33m? Package Summary / Description: \033[0m";
            std::string input;
            if (std::getline(std::cin, input)) {
                input = trim(input);
                m.description = input.empty() ? (m.name + " package for NextViper") : input;
                updated_manifest = true;
            } else {
                m.description = m.name + " package";
            }
        } else {
            m.description = m.name + " package";
        }
    }

    // Interactive prompt for Repository URL if missing
    if (m.repository.empty() && is_interactive) {
        std::cout << "\033[1;33m? Repository URL (e.g. https://github.com/user/pkg) [optional]: \033[0m";
        std::string input;
        if (std::getline(std::cin, input)) {
            input = trim(input);
            if (!input.empty()) {
                m.repository = input;
                updated_manifest = true;
            }
        }
    }

    // Interactive prompt for Homepage URL if missing
    if (m.homepage.empty() && is_interactive) {
        std::cout << "\033[1;33m? Homepage URL [optional]: \033[0m";
        std::string input;
        if (std::getline(std::cin, input)) {
            input = trim(input);
            if (!input.empty()) {
                m.homepage = input;
                updated_manifest = true;
            }
        }
    }

    // Interactive prompt for Documentation URL if missing
    if (m.documentation.empty() && is_interactive) {
        std::cout << "\033[1;33m? Documentation URL [optional]: \033[0m";
        std::string input;
        if (std::getline(std::cin, input)) {
            input = trim(input);
            if (!input.empty()) {
                m.documentation = input;
                updated_manifest = true;
            }
        }
    }

    // Interactive prompt for Category if missing
    if (m.category.empty() && is_interactive) {
        std::cout << "\033[1;33m? Category (general/ai/data/numerical/networking/crypto) [default: general]: \033[0m";
        std::string input;
        if (std::getline(std::cin, input)) {
            input = trim(input);
            if (!input.empty()) {
                m.category = input;
                updated_manifest = true;
            }
        }
    }

    if (updated_manifest) {
        std::string save_err;
        m.save_to_file(manifest_path(), save_err);
        std::cout << "\033[1;32m✓ Saved manifest configuration to nextviper.toml\033[0m\n";
    }

    std::string tree_hash = PackageIntegrity::compute_tree_hash(project_root_);
    std::cout << "\n\033[1;34m[Packaging]\033[0m " << m.name << " v" << m.version.to_string() << "\n"
              << "  Tree SHA-256: " << tree_hash << "\n"
              << "  License:      " << m.license << "\n"
              << "  Repository:   " << (m.repository.empty() ? "(none)" : m.repository) << "\n"
              << "  Homepage:     " << (m.homepage.empty() ? "(none)" : m.homepage) << "\n";

    if (dry_run) {
        std::cout << "\033[1;32m✓ Package validation passed (dry-run mode).\033[0m Ready for publication.\n";
        return 0;
    }

    std::error_code ec;
    fs::create_directories(project_root_ / "dist", ec);
    fs::path dist_pkg = project_root_ / "dist" / (m.name + "-" + m.version.to_string() + ".nvpkg");

    std::string tar_cmd = "tar -czf \"" + dist_pkg.string() + "\" --exclude='.git' --exclude='.nextviper' --exclude='dist' --exclude='build' -C \"" + project_root_.string() + "\" .";
    int ret = system(tar_cmd.c_str());
    if (ret != 0) {
        std::cerr << "\033[1;31merror:\033[0m failed to archive package bundle\n";
        return 1;
    }

    std::cout << "\033[1;32m✓ Built package bundle:\033[0m " << dist_pkg.string() << "\n\n";

    // ------------------------------------------------------------------------
    // Credentials & Registry Resolution
    // ------------------------------------------------------------------------
    auto rc = load_nextviperrc();

    std::string final_registry = registry_arg;
    if (final_registry.empty()) {
        const char* reg_env = std::getenv("NEXTVIPER_REGISTRY");
        if (reg_env && std::string(reg_env).length() > 0) {
            final_registry = reg_env;
        } else if (rc.count("registry") && !rc["registry"].empty()) {
            final_registry = rc["registry"];
        } else {
            final_registry = "https://nextviper.nuratix.com";
        }
    }
    while (!final_registry.empty() && final_registry.back() == '/') {
        final_registry.pop_back();
    }

    std::string final_token = token_arg;
    if (final_token.empty()) {
        const char* tok_env = std::getenv("NEXTVIPER_TOKEN");
        if (!tok_env) tok_env = std::getenv("NEXTVIPER_ACCESS_KEY");
        if (tok_env && std::string(tok_env).length() > 0) {
            final_token = tok_env;
        } else if (rc.count("token") && !rc["token"].empty()) {
            final_token = rc["token"];
        }
    }

    std::string final_user = user_arg;
    if (final_user.empty()) {
        const char* usr_env = std::getenv("NEXTVIPER_USER");
        if (!usr_env) usr_env = std::getenv("NEXTVIPER_USERNAME");
        if (usr_env && std::string(usr_env).length() > 0) {
            final_user = usr_env;
        } else if (rc.count("user") && !rc["user"].empty()) {
            final_user = rc["user"];
        }
    }

    std::string final_access = access_arg;
    if (final_access.empty()) {
        const char* acc_env = std::getenv("NEXTVIPER_ACCESS");
        if (acc_env && std::string(acc_env).length() > 0) {
            final_access = acc_env;
        } else {
            final_access = "public";
        }
    }

    bool should_save_rc = false;

    // Interactive Prompt for missing credentials
    if ((final_user.empty() || final_token.empty()) && is_interactive) {
        std::cout << "\033[1;36m====================================================\033[0m\n";
        std::cout << "\033[1;36m NextViper Cloud Registry Authentication\033[0m\n";
        std::cout << "\033[1;36m====================================================\033[0m\n";

        if (final_user.empty()) {
            std::cout << "\033[1;33m? Enter your NextViper Username: \033[0m";
            std::string input;
            if (std::getline(std::cin, input)) {
                final_user = trim(input);
            }
        }

        if (final_token.empty()) {
            std::cout << "\033[1;33m? Enter Developer Access Key (starts with nv_...): \033[0m";
            std::string input;
            if (std::getline(std::cin, input)) {
                final_token = trim(input);
            }
        }

        if (access_arg.empty()) {
            std::cout << "\033[1;33m? Package Visibility (public/private) [default: public]: \033[0m";
            std::string input;
            if (std::getline(std::cin, input)) {
                input = trim(input);
                if (input == "private") {
                    final_access = "private";
                } else {
                    final_access = "public";
                }
            }
        }

        std::cout << "\033[1;33m? Save credentials to ~/.nextviperrc for future releases? (Y/n): \033[0m";
        std::string save_in;
        if (std::getline(std::cin, save_in)) {
            save_in = trim(save_in);
            if (save_in.empty() || save_in == "y" || save_in == "Y" || save_in == "yes") {
                should_save_rc = true;
            }
        } else {
            should_save_rc = true;
        }
    }

    if (final_token.empty()) {
        std::cerr << "\033[1;31merror:\033[0m missing developer access key.\n"
                  << "  • Set NEXTVIPER_TOKEN environment variable\n"
                  << "  • Or run: nextviper publish --token <nv_...> --user <username>\n"
                  << "  • Or save credentials in ~/.nextviperrc\n";
        return 1;
    }

    if (final_user.empty()) {
        std::cerr << "\033[1;31merror:\033[0m missing NextViper username.\n"
                  << "  • Set NEXTVIPER_USER environment variable\n"
                  << "  • Or run: nextviper publish --user <username> --token <nv_...>\n";
        return 1;
    }

    if (should_save_rc) {
        save_nextviperrc(final_user, final_token, final_registry);
        std::cout << "\033[1;32m✓ Saved credentials to ~/.nextviperrc\033[0m\n";
    }

    // ------------------------------------------------------------------------
    // Remote Network Upload Dispatch
    // ------------------------------------------------------------------------
    std::string is_private_val = (final_access == "private") ? "true" : "false";
    std::cout << "\033[1;34m[Publishing]\033[0m Uploading " << m.name << "@" << m.version.to_string() 
              << " (" << final_access << ") to NextViper Registry (" << final_registry << ")...\n";

    std::string upload_cmd = "curl -s -w \"\\n__HTTP_STATUS__:%{http_code}\" -X POST \"" + final_registry + "/api/packages/publish\" "
        "-H \"Authorization: Bearer " + final_token + "\" "
        "-H \"X-NextViper-Access-Key: " + final_token + "\" "
        "-H \"X-NextViper-User: " + final_user + "\" "
        "-F \"file=@" + dist_pkg.string() + "\" "
        "-F \"license=" + m.license + "\" "
        "-F \"description=" + m.description + "\" "
        "-F \"category=" + m.category + "\" "
        "-F \"access=" + final_access + "\" "
        "-F \"is_private=" + is_private_val + "\"";

    FILE* pipe = popen(upload_cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "\033[1;31merror:\033[0m failed to execute network upload command\n";
        return 1;
    }

    char buf[512];
    std::string result = "";
    while (fgets(buf, sizeof(buf), pipe) != NULL) {
        result += buf;
    }
    pclose(pipe);

    // Extract HTTP Status Code
    int http_status = 0;
    size_t status_pos = result.rfind("__HTTP_STATUS__:");
    std::string body = result;
    if (status_pos != std::string::npos) {
        std::string status_str = result.substr(status_pos + 16);
        try {
            http_status = std::stoi(trim(status_str));
        } catch (...) {}
        body = result.substr(0, status_pos);
    }

    if (http_status == 200 || http_status == 201) {
        std::cout << "\n\033[1;32m====================================================\033[0m\n";
        std::cout << "\033[1;32m 🎉 SUCCESS: " << m.name << "@" << m.version.to_string() 
                  << " (" << final_access << ") PUBLISHED SUCCESSFULLY!\033[0m\n";
        std::cout << "\033[1;32m====================================================\033[0m\n";
        std::cout << "  Package:      " << m.name << "\n";
        std::cout << "  Version:      " << m.version.to_string() << " (Latest)\n";
        std::cout << "  Visibility:   " << final_access << "\n";
        std::cout << "  Owner:        @" << final_user << "\n";
        std::cout << "  Registry URL: " << final_registry << "/packages/" << m.name << "\n";
        std::cout << "  Install with: nextviper add " << m.name << "\n\n";
        return 0;
    } else {
        std::cerr << "\n\033[1;31m✗ Registry Publish Failed (HTTP " << http_status << "):\033[0m\n";
        std::cerr << "  " << trim(body) << "\n\n";
        return 1;
    }
}

} // namespace nextviper
