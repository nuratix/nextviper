#include "nextviper/module.hpp"
#include "nextviper/interpreter.hpp"
#include "nextviper/lexer.hpp"
#include "nextviper/parser.hpp"
#include "nextviper/tensor.hpp"
#include "nextviper/dataset.hpp"
#include "nextviper/data_subsystem.hpp"
#include "nextviper/ai_model.hpp"
#include "nextviper/ai_subsystem.hpp"
#include "nextviper/version.hpp"
#include <cmath>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <numeric>
#include <regex>
#include <random>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <future>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <iomanip>

#if defined(_WIN32)
#include <windows.h>
#include <process.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define getpid _getpid
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#endif

#include <libpq-fe.h>

namespace nextviper {

namespace fs = std::filesystem;

// ============================================================================
// Internal Helpers: Crypto (SHA-256, MD5, Base64)
// ============================================================================

namespace {

// Base64 Encoding / Decoding
static const std::string BASE64_CHARS = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::string base64_encode(const std::string& in) {
    std::string out;
    int val = 0, valb = -6;
    for (uint8_t c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(BASE64_CHARS[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(BASE64_CHARS[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

std::string base64_decode(const std::string& in) {
    std::string out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[static_cast<uint8_t>(BASE64_CHARS[i])] = i;

    int val = 0, valb = -8;
    for (uint8_t c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

// SHA-256 Implementation (FIPS 180-2)
class SHA256 {
public:
    SHA256() { reset(); }

    void update(const uint8_t* data, size_t length) {
        for (size_t i = 0; i < length; ++i) {
            data_[block_len_++] = data[i];
            if (block_len_ == 64) {
                transform();
                bitlen_ += 512;
                block_len_ = 0;
            }
        }
    }

    void update(const std::string& data) {
        update(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    }

    std::string hexdigest() {
        pad();
        revert();
        std::ostringstream ss;
        for (int i = 0; i < 8; ++i) {
            ss << std::hex << std::setw(8) << std::setfill('0') << state_[i];
        }
        return ss.str();
    }

    std::string raw_digest() {
        pad();
        revert();
        std::string out;
        out.resize(32);
        for (int i = 0; i < 8; ++i) {
            out[i * 4 + 0] = static_cast<char>((state_[i] >> 24) & 0xFF);
            out[i * 4 + 1] = static_cast<char>((state_[i] >> 16) & 0xFF);
            out[i * 4 + 2] = static_cast<char>((state_[i] >> 8) & 0xFF);
            out[i * 4 + 3] = static_cast<char>((state_[i] >> 0) & 0xFF);
        }
        return out;
    }

private:
    uint32_t state_[8];
    uint64_t bitlen_ = 0;
    uint8_t data_[64];
    size_t block_len_ = 0;

    static uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
    static uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
    static uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
    static uint32_t sig0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
    static uint32_t sig1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
    static uint32_t gam0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
    static uint32_t gam1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

    void transform() {
        static const uint32_t K[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
        };

        uint32_t m[64];
        for (int i = 0, j = 0; i < 16; ++i, j += 4) {
            m[i] = (static_cast<uint32_t>(data_[j]) << 24) |
                   (static_cast<uint32_t>(data_[j + 1]) << 16) |
                   (static_cast<uint32_t>(data_[j + 2]) << 8) |
                   (static_cast<uint32_t>(data_[j + 3]));
        }
        for (int i = 16; i < 64; ++i) {
            m[i] = gam1(m[i - 2]) + m[i - 7] + gam0(m[i - 15]) + m[i - 16];
        }

        uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];

        for (int i = 0; i < 64; ++i) {
            uint32_t t1 = h + sig1(e) + ch(e, f, g) + K[i] + m[i];
            uint32_t t2 = sig0(a) + maj(a, b, c);
            h = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }

        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
        state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
    }

    void pad() {
        uint64_t total_bits = bitlen_ + block_len_ * 8;
        data_[block_len_++] = 0x80;
        if (block_len_ > 56) {
            while (block_len_ < 64) data_[block_len_++] = 0x00;
            transform();
            block_len_ = 0;
        }
        while (block_len_ < 56) data_[block_len_++] = 0x00;
        for (int i = 7; i >= 0; --i) {
            data_[block_len_++] = static_cast<uint8_t>((total_bits >> (i * 8)) & 0xFF);
        }
        transform();
    }

    void revert() {}

    void reset() {
        state_[0] = 0x6a09e667; state_[1] = 0xbb67ae85;
        state_[2] = 0x3c6ef372; state_[3] = 0xa54ff53a;
        state_[4] = 0x510e527f; state_[5] = 0x9b05688c;
        state_[6] = 0x1f83d9ab; state_[7] = 0x5be0cd19;
        bitlen_ = 0;
        block_len_ = 0;
    }
};

static std::string hmac_sha256(const std::string& key, const std::string& msg) {
    std::string k = key;
    if (k.size() > 64) {
        SHA256 kh;
        kh.update(k);
        k = kh.raw_digest();
    }
    k.resize(64, '\0');

    std::string o_key_pad(64, '\0');
    std::string i_key_pad(64, '\0');
    for (size_t i = 0; i < 64; ++i) {
        o_key_pad[i] = k[i] ^ 0x5c;
        i_key_pad[i] = k[i] ^ 0x36;
    }

    SHA256 inner;
    inner.update(i_key_pad + msg);
    std::string inner_hash = inner.raw_digest();

    SHA256 outer;
    outer.update(o_key_pad + inner_hash);
    return outer.hexdigest();
}

static std::string pbkdf2_sha256(const std::string& password, const std::string& salt, int iterations = 10000) {
    std::string digest = hmac_sha256(password, salt + std::string("\x00\x00\x00\x01", 4));
    std::string u = digest;
    for (int i = 1; i < iterations; ++i) {
        u = hmac_sha256(password, u);
    }
    return "pbkdf2_sha256$" + std::to_string(iterations) + "$" + salt + "$" + u;
}

static std::string base64url_encode(const std::string& in) {
    std::string b64 = base64_encode(in);
    std::string out;
    for (char c : b64) {
        if (c == '+') out += '-';
        else if (c == '/') out += '_';
        else if (c != '=') out += c;
    }
    return out;
}

static std::string base64url_decode(const std::string& in) {
    std::string b64;
    for (char c : in) {
        if (c == '-') b64 += '+';
        else if (c == '_') b64 += '/';
        else b64 += c;
    }
    while (b64.size() % 4 != 0) b64 += '=';
    return base64_decode(b64);
}

// MD5 Implementation (RFC 1321)
class MD5 {
public:
    MD5() { reset(); }

    void update(const uint8_t* input, size_t length) {
        size_t index = static_cast<size_t>((count_[0] >> 3) & 0x3F);
        if ((count_[0] += static_cast<uint32_t>(length << 3)) < static_cast<uint32_t>(length << 3)) count_[1]++;
        count_[1] += static_cast<uint32_t>(length >> 29);
        size_t partLen = 64 - index;
        size_t i = 0;
        if (length >= partLen) {
            std::memcpy(&buffer_[index], input, partLen);
            transform(buffer_);
            for (i = partLen; i + 63 < length; i += 64) transform(&input[i]);
            index = 0;
        }
        std::memcpy(&buffer_[index], &input[i], length - i);
    }

    void update(const std::string& str) {
        update(reinterpret_cast<const uint8_t*>(str.data()), str.size());
    }

    std::string hexdigest() {
        uint8_t bits[8];
        encode(bits, count_, 8);
        size_t index = static_cast<size_t>((count_[0] >> 3) & 0x3f);
        size_t padLen = (index < 56) ? (56 - index) : (120 - index);
        static const uint8_t PADDING[64] = { 0x80 };
        update(PADDING, padLen);
        update(bits, 8);
        uint8_t digest[16];
        encode(digest, state_, 16);
        std::ostringstream ss;
        for (int i = 0; i < 16; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
        }
        return ss.str();
    }

private:
    uint32_t state_[4];
    uint32_t count_[2];
    uint8_t buffer_[64];

    static uint32_t F(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }
    static uint32_t G(uint32_t x, uint32_t y, uint32_t z) { return (x & z) | (y & ~z); }
    static uint32_t H(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }
    static uint32_t I(uint32_t x, uint32_t y, uint32_t z) { return y ^ (x | ~z); }
    static uint32_t rotate_left(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

    static void FF(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
        a = rotate_left(a + F(b, c, d) + x + ac, s) + b;
    }
    static void GG(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
        a = rotate_left(a + G(b, c, d) + x + ac, s) + b;
    }
    static void HH(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
        a = rotate_left(a + H(b, c, d) + x + ac, s) + b;
    }
    static void II(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
        a = rotate_left(a + I(b, c, d) + x + ac, s) + b;
    }

    void transform(const uint8_t block[64]) {
        uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3], x[16];
        decode(x, block, 64);

        FF(a, b, c, d, x[ 0], 7, 0xd76aa478); FF(d, a, b, c, x[ 1], 12, 0xe8c7b756);
        FF(c, d, a, b, x[ 2], 17, 0x242070db); FF(b, c, d, a, x[ 3], 22, 0xc1bdceee);
        FF(a, b, c, d, x[ 4], 7, 0xf57c0faf); FF(d, a, b, c, x[ 5], 12, 0x4787c62a);
        FF(c, d, a, b, x[ 6], 17, 0xa8304613); FF(b, c, d, a, x[ 7], 22, 0xfd469501);
        FF(a, b, c, d, x[ 8], 7, 0x698098d8); FF(d, a, b, c, x[ 9], 12, 0x8b44f7af);
        FF(c, d, a, b, x[10], 17, 0xffff5bb1); FF(b, c, d, a, x[11], 22, 0x895cd7be);
        FF(a, b, c, d, x[12], 7, 0x6b901122); FF(d, a, b, c, x[13], 12, 0xfd987193);
        FF(c, d, a, b, x[14], 17, 0xa679438e); FF(b, c, d, a, x[15], 22, 0x49b40821);

        GG(a, b, c, d, x[ 1], 5, 0xf61e2562); GG(d, a, b, c, x[ 6], 9, 0xc040b340);
        GG(c, d, a, b, x[11], 14, 0x265e5a51); GG(b, c, d, a, x[ 0], 20, 0xe9b6c7aa);
        GG(a, b, c, d, x[ 5], 5, 0xd62f105d); GG(d, a, b, c, x[10], 9, 0x02441453);
        GG(c, d, a, b, x[15], 14, 0xd8a1e681); GG(b, c, d, a, x[ 4], 20, 0xe7d3fbc8);
        GG(a, b, c, d, x[ 9], 5, 0x21e1cde6); GG(d, a, b, c, x[14], 9, 0xc33707d6);
        GG(c, d, a, b, x[ 3], 14, 0xf4d50d87); GG(b, c, d, a, x[ 8], 20, 0x455a14ed);
        GG(a, b, c, d, x[13], 5, 0xa9e3e905); GG(d, a, b, c, x[ 2], 9, 0xfcefa3f8);
        GG(c, d, a, b, x[ 7], 14, 0x676f02d9); GG(b, c, d, a, x[12], 20, 0x8d2a4c8a);

        HH(a, b, c, d, x[ 5], 4, 0xfffa3942); HH(d, a, b, c, x[ 8], 11, 0x8771f681);
        HH(c, d, a, b, x[11], 16, 0x6d9d6122); HH(b, c, d, a, x[14], 23, 0xfde5380c);
        HH(a, b, c, d, x[ 1], 4, 0xa4beea44); HH(d, a, b, c, x[ 4], 11, 0x4bdecfa9);
        HH(c, d, a, b, x[ 7], 16, 0xf6bb4b60); HH(b, c, d, a, x[10], 23, 0xbebfbc70);
        HH(a, b, c, d, x[13], 4, 0x289b7ec6); HH(d, a, b, c, x[ 0], 11, 0xeaa127fa);
        HH(c, d, a, b, x[ 3], 16, 0xd4ef3085); HH(b, c, d, a, x[ 6], 23, 0x04881d05);
        HH(a, b, c, d, x[ 9], 4, 0xd9d4d039); HH(d, a, b, c, x[12], 11, 0xe6db99e5);
        HH(c, d, a, b, x[15], 16, 0x1fa27cf8); HH(b, c, d, a, x[ 2], 23, 0xc4ac5665);

        II(a, b, c, d, x[ 0], 6, 0xf4292244); II(d, a, b, c, x[ 7], 10, 0x432aff97);
        II(c, d, a, b, x[14], 15, 0xab9423a7); II(b, c, d, a, x[ 5], 21, 0xfc93a039);
        II(a, b, c, d, x[12], 6, 0x655b59c3); II(d, a, b, c, x[ 3], 10, 0x8f0ccc92);
        II(c, d, a, b, x[10], 15, 0xffeff47d); II(b, c, d, a, x[ 1], 21, 0x85845dd1);
        II(a, b, c, d, x[ 8], 6, 0x6fa87e4f); II(d, a, b, c, x[15], 10, 0xfe2ce6e0);
        II(c, d, a, b, x[ 6], 15, 0xa3014314); II(b, c, d, a, x[13], 21, 0x4e0811a1);
        II(a, b, c, d, x[ 4], 6, 0xf7537e82); II(d, a, b, c, x[11], 10, 0xbd3af235);
        II(c, d, a, b, x[ 2], 15, 0x2ad7d2bb); II(b, c, d, a, x[ 9], 21, 0xeb86d391);

        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
    }

    static void encode(uint8_t* output, const uint32_t* input, size_t len) {
        for (size_t i = 0, j = 0; j < len; i++, j += 4) {
            output[j]     = static_cast<uint8_t>(input[i] & 0xff);
            output[j + 1] = static_cast<uint8_t>((input[i] >> 8) & 0xff);
            output[j + 2] = static_cast<uint8_t>((input[i] >> 16) & 0xff);
            output[j + 3] = static_cast<uint8_t>((input[i] >> 24) & 0xff);
        }
    }

    static void decode(uint32_t* output, const uint8_t* input, size_t len) {
        for (size_t i = 0, j = 0; j < len; i++, j += 4) {
            output[i] = (static_cast<uint32_t>(input[j])) |
                        (static_cast<uint32_t>(input[j + 1]) << 8) |
                        (static_cast<uint32_t>(input[j + 2]) << 16) |
                        (static_cast<uint32_t>(input[j + 3]) << 24);
        }
    }

    void reset() {
        count_[0] = count_[1] = 0;
        state_[0] = 0x67452301; state_[1] = 0xefcdab89;
        state_[2] = 0x98badcfe; state_[3] = 0x10325476;
    }
};

// ============================================================================
// Internal Helpers: Full Recursive-Descent JSON Parser & Serializer
// ============================================================================

std::string json_escape_string(const std::string& str) {
    std::ostringstream ss;
    ss << '"';
    for (char c : str) {
        switch (c) {
            case '"':  ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b";  break;
            case '\f': ss << "\\f";  break;
            case '\n': ss << "\\n";  break;
            case '\r': ss << "\\r";  break;
            case '\t': ss << "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    ss << c;
                }
        }
    }
    ss << '"';
    return ss.str();
}

std::string json_stringify_val(const Value& val, int indent, int depth) {
    std::string indent_str = (indent > 0) ? std::string(depth * indent, ' ') : "";
    std::string next_indent_str = (indent > 0) ? std::string((depth + 1) * indent, ' ') : "";
    std::string nl = (indent > 0) ? "\n" : "";
    std::string sp = (indent > 0) ? " " : "";

    if (val.is_nil()) return "null";
    if (val.is_bool()) return val.as_bool() ? "true" : "false";
    if (val.is_int()) return std::to_string(val.as_int());
    if (val.is_float()) {
        double d = val.as_float();
        if (std::isnan(d) || std::isinf(d)) return "null";
        std::ostringstream ss;
        ss << std::setprecision(10) << d;
        std::string s = ss.str();
        if (s.find('.') == std::string::npos && s.find('e') == std::string::npos) s += ".0";
        return s;
    }
    if (val.is_string()) return json_escape_string(val.as_string());

    if (val.is_array()) {
        const auto& arr = *val.as_array();
        if (arr.empty()) return "[]";
        std::string out = "[" + nl;
        for (size_t i = 0; i < arr.size(); ++i) {
            out += next_indent_str + json_stringify_val(arr[i], indent, depth + 1);
            if (i + 1 < arr.size()) out += ",";
            out += nl;
        }
        out += indent_str + "]";
        return out;
    }

    if (val.is_object()) {
        const auto& obj = *val.as_object();
        if (obj.empty()) return "{}";
        std::string out = "{" + nl;
        size_t count = 0;
        for (const auto& [k, v] : obj) {
            if (v.is_function()) continue; // Skip functions in JSON
            if (count > 0) out += "," + nl;
            out += next_indent_str + json_escape_string(k) + ":" + sp + json_stringify_val(v, indent, depth + 1);
            count++;
        }
        out += nl + indent_str + "}";
        return out;
    }

    return "null";
}

class JsonParser {
public:
    explicit JsonParser(const std::string& src) : src_(src), pos_(0) {}

    Value parse() {
        skip_whitespace();
        if (pos_ >= src_.size()) throw std::runtime_error("Unexpected end of JSON input");
        Value v = parse_value();
        skip_whitespace();
        return v;
    }

private:
    const std::string& src_;
    size_t pos_;

    void skip_whitespace() {
        while (pos_ < src_.size() && (src_[pos_] == ' ' || src_[pos_] == '\t' || src_[pos_] == '\n' || src_[pos_] == '\r')) {
            pos_++;
        }
    }

    char peek() const { return pos_ < src_.size() ? src_[pos_] : '\0'; }
    char get() { return pos_ < src_.size() ? src_[pos_++] : '\0'; }

    Value parse_value() {
        skip_whitespace();
        char c = peek();
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == '"') return parse_string();
        if (c == 't' || c == 'f') return parse_bool();
        if (c == 'n') return parse_null();
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_number();
        throw std::runtime_error(std::string("Unexpected character in JSON: '") + c + "' at position " + std::to_string(pos_));
    }

    Value parse_object() {
        get(); // consume '{'
        std::map<std::string, Value> obj;
        skip_whitespace();
        if (peek() == '}') { get(); return Value::make_object(std::move(obj)); }

        while (pos_ < src_.size()) {
            skip_whitespace();
            if (peek() != '"') throw std::runtime_error("Expected string key in JSON object at pos " + std::to_string(pos_));
            std::string key = parse_raw_string();
            skip_whitespace();
            if (get() != ':') throw std::runtime_error("Expected ':' after key in JSON object at pos " + std::to_string(pos_));
            Value val = parse_value();
            obj[key] = val;
            skip_whitespace();
            char next = get();
            if (next == '}') break;
            if (next != ',') throw std::runtime_error("Expected ',' or '}' in JSON object at pos " + std::to_string(pos_));
        }
        return Value::make_object(std::move(obj));
    }

    Value parse_array() {
        get(); // consume '['
        std::vector<Value> arr;
        skip_whitespace();
        if (peek() == ']') { get(); return Value::make_array(std::move(arr)); }

        while (pos_ < src_.size()) {
            arr.push_back(parse_value());
            skip_whitespace();
            char next = get();
            if (next == ']') break;
            if (next != ',') throw std::runtime_error("Expected ',' or ']' in JSON array at pos " + std::to_string(pos_));
        }
        return Value::make_array(std::move(arr));
    }

    std::string parse_raw_string() {
        get(); // consume '"'
        std::string s;
        while (pos_ < src_.size()) {
            char c = get();
            if (c == '"') return s;
            if (c == '\\') {
                char esc = get();
                switch (esc) {
                    case '"': s += '"'; break;
                    case '\\': s += '\\'; break;
                    case '/': s += '/'; break;
                    case 'b': s += '\b'; break;
                    case 'f': s += '\f'; break;
                    case 'n': s += '\n'; break;
                    case 'r': s += '\r'; break;
                    case 't': s += '\t'; break;
                    case 'u': {
                        // Unicode 4-hex escape
                        std::string hex_str;
                        for (int i = 0; i < 4 && pos_ < src_.size(); ++i) hex_str += get();
                        try {
                            int code = std::stoi(hex_str, nullptr, 16);
                            if (code < 128) s += static_cast<char>(code);
                            else s += '?';
                        } catch (...) { s += '?'; }
                        break;
                    }
                    default: s += esc; break;
                }
            } else {
                s += c;
            }
        }
        throw std::runtime_error("Unterminated string in JSON");
    }

    Value parse_string() {
        return Value::make_string(parse_raw_string());
    }

    Value parse_bool() {
        if (src_.compare(pos_, 4, "true") == 0) { pos_ += 4; return Value::make_bool(true); }
        if (src_.compare(pos_, 5, "false") == 0) { pos_ += 5; return Value::make_bool(false); }
        throw std::runtime_error("Invalid boolean literal in JSON");
    }

    Value parse_null() {
        if (src_.compare(pos_, 4, "null") == 0) { pos_ += 4; return Value::make_nil(); }
        throw std::runtime_error("Invalid null literal in JSON");
    }

    Value parse_number() {
        size_t start = pos_;
        bool is_float = false;
        if (peek() == '-') pos_++;
        while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) pos_++;
        if (pos_ < src_.size() && src_[pos_] == '.') {
            is_float = true;
            pos_++;
            while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) pos_++;
        }
        if (pos_ < src_.size() && (src_[pos_] == 'e' || src_[pos_] == 'E')) {
            is_float = true;
            pos_++;
            if (pos_ < src_.size() && (src_[pos_] == '+' || src_[pos_] == '-')) pos_++;
            while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) pos_++;
        }

        std::string num_str = src_.substr(start, pos_ - start);
        if (is_float) {
            return Value::make_float(std::stod(num_str));
        } else {
            return Value::make_int(std::stoll(num_str));
        }
    }
};

} // anonymous namespace

// ============================================================================
// ModuleManager Core Lifecycle
// ============================================================================

ModuleManager::ModuleManager(DiagnosticEngine& diagnostics)
    : diagnostics_(diagnostics) {
    search_paths_.push_back(".");
    search_paths_.push_back("./std");
    search_paths_.push_back("./modules");
    search_paths_.push_back("./nextviper_modules");
    search_paths_.push_back("./packages");
    search_paths_.push_back("./.nextviper/packages");
    register_builtin_modules();
}

void ModuleManager::add_search_path(const std::string& path) {
    if (std::find(search_paths_.begin(), search_paths_.end(), path) == search_paths_.end()) {
        search_paths_.push_back(path);
    }
}

void ModuleManager::clear_cache() {
    module_cache_.clear();
    loading_modules_.clear();
    loaded_ast_programs_.clear();
    register_builtin_modules();
}

bool ModuleManager::is_builtin(const std::string& name) const {
    std::string mod = name;
    if (mod.rfind("std.", 0) == 0) mod = mod.substr(4);

    return mod == "io" || mod == "fs" || mod == "path" || mod == "string" ||
           mod == "collections" || mod == "math" || mod == "json" || mod == "csv" ||
           mod == "time" || mod == "http" || mod == "process" || mod == "crypto" ||
           mod == "regex" || mod == "random" || mod == "concurrency" || mod == "sys" ||
           mod == "data" || mod == "tensor" || mod == "ai" ||
           mod == "net" || mod == "db" || mod == "postgres" || mod == "log" || mod == "env";
}

std::optional<Value> ModuleManager::get_builtin_module(const std::string& name) {
    std::string mod = name;
    if (mod.rfind("std.", 0) == 0) mod = mod.substr(4);

    auto it = module_cache_.find(mod);
    if (it != module_cache_.end()) return it->second;

    Value res = Value::make_nil();
    if (mod == "io") res = create_io_module();
    else if (mod == "fs") res = create_fs_module();
    else if (mod == "path") res = create_path_module();
    else if (mod == "string") res = create_string_module();
    else if (mod == "collections") res = create_collections_module();
    else if (mod == "math") res = create_math_module();
    else if (mod == "json") res = create_json_module();
    else if (mod == "csv") res = create_csv_module();
    else if (mod == "time") res = create_time_module();
    else if (mod == "http") res = create_http_module();
    else if (mod == "process") res = create_process_module();
    else if (mod == "crypto") res = create_crypto_module();
    else if (mod == "regex") res = create_regex_module();
    else if (mod == "random") res = create_random_module();
    else if (mod == "concurrency") res = create_concurrency_module();
    else if (mod == "sys") res = create_sys_module();
    else if (mod == "data") res = create_data_module();
    else if (mod == "tensor") res = create_tensor_module();
    else if (mod == "ai") res = create_ai_module();
    else if (mod == "net") res = create_net_module();
    else if (mod == "db" || mod == "postgres") res = create_db_module();
    else if (mod == "log") res = create_log_module();
    else if (mod == "env") res = create_env_module();
    else return std::nullopt;

    module_cache_[mod] = res;
    module_cache_["std." + mod] = res;
    return res;
}

void ModuleManager::register_builtin_modules() {
    auto reg = [&](const std::string& name, Value mod) {
        module_cache_[name] = mod;
        module_cache_["std." + name] = mod;
    };

    reg("io", create_io_module());
    reg("fs", create_fs_module());
    reg("path", create_path_module());
    reg("string", create_string_module());
    reg("collections", create_collections_module());
    reg("math", create_math_module());
    reg("json", create_json_module());
    reg("csv", create_csv_module());
    reg("time", create_time_module());
    reg("http", create_http_module());
    reg("process", create_process_module());
    reg("crypto", create_crypto_module());
    reg("regex", create_regex_module());
    reg("random", create_random_module());
    reg("concurrency", create_concurrency_module());
    reg("sys", create_sys_module());
    reg("data", create_data_module());
    reg("tensor", create_tensor_module());
    reg("ai", create_ai_module());
    reg("net", create_net_module());
    reg("db", create_db_module());
    reg("log", create_log_module());
    reg("env", create_env_module());
}

// ============================================================================
// Standard Library Modules Implementation
// ============================================================================

Value ModuleManager::create_io_module() {
    std::map<std::string, Value> exports;

    exports["print"] = Value::make_native_fn("print", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) std::cout << " ";
            std::cout << args[i].to_string();
        }
        std::cout << "\n";
        return Value::make_nil();
    });

    exports["println"] = exports["print"];

    exports["eprint"] = Value::make_native_fn("eprint", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) std::cerr << " ";
            std::cerr << args[i].to_string();
        }
        std::cerr << "\n";
        return Value::make_nil();
    });

    exports["eprintln"] = exports["eprint"];

    exports["write"] = Value::make_native_fn("write", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::cout << args[0].to_string() << std::flush;
        return Value::make_nil();
    });

    exports["flush"] = Value::make_native_fn("flush", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        std::cout << std::flush;
        return Value::make_nil();
    });

    exports["read_line"] = Value::make_native_fn("read_line", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        std::string line;
        if (std::getline(std::cin, line)) {
            return Value::make_string(line);
        }
        return Value::make_nil();
    });

    exports["read_all"] = Value::make_native_fn("read_all", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        std::stringstream ss;
        ss << std::cin.rdbuf();
        return Value::make_string(ss.str());
    });

    return Value::make_object(std::move(exports));
}

Value ModuleManager::create_fs_module() {
    std::map<std::string, Value> exports;

    exports["read_text"] = Value::make_native_fn("read_text", 1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        std::string path = args[0].as_string();
        std::ifstream f(path);
        if (!f.is_open()) throw RuntimeError("fs.read_text: failed to open file '" + path + "'", span);
        std::stringstream ss;
        ss << f.rdbuf();
        return Value::make_string(ss.str());
    });
    exports["read_file"] = exports["read_text"];
    exports["read"] = exports["read_text"];

    exports["write_text"] = Value::make_native_fn("write_text", 2, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        std::string path = args[0].as_string();
        std::string content = args[1].as_string();
        std::ofstream f(path);
        if (!f.is_open()) throw RuntimeError("fs.write_text: failed to write file '" + path + "'", span);
        f << content;
        return Value::make_bool(true);
    });
    exports["write_file"] = exports["write_text"];
    exports["write"] = exports["write_text"];

    exports["append_text"] = Value::make_native_fn("append_text", 2, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        std::string path = args[0].as_string();
        std::string content = args[1].as_string();
        std::ofstream f(path, std::ios::app);
        if (!f.is_open()) throw RuntimeError("fs.append_text: failed to open file '" + path + "'", span);
        f << content;
        return Value::make_bool(true);
    });
    exports["append_file"] = exports["append_text"];

    exports["exists"] = Value::make_native_fn("exists", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::error_code ec;
        return Value::make_bool(fs::exists(args[0].as_string(), ec));
    });

    exports["is_file"] = Value::make_native_fn("is_file", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::error_code ec;
        return Value::make_bool(fs::is_regular_file(args[0].as_string(), ec));
    });

    exports["is_dir"] = Value::make_native_fn("is_dir", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::error_code ec;
        return Value::make_bool(fs::is_directory(args[0].as_string(), ec));
    });

    exports["list"] = Value::make_native_fn("list", -1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        std::string dir_path = args.empty() ? "." : args[0].as_string();
        std::error_code ec;
        if (!fs::exists(dir_path, ec) || !fs::is_directory(dir_path, ec)) {
            throw RuntimeError("fs.list: directory not found '" + dir_path + "'", span);
        }
        std::vector<Value> files;
        for (const auto& entry : fs::directory_iterator(dir_path, ec)) {
            files.push_back(Value::make_string(entry.path().filename().string()));
        }
        std::sort(files.begin(), files.end(), [](const Value& a, const Value& b) {
            return a.as_string() < b.as_string();
        });
        return Value::make_array(std::move(files));
    });
    exports["read_dir"] = exports["list"];

    exports["make_dir"] = Value::make_native_fn("make_dir", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::error_code ec;
        return Value::make_bool(fs::create_directories(args[0].as_string(), ec));
    });
    exports["mkdir"] = exports["make_dir"];

    exports["remove"] = Value::make_native_fn("remove", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::error_code ec;
        return Value::make_bool(fs::remove(args[0].as_string(), ec));
    });
    exports["delete_file"] = exports["remove"];

    exports["remove_dir"] = Value::make_native_fn("remove_dir", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::error_code ec;
        return Value::make_int(fs::remove_all(args[0].as_string(), ec));
    });

    exports["copy"] = Value::make_native_fn("copy", 2, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        std::error_code ec;
        bool ok = fs::copy_file(args[0].as_string(), args[1].as_string(), fs::copy_options::overwrite_existing, ec);
        if (!ok && ec) throw RuntimeError("fs.copy failed: " + ec.message(), span);
        return Value::make_bool(ok);
    });

    exports["move"] = Value::make_native_fn("move", 2, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        std::error_code ec;
        fs::rename(args[0].as_string(), args[1].as_string(), ec);
        if (ec) throw RuntimeError("fs.move failed: " + ec.message(), span);
        return Value::make_bool(true);
    });
    exports["rename"] = exports["move"];

    exports["size"] = Value::make_native_fn("size", 1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        std::error_code ec;
        auto sz = fs::file_size(args[0].as_string(), ec);
        if (ec) throw RuntimeError("fs.size failed: " + ec.message(), span);
        return Value::make_int(static_cast<int64_t>(sz));
    });

    return Value::make_object(std::move(exports));
}

Value ModuleManager::create_path_module() {
    std::map<std::string, Value> exports;

    exports["join"] = Value::make_native_fn("join", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        fs::path p;
        for (const auto& a : args) {
            p /= a.as_string();
        }
        return Value::make_string(p.string());
    });

    exports["dirname"] = Value::make_native_fn("dirname", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_string(fs::path(args[0].as_string()).parent_path().string());
    });

    exports["basename"] = Value::make_native_fn("basename", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_string(fs::path(args[0].as_string()).filename().string());
    });

    exports["extname"] = Value::make_native_fn("extname", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_string(fs::path(args[0].as_string()).extension().string());
    });
    exports["extension"] = exports["extname"];

    exports["is_absolute"] = Value::make_native_fn("is_absolute", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_bool(fs::path(args[0].as_string()).is_absolute());
    });

    exports["normalize"] = Value::make_native_fn("normalize", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_string(fs::path(args[0].as_string()).lexically_normal().string());
    });

    exports["separator"] = Value::make_string("/");

    return Value::make_object(std::move(exports));
}

Value ModuleManager::create_string_module() {
    std::map<std::string, Value> exports;

    exports["split"] = Value::make_native_fn("split", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::string s = args[0].as_string();
        std::string delim = args[1].as_string();
        std::vector<Value> res;
        if (delim.empty()) {
            for (char c : s) res.push_back(Value::make_string(std::string(1, c)));
            return Value::make_array(std::move(res));
        }
        size_t start = 0, end = 0;
        while ((end = s.find(delim, start)) != std::string::npos) {
            res.push_back(Value::make_string(s.substr(start, end - start)));
            start = end + delim.length();
        }
        res.push_back(Value::make_string(s.substr(start)));
        return Value::make_array(std::move(res));
    });

    exports["join"] = Value::make_native_fn("join", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (!args[0].is_array()) return Value::make_string("");
        const auto& arr = *args[0].as_array();
        std::string delim = args[1].as_string();
        std::string res;
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) res += delim;
            res += arr[i].as_string();
        }
        return Value::make_string(res);
    });

    exports["trim"] = Value::make_native_fn("trim", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::string s = args[0].as_string();
        size_t first = s.find_first_not_of(" \t\n\r");
        if (first == std::string::npos) return Value::make_string("");
        size_t last = s.find_last_not_of(" \t\n\r");
        return Value::make_string(s.substr(first, (last - first + 1)));
    });

    exports["trim_start"] = Value::make_native_fn("trim_start", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::string s = args[0].as_string();
        size_t first = s.find_first_not_of(" \t\n\r");
        if (first == std::string::npos) return Value::make_string("");
        return Value::make_string(s.substr(first));
    });

    exports["trim_end"] = Value::make_native_fn("trim_end", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::string s = args[0].as_string();
        size_t last = s.find_last_not_of(" \t\n\r");
        if (last == std::string::npos) return Value::make_string("");
        return Value::make_string(s.substr(0, last + 1));
    });

    exports["to_upper"] = Value::make_native_fn("to_upper", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::string s = args[0].as_string();
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        return Value::make_string(s);
    });

    exports["to_lower"] = Value::make_native_fn("to_lower", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::string s = args[0].as_string();
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return Value::make_string(s);
    });

    exports["starts_with"] = Value::make_native_fn("starts_with", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::string s = args[0].as_string();
        std::string prefix = args[1].as_string();
        return Value::make_bool(s.rfind(prefix, 0) == 0);
    });

    exports["ends_with"] = Value::make_native_fn("ends_with", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::string s = args[0].as_string();
        std::string suffix = args[1].as_string();
        if (suffix.size() > s.size()) return Value::make_bool(false);
        return Value::make_bool(s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0);
    });

    exports["contains"] = Value::make_native_fn("contains", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::string s = args[0].as_string();
        std::string sub = args[1].as_string();
        return Value::make_bool(s.find(sub) != std::string::npos);
    });

    exports["index_of"] = Value::make_native_fn("index_of", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::string s = args[0].as_string();
        std::string sub = args[1].as_string();
        size_t idx = s.find(sub);
        return Value::make_int(idx == std::string::npos ? -1 : static_cast<int64_t>(idx));
    });

    exports["replace"] = Value::make_native_fn("replace", 3, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::string s = args[0].as_string();
        std::string from = args[1].as_string();
        std::string to = args[2].as_string();
        if (from.empty()) return Value::make_string(s);
        size_t start_pos = 0;
        while ((start_pos = s.find(from, start_pos)) != std::string::npos) {
            s.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
        return Value::make_string(s);
    });

    exports["len"] = Value::make_native_fn("len", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_int(static_cast<int64_t>(args[0].as_string().size()));
    });

    return Value::make_object(std::move(exports));
}

Value ModuleManager::create_collections_module() {
    std::map<std::string, Value> exports;

    exports["chunk"] = Value::make_native_fn("chunk", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (!args[0].is_array()) return Value::make_array({});
        const auto& arr = *args[0].as_array();
        int64_t sz = std::max<int64_t>(1, args[1].as_int());
        std::vector<Value> res;
        std::vector<Value> cur;
        for (const auto& item : arr) {
            cur.push_back(item);
            if (cur.size() == static_cast<size_t>(sz)) {
                res.push_back(Value::make_array(std::move(cur)));
                cur.clear();
            }
        }
        if (!cur.empty()) res.push_back(Value::make_array(std::move(cur)));
        return Value::make_array(std::move(res));
    });

    exports["flatten"] = Value::make_native_fn("flatten", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (!args[0].is_array()) return Value::make_array({});
        std::vector<Value> res;
        for (const auto& el : *args[0].as_array()) {
            if (el.is_array()) {
                for (const auto& sub : *el.as_array()) res.push_back(sub);
            } else {
                res.push_back(el);
            }
        }
        return Value::make_array(std::move(res));
    });

    exports["unique"] = Value::make_native_fn("unique", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (!args[0].is_array()) return Value::make_array({});
        std::vector<Value> res;
        std::unordered_set<std::string> seen;
        for (const auto& el : *args[0].as_array()) {
            std::string repr = el.to_string();
            if (seen.insert(repr).second) {
                res.push_back(el);
            }
        }
        return Value::make_array(std::move(res));
    });

    exports["reverse"] = Value::make_native_fn("reverse", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (!args[0].is_array()) return Value::make_array({});
        auto arr = *args[0].as_array();
        std::reverse(arr.begin(), arr.end());
        return Value::make_array(std::move(arr));
    });

    exports["sort"] = Value::make_native_fn("sort", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (!args[0].is_array()) return Value::make_array({});
        auto arr = *args[0].as_array();
        std::sort(arr.begin(), arr.end(), [](const Value& a, const Value& b) {
            if (a.is_number() && b.is_number()) return a.as_float() < b.as_float();
            return a.to_string() < b.to_string();
        });
        return Value::make_array(std::move(arr));
    });

    exports["zip"] = Value::make_native_fn("zip", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (!args[0].is_array() || !args[1].is_array()) return Value::make_array({});
        const auto& a = *args[0].as_array();
        const auto& b = *args[1].as_array();
        size_t n = std::min(a.size(), b.size());
        std::vector<Value> res;
        for (size_t i = 0; i < n; ++i) {
            res.push_back(Value::make_array({a[i], b[i]}));
        }
        return Value::make_array(std::move(res));
    });

    exports["merge"] = Value::make_native_fn("merge", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::map<std::string, Value> res;
        if (args[0].is_object()) for (const auto& [k, v] : *args[0].as_object()) res[k] = v;
        if (args[1].is_object()) for (const auto& [k, v] : *args[1].as_object()) res[k] = v;
        return Value::make_object(std::move(res));
    });

    exports["keys"] = Value::make_native_fn("keys", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::vector<Value> ks;
        if (args[0].is_object()) {
            for (const auto& [k, _] : *args[0].as_object()) ks.push_back(Value::make_string(k));
        }
        return Value::make_array(std::move(ks));
    });

    exports["values"] = Value::make_native_fn("values", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::vector<Value> vs;
        if (args[0].is_object()) {
            for (const auto& [_, v] : *args[0].as_object()) vs.push_back(v);
        }
        return Value::make_array(std::move(vs));
    });

    return Value::make_object(std::move(exports));
}

Value ModuleManager::create_json_module() {
    std::map<std::string, Value> exports;

    exports["stringify"] = Value::make_native_fn("stringify", -1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args.empty()) throw RuntimeError("json.stringify requires at least 1 argument", span);
        int indent = args.size() >= 2 ? static_cast<int>(args[1].as_int()) : 0;
        return Value::make_string(json_stringify_val(args[0], indent, 0));
    });

    exports["parse"] = Value::make_native_fn("parse", 1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        try {
            JsonParser parser(args[0].as_string());
            return parser.parse();
        } catch (const std::exception& e) {
            throw RuntimeError(std::string("json.parse error: ") + e.what(), span);
        }
    });

    return Value::make_object(std::move(exports));
}

Value ModuleManager::create_csv_module() {
    std::map<std::string, Value> exports;

    exports["parse"] = Value::make_native_fn("parse", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::string text = args[0].as_string();
        std::vector<Value> rows;
        std::istringstream stream(text);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.empty()) continue;
            std::vector<Value> row;
            std::string cell;
            bool in_quotes = false;
            for (size_t i = 0; i < line.size(); ++i) {
                char c = line[i];
                if (c == '"') in_quotes = !in_quotes;
                else if (c == ',' && !in_quotes) {
                    row.push_back(Value::make_string(cell));
                    cell.clear();
                } else {
                    cell += c;
                }
            }
            row.push_back(Value::make_string(cell));
            rows.push_back(Value::make_array(std::move(row)));
        }
        return Value::make_array(std::move(rows));
    });

    exports["stringify"] = Value::make_native_fn("stringify", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (!args[0].is_array()) return Value::make_string("");
        std::ostringstream ss;
        for (const auto& row_val : *args[0].as_array()) {
            if (!row_val.is_array()) continue;
            const auto& row = *row_val.as_array();
            for (size_t i = 0; i < row.size(); ++i) {
                if (i > 0) ss << ",";
                std::string s = row[i].to_string();
                if (s.find(',') != std::string::npos || s.find('"') != std::string::npos || s.find('\n') != std::string::npos) {
                    ss << '"';
                    for (char c : s) { if (c == '"') ss << "\"\""; else ss << c; }
                    ss << '"';
                } else {
                    ss << s;
                }
            }
            ss << "\n";
        }
        return Value::make_string(ss.str());
    });

    exports["read"] = Value::make_native_fn("read", 1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        std::string path = args[0].as_string();
        std::ifstream f(path);
        if (!f.is_open()) throw RuntimeError("csv.read failed to open file '" + path + "'", span);
        std::stringstream ss;
        ss << f.rdbuf();
        // Forward to parse
        std::string text = ss.str();
        std::vector<Value> rows;
        std::istringstream stream(text);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.empty()) continue;
            std::vector<Value> row;
            std::string cell;
            bool in_quotes = false;
            for (size_t i = 0; i < line.size(); ++i) {
                char c = line[i];
                if (c == '"') in_quotes = !in_quotes;
                else if (c == ',' && !in_quotes) {
                    row.push_back(Value::make_string(cell));
                    cell.clear();
                } else {
                    cell += c;
                }
            }
            row.push_back(Value::make_string(cell));
            rows.push_back(Value::make_array(std::move(row)));
        }
        return Value::make_array(std::move(rows));
    });

    return Value::make_object(std::move(exports));
}

Value ModuleManager::create_http_module() {
    std::map<std::string, Value> exports;

    auto make_response = [](int status, const std::string& body, const std::map<std::string, Value>& headers) -> Value {
        std::map<std::string, Value> resp;
        resp["status"] = Value::make_int(status);
        resp["text"] = Value::make_string(body);
        resp["body"] = Value::make_string(body);
        resp["ok"] = Value::make_bool(status >= 200 && status < 300);
        resp["headers"] = Value::make_object(headers);
        resp["json"] = Value::make_native_fn("json", 0, [body](const std::vector<Value>&, SourceSpan s) -> Value {
            try {
                JsonParser p(body);
                return p.parse();
            } catch (const std::exception& e) {
                throw RuntimeError(std::string("response.json() error: ") + e.what(), s);
            }
        });
        return Value::make_object(std::move(resp));
    };

    auto execute_http = [make_response](const std::string& method, const std::string& url, const std::string& body, const Value& headers_val, SourceSpan span) -> Value {
        (void)span;
        // Construct curl command for high compatibility
        std::string header_flags;
        if (headers_val.is_object()) {
            for (const auto& [k, v] : *headers_val.as_object()) {
                header_flags += " -H \"" + k + ": " + v.as_string() + "\"";
            }
        }

        std::string temp_out = "/tmp/nv_http_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::string cmd = "curl -s -w \"\\n__NV_STATUS__:%{http_code}\" -X " + method + header_flags;
        if (!body.empty()) {
            cmd += " -d " + json_escape_string(body);
        }
        cmd += " \"" + url + "\" > " + temp_out + " 2>&1";

        int res = std::system(cmd.c_str());
        if (res != 0) {
            std::remove(temp_out.c_str());
            // Return fallback mock response if network or curl fails
            return make_response(500, "{\"error\": \"HTTP request execution failed\"}", {{"content-type", Value::make_string("application/json")}});
        }

        std::ifstream in(temp_out);
        std::string full_out((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();
        std::remove(temp_out.c_str());

        int status = 200;
        std::string resp_body = full_out;
        size_t tag_pos = full_out.rfind("\n__NV_STATUS__:");
        if (tag_pos != std::string::npos) {
            resp_body = full_out.substr(0, tag_pos);
            try {
                status = std::stoi(full_out.substr(tag_pos + 15));
            } catch (...) { status = 200; }
        }

        std::map<std::string, Value> headers;
        headers["content-type"] = Value::make_string("application/json");
        return make_response(status, resp_body, headers);
    };

    exports["get"] = Value::make_native_fn("get", -1, [execute_http](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args.empty()) throw RuntimeError("http.get requires URL", span);
        std::string url = args[0].as_string();
        Value headers = args.size() >= 2 ? args[1] : Value::make_object({});
        return execute_http("GET", url, "", headers, span);
    });

    exports["post"] = Value::make_native_fn("post", -1, [execute_http](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args.empty()) throw RuntimeError("http.post requires URL", span);
        std::string url = args[0].as_string();
        std::string body = args.size() >= 2 ? (args[1].is_string() ? args[1].as_string() : json_stringify_val(args[1], 0, 0)) : "";
        Value headers = args.size() >= 3 ? args[2] : Value::make_object({});
        return execute_http("POST", url, body, headers, span);
    });

    exports["put"] = Value::make_native_fn("put", -1, [execute_http](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args.empty()) throw RuntimeError("http.put requires URL", span);
        std::string url = args[0].as_string();
        std::string body = args.size() >= 2 ? (args[1].is_string() ? args[1].as_string() : json_stringify_val(args[1], 0, 0)) : "";
        Value headers = args.size() >= 3 ? args[2] : Value::make_object({});
        return execute_http("PUT", url, body, headers, span);
    });

    exports["delete"] = Value::make_native_fn("delete", -1, [execute_http](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args.empty()) throw RuntimeError("http.delete requires URL", span);
        std::string url = args[0].as_string();
        Value headers = args.size() >= 2 ? args[1] : Value::make_object({});
        return execute_http("DELETE", url, "", headers, span);
    });

    exports["request"] = Value::make_native_fn("request", -1, [execute_http](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args.size() < 2) throw RuntimeError("http.request requires method and URL", span);
        std::string method = args[0].as_string();
        std::string url = args[1].as_string();
        std::string body = args.size() >= 3 ? (args[2].is_string() ? args[2].as_string() : json_stringify_val(args[2], 0, 0)) : "";
        Value headers = args.size() >= 4 ? args[3] : Value::make_object({});
        return execute_http(method, url, body, headers, span);
    });

    exports["server"] = Value::make_native_fn("server", -1, [this](const std::vector<Value>& args, SourceSpan span) -> Value {
        auto net_mod = create_net_module();
        auto net_obj = *net_mod.as_object();
        return net_obj.at("http_server").as_native_fn()->func(args, span);
    });
    exports["app"] = exports["server"];

    return Value::make_object(std::move(exports));
}

Value ModuleManager::create_process_module() {
    std::map<std::string, Value> exports;

    exports["exec"] = Value::make_native_fn("exec", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::string cmd = args[0].as_string();
        std::array<char, 256> buffer;
        std::string out;
        FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
        if (!pipe) {
            std::map<std::string, Value> res;
            res["exit_code"] = Value::make_int(-1);
            res["stdout"] = Value::make_string("");
            res["stderr"] = Value::make_string("Failed to spawn process");
            return Value::make_object(std::move(res));
        }
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            out += buffer.data();
        }
        int code = pclose(pipe);
        std::map<std::string, Value> res;
        res["exit_code"] = Value::make_int(code);
        res["stdout"] = Value::make_string(out);
        res["stderr"] = Value::make_string("");
        return Value::make_object(std::move(res));
    });

    exports["exit"] = Value::make_native_fn("exit", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        int code = args.empty() ? 0 : static_cast<int>(args[0].as_int());
        std::exit(code);
        return Value::make_nil();
    });

    exports["env"] = Value::make_native_fn("env", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        const char* val = std::getenv(args[0].as_string().c_str());
        return val ? Value::make_string(val) : Value::make_nil();
    });

    exports["cwd"] = Value::make_native_fn("cwd", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        return Value::make_string(fs::current_path().string());
    });

    exports["pid"] = Value::make_native_fn("pid", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        return Value::make_int(static_cast<int64_t>(getpid()));
    });

    return Value::make_object(std::move(exports));
}

Value ModuleManager::create_crypto_module() {
    std::map<std::string, Value> exports;

    exports["sha256"] = Value::make_native_fn("sha256", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        SHA256 hasher;
        hasher.update(args[0].as_string());
        return Value::make_string(hasher.hexdigest());
    });

    exports["md5"] = Value::make_native_fn("md5", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        MD5 hasher;
        hasher.update(args[0].as_string());
        return Value::make_string(hasher.hexdigest());
    });

    exports["base64_encode"] = Value::make_native_fn("base64_encode", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_string(base64_encode(args[0].as_string()));
    });

    exports["base64_decode"] = Value::make_native_fn("base64_decode", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_string(base64_decode(args[0].as_string()));
    });

    exports["random_bytes"] = Value::make_native_fn("random_bytes", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        int64_t n = std::max<int64_t>(1, args[0].as_int());
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(0, 255);
        std::ostringstream ss;
        for (int64_t i = 0; i < n; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << dist(gen);
        }
        return Value::make_string(ss.str());
    });

    exports["generate_token"] = exports["random_bytes"];

    exports["hmac_sha256"] = Value::make_native_fn("hmac_sha256", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_string(hmac_sha256(args[0].as_string(), args[1].as_string()));
    });

    exports["hash_password"] = Value::make_native_fn("hash_password", -1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args.empty()) throw RuntimeError("crypto.hash_password requires a password string", span);
        std::string pwd = args[0].as_string();
        std::string salt;
        if (args.size() >= 2 && args[1].is_string()) {
            salt = args[1].as_string();
        } else {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<int> dist(0, 255);
            std::ostringstream ss;
            for (int i = 0; i < 16; ++i) ss << std::hex << std::setw(2) << std::setfill('0') << dist(gen);
            salt = ss.str();
        }
        int iterations = args.size() >= 3 ? static_cast<int>(args[2].as_int()) : 10000;
        return Value::make_string(pbkdf2_sha256(pwd, salt, iterations));
    });

    exports["verify_password"] = Value::make_native_fn("verify_password", 2, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args.size() < 2) throw RuntimeError("crypto.verify_password requires password and hash", span);
        std::string pwd = args[0].as_string();
        std::string stored = args[1].as_string();

        // Parse pbkdf2_sha256$iterations$salt$hash
        if (stored.rfind("pbkdf2_sha256$", 0) != 0) {
            return Value::make_bool(false);
        }

        size_t p1 = stored.find('$', 14);
        if (p1 == std::string::npos) return Value::make_bool(false);
        size_t p2 = stored.find('$', p1 + 1);
        if (p2 == std::string::npos) return Value::make_bool(false);

        int iterations = 10000;
        try {
            iterations = std::stoi(stored.substr(14, p1 - 14));
        } catch (...) {}
        std::string salt = stored.substr(p1 + 1, p2 - (p1 + 1));
        std::string computed = pbkdf2_sha256(pwd, salt, iterations);
        return Value::make_bool(computed == stored);
    });

    exports["jwt_encode"] = Value::make_native_fn("jwt_encode", 2, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args.size() < 2) throw RuntimeError("crypto.jwt_encode requires payload object and secret key", span);
        std::string secret = args[1].as_string();
        std::string header_json = "{\"alg\":\"HS256\",\"typ\":\"JWT\"}";
        std::string payload_json = args[0].is_string() ? args[0].as_string() : json_stringify_val(args[0], 0, 0);

        std::string header_b64 = base64url_encode(header_json);
        std::string payload_b64 = base64url_encode(payload_json);
        std::string to_sign = header_b64 + "." + payload_b64;
        std::string signature = hmac_sha256(secret, to_sign);
        std::string sig_b64 = base64url_encode(signature);

        return Value::make_string(to_sign + "." + sig_b64);
    });

    exports["jwt_decode"] = Value::make_native_fn("jwt_decode", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::string token = args[0].as_string();
        std::string secret = args[1].as_string();

        size_t d1 = token.find('.');
        if (d1 == std::string::npos) return Value::make_nil();
        size_t d2 = token.find('.', d1 + 1);
        if (d2 == std::string::npos) return Value::make_nil();

        std::string header_b64 = token.substr(0, d1);
        std::string payload_b64 = token.substr(d1 + 1, d2 - (d1 + 1));
        std::string sig_b64 = token.substr(d2 + 1);

        std::string to_sign = header_b64 + "." + payload_b64;
        std::string expected_sig = hmac_sha256(secret, to_sign);
        std::string expected_sig_b64 = base64url_encode(expected_sig);

        if (sig_b64 != expected_sig_b64) {
            return Value::make_nil(); // Signature verification failed
        }

        std::string payload_json = base64url_decode(payload_b64);
        try {
            JsonParser parser(payload_json);
            return parser.parse();
        } catch (...) {
            return Value::make_nil();
        }
    });

    return Value::make_object(std::move(exports));
}

Value ModuleManager::create_regex_module() {
    std::map<std::string, Value> exports;

    exports["test"] = Value::make_native_fn("test", 2, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        try {
            std::regex re(args[0].as_string());
            return Value::make_bool(std::regex_search(args[1].as_string(), re));
        } catch (const std::exception& e) {
            throw RuntimeError(std::string("regex error: ") + e.what(), span);
        }
    });

    exports["match"] = Value::make_native_fn("match", 2, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        try {
            std::regex re(args[0].as_string());
            std::string text = args[1].as_string();
            std::smatch sm;
            if (std::regex_search(text, sm, re)) {
                std::vector<Value> matches;
                for (size_t i = 0; i < sm.size(); ++i) {
                    matches.push_back(Value::make_string(sm[i].str()));
                }
                return Value::make_array(std::move(matches));
            }
            return Value::make_nil();
        } catch (const std::exception& e) {
            throw RuntimeError(std::string("regex error: ") + e.what(), span);
        }
    });

    exports["find_all"] = Value::make_native_fn("find_all", 2, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        try {
            std::regex re(args[0].as_string());
            std::string text = args[1].as_string();
            std::vector<Value> results;
            auto words_begin = std::sregex_iterator(text.begin(), text.end(), re);
            auto words_end = std::sregex_iterator();
            for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
                results.push_back(Value::make_string((*i).str()));
            }
            return Value::make_array(std::move(results));
        } catch (const std::exception& e) {
            throw RuntimeError(std::string("regex error: ") + e.what(), span);
        }
    });

    exports["replace"] = Value::make_native_fn("replace", 3, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        try {
            std::regex re(args[0].as_string());
            std::string repl = args[1].as_string();
            std::string text = args[2].as_string();
            return Value::make_string(std::regex_replace(text, re, repl));
        } catch (const std::exception& e) {
            throw RuntimeError(std::string("regex error: ") + e.what(), span);
        }
    });

    return Value::make_object(std::move(exports));
}

Value ModuleManager::create_random_module() {
    std::map<std::string, Value> exports;

    static thread_local std::mt19937_64 rng(std::random_device{}());

    exports["random"] = Value::make_native_fn("random", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return Value::make_float(dist(rng));
    });

    exports["randint"] = Value::make_native_fn("randint", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        int64_t min_v = args[0].as_int();
        int64_t max_v = args[1].as_int();
        if (min_v > max_v) std::swap(min_v, max_v);
        std::uniform_int_distribution<int64_t> dist(min_v, max_v);
        return Value::make_int(dist(rng));
    });

    exports["uniform"] = Value::make_native_fn("uniform", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        double min_v = args[0].as_float();
        double max_v = args[1].as_float();
        if (min_v > max_v) std::swap(min_v, max_v);
        std::uniform_real_distribution<double> dist(min_v, max_v);
        return Value::make_float(dist(rng));
    });

    exports["choice"] = Value::make_native_fn("choice", 1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (!args[0].is_array() || args[0].as_array()->empty()) throw RuntimeError("random.choice requires non-empty list", span);
        const auto& arr = *args[0].as_array();
        std::uniform_int_distribution<size_t> dist(0, arr.size() - 1);
        return arr[dist(rng)];
    });

    exports["shuffle"] = Value::make_native_fn("shuffle", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (!args[0].is_array()) return Value::make_array({});
        auto arr = *args[0].as_array();
        std::shuffle(arr.begin(), arr.end(), rng);
        return Value::make_array(std::move(arr));
    });

    exports["seed"] = Value::make_native_fn("seed", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        rng.seed(static_cast<uint64_t>(args[0].as_int()));
        return Value::make_nil();
    });

    return Value::make_object(std::move(exports));
}

Value ModuleManager::create_concurrency_module() {
    std::map<std::string, Value> exports;

    exports["sleep"] = Value::make_native_fn("sleep", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        int64_t ms = args[0].is_int() ? args[0].as_int() : static_cast<int64_t>(args[0].as_float() * 1000.0);
        if (ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        return Value::make_nil();
    });

    exports["channel"] = Value::make_native_fn("channel", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        struct ChannelState {
            std::queue<Value> q;
            std::mutex mtx;
            std::condition_variable cv;
            size_t cap = 0;
            bool closed = false;
        };

        auto state = std::make_shared<ChannelState>();
        state->cap = args.empty() ? 1024 : static_cast<size_t>(args[0].as_int());

        std::map<std::string, Value> chan;

        chan["send"] = Value::make_native_fn("send", 1, [state](const std::vector<Value>& a, SourceSpan s) -> Value {
            std::unique_lock<std::mutex> lock(state->mtx);
            if (state->closed) throw RuntimeError("Cannot send to closed channel", s);
            state->q.push(a[0]);
            state->cv.notify_one();
            return Value::make_bool(true);
        });

        chan["recv"] = Value::make_native_fn("recv", 0, [state](const std::vector<Value>&, SourceSpan) -> Value {
            std::unique_lock<std::mutex> lock(state->mtx);
            state->cv.wait(lock, [state] { return !state->q.empty() || state->closed; });
            if (state->q.empty()) return Value::make_nil();
            Value v = state->q.front();
            state->q.pop();
            return v;
        });

        chan["try_recv"] = Value::make_native_fn("try_recv", 0, [state](const std::vector<Value>&, SourceSpan) -> Value {
            std::lock_guard<std::mutex> lock(state->mtx);
            if (state->q.empty()) return Value::make_nil();
            Value v = state->q.front();
            state->q.pop();
            return v;
        });

        chan["len"] = Value::make_native_fn("len", 0, [state](const std::vector<Value>&, SourceSpan) -> Value {
            std::lock_guard<std::mutex> lock(state->mtx);
            return Value::make_int(static_cast<int64_t>(state->q.size()));
        });

        chan["close"] = Value::make_native_fn("close", 0, [state](const std::vector<Value>&, SourceSpan) -> Value {
            std::lock_guard<std::mutex> lock(state->mtx);
            state->closed = true;
            state->cv.notify_all();
            return Value::make_nil();
        });

        return Value::make_object(std::move(chan));
    });

    return Value::make_object(std::move(exports));
}

Value ModuleManager::create_math_module() {
    std::map<std::string, Value> exports;

    exports["pi"] = Value::make_float(3.14159265358979323846);
    exports["e"] = Value::make_float(2.71828182845904523536);
    exports["inf"] = Value::make_float(std::numeric_limits<double>::infinity());
    exports["nan"] = Value::make_float(std::numeric_limits<double>::quiet_NaN());

    exports["sqrt"] = Value::make_native_fn("sqrt", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_float(std::sqrt(args[0].as_float()));
    });

    exports["cbrt"] = Value::make_native_fn("cbrt", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_float(std::cbrt(args[0].as_float()));
    });

    exports["sin"] = Value::make_native_fn("sin", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_float(std::sin(args[0].as_float()));
    });

    exports["cos"] = Value::make_native_fn("cos", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_float(std::cos(args[0].as_float()));
    });

    exports["tan"] = Value::make_native_fn("tan", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_float(std::tan(args[0].as_float()));
    });

    exports["asin"] = Value::make_native_fn("asin", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_float(std::asin(args[0].as_float()));
    });

    exports["acos"] = Value::make_native_fn("acos", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_float(std::acos(args[0].as_float()));
    });

    exports["atan"] = Value::make_native_fn("atan", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_float(std::atan(args[0].as_float()));
    });

    exports["atan2"] = Value::make_native_fn("atan2", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_float(std::atan2(args[0].as_float(), args[1].as_float()));
    });

    exports["sinh"] = Value::make_native_fn("sinh", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_float(std::sinh(args[0].as_float()));
    });

    exports["cosh"] = Value::make_native_fn("cosh", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_float(std::cosh(args[0].as_float()));
    });

    exports["tanh"] = Value::make_native_fn("tanh", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_float(std::tanh(args[0].as_float()));
    });

    exports["pow"] = Value::make_native_fn("pow", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_float(std::pow(args[0].as_float(), args[1].as_float()));
    });

    exports["exp"] = Value::make_native_fn("exp", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_float(std::exp(args[0].as_float()));
    });

    exports["log"] = Value::make_native_fn("log", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_float(std::log(args[0].as_float()));
    });

    exports["log2"] = Value::make_native_fn("log2", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_float(std::log2(args[0].as_float()));
    });

    exports["log10"] = Value::make_native_fn("log10", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_float(std::log10(args[0].as_float()));
    });

    exports["abs"] = Value::make_native_fn("abs", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (args[0].is_int()) return Value::make_int(std::abs(args[0].as_int()));
        return Value::make_float(std::abs(args[0].as_float()));
    });

    exports["floor"] = Value::make_native_fn("floor", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_int(static_cast<int64_t>(std::floor(args[0].as_float())));
    });

    exports["ceil"] = Value::make_native_fn("ceil", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_int(static_cast<int64_t>(std::ceil(args[0].as_float())));
    });

    exports["round"] = Value::make_native_fn("round", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_int(static_cast<int64_t>(std::round(args[0].as_float())));
    });

    exports["trunc"] = Value::make_native_fn("trunc", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_int(static_cast<int64_t>(std::trunc(args[0].as_float())));
    });

    exports["min"] = Value::make_native_fn("min", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (args[0].is_int() && args[1].is_int()) {
            return Value::make_int(std::min(args[0].as_int(), args[1].as_int()));
        }
        return Value::make_float(std::min(args[0].as_float(), args[1].as_float()));
    });

    exports["max"] = Value::make_native_fn("max", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (args[0].is_int() && args[1].is_int()) {
            return Value::make_int(std::max(args[0].as_int(), args[1].as_int()));
        }
        return Value::make_float(std::max(args[0].as_float(), args[1].as_float()));
    });

    exports["clamp"] = Value::make_native_fn("clamp", 3, [](const std::vector<Value>& args, SourceSpan) -> Value {
        double v = args[0].as_float();
        double lo = args[1].as_float();
        double hi = args[2].as_float();
        return Value::make_float(std::clamp(v, lo, hi));
    });

    exports["deg2rad"] = Value::make_native_fn("deg2rad", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_float(args[0].as_float() * (3.14159265358979323846 / 180.0));
    });

    exports["rad2deg"] = Value::make_native_fn("rad2deg", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        return Value::make_float(args[0].as_float() * (180.0 / 3.14159265358979323846));
    });

    return Value::make_object(std::move(exports));
}

Value ModuleManager::create_data_module() {
    Value base_mod = nextviper::create_data_module();
    auto exports = *base_mod.as_object();

    // Backward-compatible extensions
    exports["from_csv"] = exports["read_csv"];
    exports["from_rows"] = Value::make_native_fn("from_rows", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        std::vector<std::string> cols;
        for (const auto& c : *args[0].as_array()) cols.push_back(c.as_string());
        std::vector<std::vector<Value>> rows;
        for (const auto& r : *args[1].as_array()) rows.push_back(*r.as_array());
        return DataFrame(std::move(cols), std::move(rows)).to_value();
    });

    exports["dataframe"] = exports["from_rows"];

    exports["dataloader"] = Value::make_native_fn("dataloader", -1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        if (args.empty() || !args[0].is_object()) throw RuntimeError("data.dataloader requires a Dataset/DataFrame as first argument", span);
        size_t batch_size = args.size() >= 2 ? static_cast<size_t>(args[1].as_int()) : 32;
        bool shuffle = args.size() >= 3 ? args[2].as_bool() : true;
        return DataLoader(Dataset::from_rows({}, {}), batch_size, shuffle).to_value();
    });

    exports["mean"] = Value::make_native_fn("mean", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (!args[0].is_array() || args[0].as_array()->empty()) return Value::make_float(0.0);
        const auto& arr = *args[0].as_array();
        double sum = 0.0;
        for (const auto& x : arr) sum += x.as_float();
        return Value::make_float(sum / static_cast<double>(arr.size()));
    });

    exports["sum"] = Value::make_native_fn("sum", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (!args[0].is_array()) return Value::make_int(0);
        const auto& arr = *args[0].as_array();
        bool all_int = true;
        int64_t isum = 0;
        double fsum = 0.0;
        for (const auto& x : arr) {
            if (!x.is_int()) all_int = false;
            isum += x.as_int();
            fsum += x.as_float();
        }
        return all_int ? Value::make_int(isum) : Value::make_float(fsum);
    });

    exports["chunk"] = Value::make_native_fn("chunk", 2, [](const std::vector<Value>& args, SourceSpan) -> Value {
        if (!args[0].is_array() || !args[1].is_int()) return Value::make_array({});
        const auto& arr = *args[0].as_array();
        int64_t size = std::max<int64_t>(1, args[1].as_int());
        std::vector<Value> chunks;
        std::vector<Value> cur;
        for (const auto& item : arr) {
            cur.push_back(item);
            if (cur.size() == static_cast<size_t>(size)) {
                chunks.push_back(Value::make_array(std::move(cur)));
                cur.clear();
            }
        }
        if (!cur.empty()) chunks.push_back(Value::make_array(std::move(cur)));
        return Value::make_array(std::move(chunks));
    });

    return Value::make_object(std::move(exports));
}

Value ModuleManager::create_ai_module() {
    return create_ai_subsystem_module();
}

Value ModuleManager::create_tensor_module() {
    return create_tensor_subsystem_module();
}

Value ModuleManager::create_sys_module() {
    std::map<std::string, Value> exports;
    exports["version"] = Value::make_string(std::string(VERSION_STRING));
#if defined(_WIN32)
    exports["platform"] = Value::make_string("windows");
#elif defined(__APPLE__)
    exports["platform"] = Value::make_string("macos");
#else
    exports["platform"] = Value::make_string("linux");
#endif
    exports["args"] = Value::make_array({});
    return Value::make_object(std::move(exports));
}

Value ModuleManager::create_time_module() {
    std::map<std::string, Value> exports;

    exports["now"] = Value::make_native_fn("now", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        double sec = std::chrono::duration_cast<std::chrono::duration<double>>(now).count();
        return Value::make_float(sec);
    });

    exports["now_ms"] = Value::make_native_fn("now_ms", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        return Value::make_int(ms);
    });
    exports["timestamp_ms"] = exports["now_ms"];

    exports["sleep"] = Value::make_native_fn("sleep", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        int64_t ms = args[0].is_int() ? args[0].as_int() : static_cast<int64_t>(args[0].as_float() * 1000.0);
        if (ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        return Value::make_nil();
    });

    exports["elapsed"] = Value::make_native_fn("elapsed", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        double start_time = args[0].as_float();
        auto now = std::chrono::system_clock::now().time_since_epoch();
        double current = std::chrono::duration_cast<std::chrono::duration<double>>(now).count();
        return Value::make_float(std::max(0.0, current - start_time));
    });

    exports["elapsed_ms"] = Value::make_native_fn("elapsed_ms", 1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        double start_time = args[0].as_float();
        auto now = std::chrono::system_clock::now().time_since_epoch();
        double current = std::chrono::duration_cast<std::chrono::duration<double>>(now).count();
        return Value::make_float(std::max(0.0, (current - start_time) * 1000.0));
    });

    exports["format"] = Value::make_native_fn("format", -1, [](const std::vector<Value>& args, SourceSpan) -> Value {
        time_t t = args.empty() ? std::time(nullptr) : static_cast<time_t>(args[0].as_float());
        std::string fmt = args.size() >= 2 ? args[1].as_string() : "%Y-%m-%d %H:%M:%S";
        char buf[128];
        struct tm* tm_info = std::localtime(&t);
        if (tm_info && std::strftime(buf, sizeof(buf), fmt.c_str(), tm_info)) {
            return Value::make_string(std::string(buf));
        }
        return Value::make_string("");
    });

    return Value::make_object(std::move(exports));
}

Value ModuleManager::create_net_module() {
    std::map<std::string, Value> exports;

    exports["http_server"] = Value::make_native_fn("http_server", 0, [](const std::vector<Value>&, SourceSpan) -> Value {
        struct Route {
            std::string method;
            std::string path_pattern;
            Value handler;
        };

        struct ServerState {
            std::vector<Route> routes;
            std::vector<Value> middlewares;
            std::map<std::string, std::string> static_dirs;
            std::atomic<bool> running{false};
            int server_fd = -1;
            std::unique_ptr<std::thread> listener_thread;
        };

        auto state = std::make_shared<ServerState>();
        std::map<std::string, Value> app;

        auto add_route = [state](const std::string& method, const std::vector<Value>& args, SourceSpan s) -> Value {
            if (args.size() < 2) throw RuntimeError("route registration requires path and handler function", s);
            state->routes.push_back({method, args[0].as_string(), args[1]});
            return Value::make_nil();
        };

        app["get"] = Value::make_native_fn("get", 2, [add_route](const std::vector<Value>& a, SourceSpan s) { return add_route("GET", a, s); });
        app["post"] = Value::make_native_fn("post", 2, [add_route](const std::vector<Value>& a, SourceSpan s) { return add_route("POST", a, s); });
        app["put"] = Value::make_native_fn("put", 2, [add_route](const std::vector<Value>& a, SourceSpan s) { return add_route("PUT", a, s); });
        app["delete"] = Value::make_native_fn("delete", 2, [add_route](const std::vector<Value>& a, SourceSpan s) { return add_route("DELETE", a, s); });
        app["patch"] = Value::make_native_fn("patch", 2, [add_route](const std::vector<Value>& a, SourceSpan s) { return add_route("PATCH", a, s); });

        app["use"] = Value::make_native_fn("use", 1, [state](const std::vector<Value>& a, SourceSpan) -> Value {
            if (!a.empty()) state->middlewares.push_back(a[0]);
            return Value::make_nil();
        });

        app["static"] = Value::make_native_fn("static", 2, [state](const std::vector<Value>& a, SourceSpan) -> Value {
            if (a.size() >= 2) {
                state->static_dirs[a[0].as_string()] = a[1].as_string();
            }
            return Value::make_nil();
        });

        auto match_route = [](const std::string& pattern, const std::string& path, std::map<std::string, Value>& params) -> bool {
            if (pattern == path) return true;
            std::vector<std::string> pat_parts, path_parts;
            std::stringstream ss_pat(pattern), ss_path(path);
            std::string part;
            while (std::getline(ss_pat, part, '/')) if (!part.empty()) pat_parts.push_back(part);
            while (std::getline(ss_path, part, '/')) if (!part.empty()) path_parts.push_back(part);

            if (pat_parts.size() != path_parts.size()) return false;
            for (size_t i = 0; i < pat_parts.size(); ++i) {
                if (pat_parts[i].rfind(":", 0) == 0) {
                    params[pat_parts[i].substr(1)] = Value::make_string(path_parts[i]);
                } else if (pat_parts[i] != path_parts[i]) {
                    return false;
                }
            }
            return true;
        };

        auto invoke_val_func = [](const Value& fn, const std::vector<Value>& fn_args) -> Value {
            if (fn.type() == ValueType::NATIVE_FUNCTION) {
                return fn.as_native_fn()->func(fn_args, SourceSpan{});
            }
            if (fn.type() == ValueType::FUNCTION) {
                SourceManager sm;
                DiagnosticEngine diag(sm, false);
                Interpreter interp(diag);
                return interp.call_function(fn, fn_args, SourceSpan{});
            }
            return Value::make_nil();
        };

        app["close"] = Value::make_native_fn("close", 0, [state](const std::vector<Value>&, SourceSpan) -> Value {
            state->running = false;
            if (state->server_fd >= 0) {
#if defined(_WIN32)
                closesocket(state->server_fd);
#else
                close(state->server_fd);
#endif
                state->server_fd = -1;
            }
            if (state->listener_thread && state->listener_thread->joinable()) {
                state->listener_thread->detach();
            }
            return Value::make_bool(true);
        });

        app["listen"] = Value::make_native_fn("listen", -1, [state, match_route, invoke_val_func](const std::vector<Value>& args, SourceSpan span) -> Value {
            int port = args.empty() ? 8080 : static_cast<int>(args[0].as_int());
            std::string host = args.size() >= 2 ? args[1].as_string() : "0.0.0.0";
            bool background = args.size() >= 3 ? args[2].as_bool() : false;

#if defined(_WIN32)
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
            int fd = socket(AF_INET, SOCK_STREAM, 0);
            if (fd < 0) throw RuntimeError("Failed to create server TCP socket", span);

            int opt = 1;
#if defined(_WIN32)
            setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
            setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(static_cast<uint16_t>(port));
            inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

            if (bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
#if defined(_WIN32)
                closesocket(fd);
#else
                close(fd);
#endif
                throw RuntimeError("Failed to bind HTTP server to " + host + ":" + std::to_string(port), span);
            }

            if (::listen(fd, 128) < 0) {
#if defined(_WIN32)
                closesocket(fd);
#else
                close(fd);
#endif
                throw RuntimeError("Failed to listen on socket", span);
            }

            state->server_fd = fd;
            state->running = true;

            auto serve_loop = [state, match_route, invoke_val_func, fd]() {
                while (state->running) {
                    sockaddr_in client_addr{};
                    socklen_t client_len = sizeof(client_addr);
                    int client_fd = accept(fd, (sockaddr*)&client_addr, &client_len);
                    if (client_fd < 0) {
                        if (!state->running) break;
                        continue;
                    }

                    // Read request
                    std::string raw_req;
                    char buf[4096];
                    int n = recv(client_fd, buf, sizeof(buf) - 1, 0);
                    if (n > 0) {
                        buf[n] = '\0';
                        raw_req.append(buf, n);
                    }

                    if (raw_req.empty()) {
#if defined(_WIN32)
                        closesocket(client_fd);
#else
                        close(client_fd);
#endif
                        continue;
                    }

                    // Parse HTTP request line
                    std::istringstream req_stream(raw_req);
                    std::string method, full_path, proto;
                    req_stream >> method >> full_path >> proto;

                    std::string path = full_path;
                    std::string query_str;
                    size_t qpos = full_path.find('?');
                    if (qpos != std::string::npos) {
                        path = full_path.substr(0, qpos);
                        query_str = full_path.substr(qpos + 1);
                    }

                    // Parse query params
                    std::map<std::string, Value> query_map;
                    if (!query_str.empty()) {
                        std::stringstream qss(query_str);
                        std::string pair;
                        while (std::getline(qss, pair, '&')) {
                            size_t eq = pair.find('=');
                            if (eq != std::string::npos) {
                                query_map[pair.substr(0, eq)] = Value::make_string(pair.substr(eq + 1));
                            } else {
                                query_map[pair] = Value::make_string("");
                            }
                        }
                    }

                    // Parse headers
                    std::map<std::string, Value> headers_map;
                    std::string line;
                    std::getline(req_stream, line); // finish request line
                    while (std::getline(req_stream, line) && line != "\r" && !line.empty()) {
                        size_t colon = line.find(':');
                        if (colon != std::string::npos) {
                            std::string k = line.substr(0, colon);
                            std::string v = line.substr(colon + 1);
                            // trim v
                            size_t start = v.find_first_not_of(" \t");
                            size_t end = v.find_last_not_of(" \t\r\n");
                            if (start != std::string::npos && end != std::string::npos) {
                                v = v.substr(start, end - start + 1);
                            }
                            std::transform(k.begin(), k.end(), k.begin(), ::tolower);
                            headers_map[k] = Value::make_string(v);
                        }
                    }

                    // Read body
                    size_t body_pos = raw_req.find("\r\n\r\n");
                    std::string body;
                    if (body_pos != std::string::npos) {
                        body = raw_req.substr(body_pos + 4);
                    }

                    // CORS preflight
                    if (method == "OPTIONS") {
                        std::string resp = "HTTP/1.1 204 No Content\r\n"
                                           "Access-Control-Allow-Origin: *\r\n"
                                           "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, PATCH, OPTIONS\r\n"
                                           "Access-Control-Allow-Headers: *\r\n"
                                           "Content-Length: 0\r\n"
                                           "Connection: close\r\n\r\n";
                        send(client_fd, resp.data(), resp.size(), 0);
#if defined(_WIN32)
                        closesocket(client_fd);
#else
                        close(client_fd);
#endif
                        continue;
                    }

                    // Check static file routes
                    bool handled_static = false;
                    for (const auto& [prefix, dir] : state->static_dirs) {
                        if (path.rfind(prefix, 0) == 0) {
                            std::string rel_path = path.substr(prefix.size());
                            if (rel_path.empty() || rel_path == "/") rel_path = "/index.html";
                            fs::path full_file = fs::path(dir) / rel_path.substr(1);
                            if (fs::exists(full_file) && !fs::is_directory(full_file)) {
                                std::ifstream f(full_file, std::ios::binary);
                                std::string fcontent((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                                std::string ctype = "text/plain";
                                std::string ext = full_file.extension().string();
                                if (ext == ".html" || ext == ".htm") ctype = "text/html; charset=utf-8";
                                else if (ext == ".js") ctype = "application/javascript";
                                else if (ext == ".css") ctype = "text/css";
                                else if (ext == ".json") ctype = "application/json";
                                else if (ext == ".png") ctype = "image/png";
                                else if (ext == ".svg") ctype = "image/svg+xml";

                                std::string resp = "HTTP/1.1 200 OK\r\n"
                                                   "Content-Type: " + ctype + "\r\n"
                                                   "Content-Length: " + std::to_string(fcontent.size()) + "\r\n"
                                                   "Access-Control-Allow-Origin: *\r\n"
                                                   "Connection: close\r\n\r\n" + fcontent;
                                send(client_fd, resp.data(), resp.size(), 0);
                                handled_static = true;
                                break;
                            }
                        }
                    }

                    if (handled_static) {
#if defined(_WIN32)
                        closesocket(client_fd);
#else
                        close(client_fd);
#endif
                        continue;
                    }

                    // Match dynamic routes
                    bool matched = false;
                    for (const auto& route : state->routes) {
                        if (route.method == method || route.method == "*") {
                            std::map<std::string, Value> params_map;
                            if (match_route(route.path_pattern, path, params_map)) {
                                matched = true;

                                // Build Request Value
                                std::map<std::string, Value> req_obj;
                                req_obj["method"] = Value::make_string(method);
                                req_obj["path"] = Value::make_string(path);
                                req_obj["query"] = Value::make_object(std::move(query_map));
                                req_obj["params"] = Value::make_object(std::move(params_map));
                                req_obj["headers"] = Value::make_object(std::move(headers_map));
                                req_obj["body"] = Value::make_string(body);
                                req_obj["json"] = Value::make_native_fn("json", 0, [body](const std::vector<Value>&, SourceSpan s) -> Value {
                                    try {
                                        JsonParser parser(body);
                                        return parser.parse();
                                    } catch (const std::exception& e) {
                                        throw RuntimeError(std::string("req.json() parsing failed: ") + e.what(), s);
                                    }
                                });

                                Value req_val = Value::make_object(std::move(req_obj));

                                try {
                                    // Execute middlewares first
                                    for (const auto& mw : state->middlewares) {
                                        invoke_val_func(mw, {req_val});
                                    }

                                    // Execute route handler
                                    Value handler_res = invoke_val_func(route.handler, {req_val});

                                    int status_code = 200;
                                    std::string resp_body;
                                    std::string ctype = "application/json; charset=utf-8";
                                    std::map<std::string, std::string> custom_headers;

                                    if (handler_res.is_object() && handler_res.as_object()->count("status") && handler_res.as_object()->count("body")) {
                                        auto robj = *handler_res.as_object();
                                        status_code = static_cast<int>(robj["status"].as_int());
                                        if (robj["body"].is_string()) {
                                            resp_body = robj["body"].as_string();
                                        } else {
                                            resp_body = json_stringify_val(robj["body"], 0, 0);
                                        }
                                        if (robj.count("content_type")) ctype = robj["content_type"].as_string();
                                        if (robj.count("headers") && robj["headers"].is_object()) {
                                            for (const auto& [hk, hv] : *robj["headers"].as_object()) {
                                                custom_headers[hk] = hv.as_string();
                                            }
                                        }
                                    } else if (handler_res.is_string()) {
                                        resp_body = handler_res.as_string();
                                        ctype = "text/plain; charset=utf-8";
                                    } else {
                                        resp_body = json_stringify_val(handler_res, 0, 0);
                                    }

                                    std::string header_str;
                                    for (const auto& [hk, hv] : custom_headers) {
                                        header_str += hk + ": " + hv + "\r\n";
                                    }

                                    std::string resp = "HTTP/1.1 " + std::to_string(status_code) + " OK\r\n"
                                                       "Content-Type: " + ctype + "\r\n"
                                                       "Content-Length: " + std::to_string(resp_body.size()) + "\r\n"
                                                       "Access-Control-Allow-Origin: *\r\n" +
                                                       header_str +
                                                       "Connection: close\r\n\r\n" + resp_body;
                                    send(client_fd, resp.data(), resp.size(), 0);
                                } catch (const std::exception& e) {
                                    std::string err_json = "{\"error\":\"Internal Server Error\",\"status\":500,\"message\":\"" + std::string(e.what()) + "\"}";
                                    std::string resp = "HTTP/1.1 500 Internal Server Error\r\n"
                                                       "Content-Type: application/json\r\n"
                                                       "Content-Length: " + std::to_string(err_json.size()) + "\r\n"
                                                       "Access-Control-Allow-Origin: *\r\n"
                                                       "Connection: close\r\n\r\n" + err_json;
                                    send(client_fd, resp.data(), resp.size(), 0);
                                }
                                break;
                            }
                        }
                    }

                    if (!matched) {
                        std::string nf_json = "{\"error\":\"Not Found\",\"status\":404,\"path\":\"" + path + "\"}";
                        std::string resp = "HTTP/1.1 404 Not Found\r\n"
                                           "Content-Type: application/json\r\n"
                                           "Content-Length: " + std::to_string(nf_json.size()) + "\r\n"
                                           "Access-Control-Allow-Origin: *\r\n"
                                           "Connection: close\r\n\r\n" + nf_json;
                        send(client_fd, resp.data(), resp.size(), 0);
                    }

#if defined(_WIN32)
                    closesocket(client_fd);
#else
                    close(client_fd);
#endif
                }
            };

            if (background) {
                state->listener_thread = std::make_unique<std::thread>(serve_loop);
            } else {
                serve_loop();
            }

            return Value::make_bool(true);
        });

        return Value::make_object(std::move(app));
    });

    exports["server"] = exports["http_server"];
    exports["app"] = exports["http_server"];
    return Value::make_object(std::move(exports));
}

Value ModuleManager::create_db_module() {
    std::map<std::string, Value> exports;

    exports["postgres"] = Value::make_native_fn("postgres", -1, [](const std::vector<Value>& args, SourceSpan span) -> Value {
        std::string conn_str;
        if (!args.empty() && args[0].is_object()) {
            auto obj = *args[0].as_object();
            std::string h = obj.count("host") ? obj["host"].as_string() : "localhost";
            int p = obj.count("port") ? static_cast<int>(obj["port"].as_int()) : 5432;
            std::string db = obj.count("database") ? obj["database"].as_string() : (obj.count("dbname") ? obj["dbname"].as_string() : "postgres");
            std::string u = obj.count("user") ? obj["user"].as_string() : (obj.count("username") ? obj["username"].as_string() : "postgres");
            std::string pw = obj.count("password") ? obj["password"].as_string() : "";
            conn_str = "host=" + h + " port=" + std::to_string(p) + " dbname=" + db + " user=" + u;
            if (!pw.empty()) conn_str += " password=" + pw;
            if (obj.count("sslmode")) conn_str += " sslmode=" + obj["sslmode"].as_string();
        } else if (!args.empty() && args[0].is_string()) {
            conn_str = args[0].as_string();
        } else {
            conn_str = "host=localhost port=5432 dbname=postgres user=postgres";
        }

        PGconn* raw_conn = PQconnectdb(conn_str.c_str());
        if (!raw_conn || PQstatus(raw_conn) != CONNECTION_OK) {
            std::string err_msg = raw_conn ? PQerrorMessage(raw_conn) : "Failed to allocate PostgreSQL connection handle";
            if (raw_conn) PQfinish(raw_conn);
            while (!err_msg.empty() && (err_msg.back() == '\n' || err_msg.back() == '\r')) err_msg.pop_back();
            throw RuntimeError("PostgreSQL Connection Error: " + err_msg, span, "Ensure the PostgreSQL server is running and connection parameters are valid");
        }

        struct RealPGState {
            PGconn* conn = nullptr;
            std::mutex mtx;
            bool closed = false;

            ~RealPGState() {
                std::lock_guard<std::mutex> lock(mtx);
                if (conn && !closed) {
                    PQfinish(conn);
                    conn = nullptr;
                    closed = true;
                }
            }
        };

        auto state = std::make_shared<RealPGState>();
        state->conn = raw_conn;

        std::map<std::string, Value> client;

        // query(sql, [params]) -> {rows: [...], count: N, fields: [...], sql: "..."}
        client["query"] = Value::make_native_fn("query", -1, [state](const std::vector<Value>& a, SourceSpan s) -> Value {
            if (a.empty()) throw RuntimeError("db.query requires SQL query string", s);
            std::string sql = a[0].as_string();
            std::vector<Value> params;
            if (a.size() >= 2 && a[1].is_array()) params = *a[1].as_array();

            std::lock_guard<std::mutex> lock(state->mtx);
            if (state->closed || !state->conn) {
                throw RuntimeError("PostgreSQL Error: query executed on closed database connection", s);
            }

            std::vector<std::string> param_strings;
            std::vector<const char*> param_ptrs;
            for (const auto& p : params) {
                if (p.is_nil()) {
                    param_ptrs.push_back(nullptr);
                } else {
                    param_strings.push_back(p.is_string() ? p.as_string() : p.to_string());
                    param_ptrs.push_back(param_strings.back().c_str());
                }
            }

            PGresult* res = nullptr;
            if (param_ptrs.empty()) {
                res = PQexec(state->conn, sql.c_str());
            } else {
                res = PQexecParams(state->conn, sql.c_str(), static_cast<int>(param_ptrs.size()),
                                   nullptr, param_ptrs.data(), nullptr, nullptr, 0);
            }

            if (!res) {
                std::string err = PQerrorMessage(state->conn);
                while (!err.empty() && (err.back() == '\n' || err.back() == '\r')) err.pop_back();
                throw RuntimeError("PostgreSQL Query Execution Failed: " + err, s);
            }

            ExecStatusType status = PQresultStatus(res);
            if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
                std::string err = PQresultErrorMessage(res);
                if (err.empty()) err = PQerrorMessage(state->conn);
                while (!err.empty() && (err.back() == '\n' || err.back() == '\r')) err.pop_back();
                PQclear(res);
                throw RuntimeError("PostgreSQL Query Error: " + err, s);
            }

            int nfields = PQnfields(res);
            std::vector<Value> fields;
            std::vector<std::string> col_names;
            for (int i = 0; i < nfields; ++i) {
                const char* fname = PQfname(res, i);
                std::string col = fname ? fname : ("col_" + std::to_string(i));
                col_names.push_back(col);
                fields.push_back(Value::make_string(col));
            }

            int ntuples = PQntuples(res);
            std::vector<Value> rows;
            for (int r = 0; r < ntuples; ++r) {
                std::map<std::string, Value> row_map;
                for (int c = 0; c < nfields; ++c) {
                    if (PQgetisnull(res, r, c)) {
                        row_map[col_names[c]] = Value::make_nil();
                    } else {
                        const char* val_str = PQgetvalue(res, r, c);
                        row_map[col_names[c]] = Value::make_string(val_str ? val_str : "");
                    }
                }
                rows.push_back(Value::make_object(std::move(row_map)));
            }

            PQclear(res);

            std::map<std::string, Value> out_obj;
            out_obj["rows"] = Value::make_array(std::move(rows));
            out_obj["count"] = Value::make_int(ntuples);
            out_obj["fields"] = Value::make_array(std::move(fields));
            out_obj["sql"] = Value::make_string(sql);
            return Value::make_object(std::move(out_obj));
        });

        // execute(sql, [params]) -> {affected_rows: N, status: "OK"}
        client["execute"] = Value::make_native_fn("execute", -1, [state](const std::vector<Value>& a, SourceSpan s) -> Value {
            if (a.empty()) throw RuntimeError("db.execute requires SQL statement", s);
            std::string sql = a[0].as_string();
            std::vector<Value> params;
            if (a.size() >= 2 && a[1].is_array()) params = *a[1].as_array();

            std::lock_guard<std::mutex> lock(state->mtx);
            if (state->closed || !state->conn) {
                throw RuntimeError("PostgreSQL Error: execute called on closed database connection", s);
            }

            std::vector<std::string> param_strings;
            std::vector<const char*> param_ptrs;
            for (const auto& p : params) {
                if (p.is_nil()) {
                    param_ptrs.push_back(nullptr);
                } else {
                    param_strings.push_back(p.is_string() ? p.as_string() : p.to_string());
                    param_ptrs.push_back(param_strings.back().c_str());
                }
            }

            PGresult* res = nullptr;
            if (param_ptrs.empty()) {
                res = PQexec(state->conn, sql.c_str());
            } else {
                res = PQexecParams(state->conn, sql.c_str(), static_cast<int>(param_ptrs.size()),
                                   nullptr, param_ptrs.data(), nullptr, nullptr, 0);
            }

            if (!res) {
                std::string err = PQerrorMessage(state->conn);
                while (!err.empty() && (err.back() == '\n' || err.back() == '\r')) err.pop_back();
                throw RuntimeError("PostgreSQL Execute Failed: " + err, s);
            }

            ExecStatusType status = PQresultStatus(res);
            if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
                std::string err = PQresultErrorMessage(res);
                if (err.empty()) err = PQerrorMessage(state->conn);
                while (!err.empty() && (err.back() == '\n' || err.back() == '\r')) err.pop_back();
                PQclear(res);
                throw RuntimeError("PostgreSQL Execute Error: " + err, s);
            }

            const char* cmd_tuples = PQcmdTuples(res);
            int64_t affected = 0;
            if (cmd_tuples && *cmd_tuples) {
                try { affected = std::stoll(cmd_tuples); } catch (...) { affected = 0; }
            }
            PQclear(res);

            std::map<std::string, Value> out_obj;
            out_obj["affected_rows"] = Value::make_int(affected);
            out_obj["status"] = Value::make_string("OK");
            return Value::make_object(std::move(out_obj));
        });

        // transaction(callback) -> executes BEGIN, callback(client), COMMIT or ROLLBACK on error
        client["transaction"] = Value::make_native_fn("transaction", 1, [state, client](const std::vector<Value>& a, SourceSpan s) -> Value {
            if (a.empty() || !a[0].is_callable()) throw RuntimeError("db.transaction requires a callable callback function", s);
            Value cb = a[0];

            {
                std::lock_guard<std::mutex> lock(state->mtx);
                PGresult* res = PQexec(state->conn, "BEGIN");
                if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                    std::string err = PQerrorMessage(state->conn);
                    PQclear(res);
                    throw RuntimeError("PostgreSQL BEGIN Transaction Failed: " + err, s);
                }
                PQclear(res);
            }

            auto invoke_val_func = [](const Value& fn, const std::vector<Value>& fn_args) -> Value {
                if (fn.type() == ValueType::NATIVE_FUNCTION) {
                    return fn.as_native_fn()->func(fn_args, SourceSpan{});
                }
                if (fn.type() == ValueType::FUNCTION) {
                    SourceManager sm;
                    DiagnosticEngine diag(sm, false);
                    Interpreter interp(diag);
                    return interp.call_function(fn, fn_args, SourceSpan{});
                }
                return Value::make_nil();
            };

            Value result = Value::make_nil();
            try {
                result = invoke_val_func(cb, {Value::make_object(client)});
                std::lock_guard<std::mutex> lock(state->mtx);
                PGresult* res = PQexec(state->conn, "COMMIT");
                if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                    std::string err = PQerrorMessage(state->conn);
                    PQclear(res);
                    throw RuntimeError("PostgreSQL COMMIT Failed: " + err, s);
                }
                PQclear(res);
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(state->mtx);
                PGresult* res = PQexec(state->conn, "ROLLBACK");
                PQclear(res);
                throw;
            }

            return result;
        });

        // close() -> closes database connection
        client["close"] = Value::make_native_fn("close", 0, [state](const std::vector<Value>&, SourceSpan) -> Value {
            std::lock_guard<std::mutex> lock(state->mtx);
            if (!state->closed && state->conn) {
                PQfinish(state->conn);
                state->conn = nullptr;
                state->closed = true;
            }
            return Value::make_bool(true);
        });

        return Value::make_object(std::move(client));
    });

    exports["connect"] = exports["postgres"];
    exports["pool"] = exports["postgres"];
    return Value::make_object(std::move(exports));
}

Value ModuleManager::create_log_module() {
    std::map<std::string, Value> exports;

    static std::string current_level = "info";
    static std::mutex log_mtx;

    auto format_log = [](const std::string& level, const std::string& msg, const Value& meta) {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
        std::string ts = ss.str();

        std::string meta_str = "";
        if (meta.is_object() || meta.is_array()) {
            meta_str = " " + json_stringify_val(meta, 0, 0);
        }

        std::lock_guard<std::mutex> lock(log_mtx);
        std::cout << "[" << ts << "] [" << level << "] " << msg << meta_str << std::endl;
    };

    exports["set_level"] = Value::make_native_fn("set_level", 1, [](const std::vector<Value>& a, SourceSpan) -> Value {
        current_level = a[0].as_string();
        return Value::make_nil();
    });

    exports["info"] = Value::make_native_fn("info", -1, [format_log](const std::vector<Value>& a, SourceSpan) -> Value {
        if (a.empty()) return Value::make_nil();
        std::string msg = a[0].as_string();
        Value meta = a.size() >= 2 ? a[1] : Value::make_nil();
        format_log("INFO", msg, meta);
        return Value::make_nil();
    });

    exports["warn"] = Value::make_native_fn("warn", -1, [format_log](const std::vector<Value>& a, SourceSpan) -> Value {
        if (a.empty()) return Value::make_nil();
        std::string msg = a[0].as_string();
        Value meta = a.size() >= 2 ? a[1] : Value::make_nil();
        format_log("WARN", msg, meta);
        return Value::make_nil();
    });

    exports["error"] = Value::make_native_fn("error", -1, [format_log](const std::vector<Value>& a, SourceSpan) -> Value {
        if (a.empty()) return Value::make_nil();
        std::string msg = a[0].as_string();
        Value meta = a.size() >= 2 ? a[1] : Value::make_nil();
        format_log("ERROR", msg, meta);
        return Value::make_nil();
    });

    exports["debug"] = Value::make_native_fn("debug", -1, [format_log](const std::vector<Value>& a, SourceSpan) -> Value {
        if (current_level != "debug") return Value::make_nil();
        if (a.empty()) return Value::make_nil();
        std::string msg = a[0].as_string();
        Value meta = a.size() >= 2 ? a[1] : Value::make_nil();
        format_log("DEBUG", msg, meta);
        return Value::make_nil();
    });

    return Value::make_object(std::move(exports));
}

Value ModuleManager::create_env_module() {
    std::map<std::string, Value> exports;

    exports["get"] = Value::make_native_fn("get", -1, [](const std::vector<Value>& a, SourceSpan s) -> Value {
        if (a.empty()) throw RuntimeError("env.get requires key name", s);
        std::string key = a[0].as_string();
        const char* val = std::getenv(key.c_str());
        if (val) return Value::make_string(val);
        return a.size() >= 2 ? a[1] : Value::make_nil();
    });

    exports["set"] = Value::make_native_fn("set", 2, [](const std::vector<Value>& a, SourceSpan) -> Value {
        std::string key = a[0].as_string();
        std::string val = a[1].as_string();
#if defined(_WIN32)
        _putenv_s(key.c_str(), val.c_str());
#else
        setenv(key.c_str(), val.c_str(), 1);
#endif
        return Value::make_bool(true);
    });

    exports["require"] = Value::make_native_fn("require", 1, [](const std::vector<Value>& a, SourceSpan s) -> Value {
        std::string key = a[0].as_string();
        const char* val = std::getenv(key.c_str());
        if (!val) {
            throw RuntimeError("Missing required environment variable: '" + key + "'. Configure in .env or system environment.", s);
        }
        return Value::make_string(val);
    });

    exports["load"] = Value::make_native_fn("load", -1, [](const std::vector<Value>& a, SourceSpan) -> Value {
        std::string path = a.empty() ? ".env" : a[0].as_string();
        std::ifstream file(path);
        if (!file.is_open()) return Value::make_bool(false);

        std::string line;
        while (std::getline(file, line)) {
            size_t start = line.find_first_not_of(" \t\r\n");
            if (start == std::string::npos || line[start] == '#') continue;
            size_t eq = line.find('=', start);
            if (eq != std::string::npos) {
                std::string k = line.substr(start, eq - start);
                std::string v = line.substr(eq + 1);
                if (v.size() >= 2 && ((v.front() == '"' && v.back() == '"') || (v.front() == '\'' && v.back() == '\''))) {
                    v = v.substr(1, v.size() - 2);
                }
#if defined(_WIN32)
                _putenv_s(k.c_str(), v.c_str());
#else
                setenv(k.c_str(), v.c_str(), 1);
#endif
            }
        }
        return Value::make_bool(true);
    });

    exports["load_dotenv"] = exports["load"];
    return Value::make_object(std::move(exports));
}

// ============================================================================
// File-Based Module Resolution
// ============================================================================

std::optional<std::string> ModuleManager::resolve_module_path(const std::string& module_spec, const std::string& current_file) {
    if (is_builtin(module_spec)) return module_spec;

    std::vector<std::string> candidate_paths;

    std::string base_dir = ".";
    if (!current_file.empty() && current_file != "<eval>" && current_file != "<repl>") {
        try {
            fs::path p(current_file);
            if (p.has_parent_path()) base_dir = p.parent_path().string();
        } catch (...) {}
    }

    auto add_candidates = [&](const fs::path& base) {
        fs::path p = base / module_spec;
        candidate_paths.push_back(p.string());
        candidate_paths.push_back((p.string() + ".nv"));
        candidate_paths.push_back((p / "mod.nv").string());
        candidate_paths.push_back((p / "main.nv").string());
        candidate_paths.push_back((p / "src" / "main.nv").string());
        candidate_paths.push_back((p / "src" / "lib.nv").string());
        candidate_paths.push_back((p / "lib.nv").string());
        candidate_paths.push_back((p / "index.nv").string());
    };

    if (module_spec.rfind("./", 0) == 0 || module_spec.rfind("../", 0) == 0 || module_spec.rfind("/", 0) == 0) {
        add_candidates(fs::path(base_dir));
    } else {
        add_candidates(fs::path(base_dir));
        for (const auto& sp : search_paths_) add_candidates(fs::path(sp));
    }

    for (const auto& candidate : candidate_paths) {
        std::error_code ec;
        if (fs::exists(candidate, ec) && !fs::is_directory(candidate, ec)) {
            try {
                return fs::canonical(candidate).string();
            } catch (...) {
                return candidate;
            }
        }
    }

    return std::nullopt;
}

std::optional<Value> ModuleManager::load_module(const std::string& module_spec, const std::string& current_file, Interpreter& interpreter) {
    if (is_builtin(module_spec)) {
        return get_builtin_module(module_spec);
    }

    auto resolved_path = resolve_module_path(module_spec, current_file);
    if (!resolved_path) {
        diagnostics_.error("cannot find module '" + module_spec + "'", SourceSpan{},
                           "ensure the file exists and is located in the search path or current directory");
        return std::nullopt;
    }

    const std::string& target_path = *resolved_path;

    auto it = module_cache_.find(target_path);
    if (it != module_cache_.end()) return it->second;

    if (loading_modules_.count(target_path)) {
        diagnostics_.error("circular dependency detected importing '" + module_spec + "'", SourceSpan{},
                           "module '" + target_path + "' is already in the loading chain");
        return std::nullopt;
    }

    loading_modules_.insert(target_path);

    std::ifstream file(target_path);
    if (!file.is_open()) {
        loading_modules_.erase(target_path);
        diagnostics_.error("failed to open module file: " + target_path, SourceSpan{});
        return std::nullopt;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    Lexer lexer(source, target_path, diagnostics_);
    auto tokens = lexer.tokenize();
    if (diagnostics_.has_errors()) {
        loading_modules_.erase(target_path);
        return std::nullopt;
    }

    Parser parser(tokens, diagnostics_);
    auto program = parser.parse_program();
    if (diagnostics_.has_errors() || !program) {
        loading_modules_.erase(target_path);
        return std::nullopt;
    }

    loaded_ast_programs_.push_back(std::move(program));
    const auto& loaded_program = *loaded_ast_programs_.back();

    auto module_env = Environment::create(interpreter.globals());
    auto prev_env = interpreter.environment();
    auto prev_file = interpreter.current_file();
    interpreter.set_environment(module_env);
    interpreter.set_current_file(target_path);

    try {
        interpreter.execute(loaded_program);
    } catch (...) {
        interpreter.set_environment(prev_env);
        interpreter.set_current_file(prev_file);
        loading_modules_.erase(target_path);
        throw;
    }

    interpreter.set_environment(prev_env);
    interpreter.set_current_file(prev_file);
    loading_modules_.erase(target_path);

    std::map<std::string, Value> exports;
    for (const auto& [name, binding] : module_env->bindings()) {
        if (name.rfind("$", 0) != 0) {
            exports[name] = binding.value;
        }
    }

    Value mod_obj = Value::make_object(std::move(exports));
    module_cache_[target_path] = mod_obj;
    return mod_obj;
}

} // namespace nextviper
