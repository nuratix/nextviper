#pragma once

#include "nextviper/opcode.hpp"
#include "nextviper/value.hpp"
#include <vector>
#include <cstdint>
#include <string_view>

namespace nextviper {

class Chunk {
public:
    Chunk() = default;

    void write(uint8_t byte, size_t line);
    void write_opcode(OpCode op, size_t line);
    size_t add_constant(Value value);

    const std::vector<uint8_t>& code() const { return code_; }
    const std::vector<Value>& constants() const { return constants_; }
    const std::vector<size_t>& lines() const { return lines_; }

    uint8_t& byte_at(size_t index) { return code_[index]; }
    uint8_t byte_at(size_t index) const { return code_[index]; }

    size_t count() const { return code_.size(); }

    void disassemble(std::string_view name) const;
    size_t disassemble_instruction(size_t offset) const;

private:
    std::vector<uint8_t> code_;
    std::vector<Value> constants_;
    std::vector<size_t> lines_;
};

} // namespace nextviper
