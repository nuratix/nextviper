#include "nextviper/chunk.hpp"
#include <iostream>
#include <iomanip>

namespace nextviper {

std::string_view opcode_to_string(OpCode op) {
    switch (op) {
        case OpCode::OP_CONSTANT: return "OP_CONSTANT";
        case OpCode::OP_NIL: return "OP_NIL";
        case OpCode::OP_TRUE: return "OP_TRUE";
        case OpCode::OP_FALSE: return "OP_FALSE";
        case OpCode::OP_POP: return "OP_POP";
        case OpCode::OP_DUP: return "OP_DUP";
        case OpCode::OP_GET_LOCAL: return "OP_GET_LOCAL";
        case OpCode::OP_SET_LOCAL: return "OP_SET_LOCAL";
        case OpCode::OP_GET_GLOBAL: return "OP_GET_GLOBAL";
        case OpCode::OP_SET_GLOBAL: return "OP_SET_GLOBAL";
        case OpCode::OP_DEFINE_GLOBAL: return "OP_DEFINE_GLOBAL";
        case OpCode::OP_EQUAL: return "OP_EQUAL";
        case OpCode::OP_NOT_EQUAL: return "OP_NOT_EQUAL";
        case OpCode::OP_GREATER: return "OP_GREATER";
        case OpCode::OP_GREATER_EQUAL: return "OP_GREATER_EQUAL";
        case OpCode::OP_LESS: return "OP_LESS";
        case OpCode::OP_LESS_EQUAL: return "OP_LESS_EQUAL";
        case OpCode::OP_ADD: return "OP_ADD";
        case OpCode::OP_SUBTRACT: return "OP_SUBTRACT";
        case OpCode::OP_MULTIPLY: return "OP_MULTIPLY";
        case OpCode::OP_DIVIDE: return "OP_DIVIDE";
        case OpCode::OP_MODULO: return "OP_MODULO";
        case OpCode::OP_POWER: return "OP_POWER";
        case OpCode::OP_NEGATE: return "OP_NEGATE";
        case OpCode::OP_NOT: return "OP_NOT";
        case OpCode::OP_JUMP: return "OP_JUMP";
        case OpCode::OP_JUMP_IF_FALSE: return "OP_JUMP_IF_FALSE";
        case OpCode::OP_LOOP: return "OP_LOOP";
        case OpCode::OP_CALL: return "OP_CALL";
        case OpCode::OP_CLOSURE: return "OP_CLOSURE";
        case OpCode::OP_RETURN: return "OP_RETURN";
        case OpCode::OP_BUILD_ARRAY: return "OP_BUILD_ARRAY";
        case OpCode::OP_BUILD_OBJECT: return "OP_BUILD_OBJECT";
        case OpCode::OP_INDEX_GET: return "OP_INDEX_GET";
        case OpCode::OP_INDEX_SET: return "OP_INDEX_SET";
        case OpCode::OP_PRINT: return "OP_PRINT";
    }
    return "UNKNOWN_OPCODE";
}

void Chunk::write(uint8_t byte, size_t line) {
    code_.push_back(byte);
    lines_.push_back(line);
}

void Chunk::write_opcode(OpCode op, size_t line) {
    write(static_cast<uint8_t>(op), line);
}

size_t Chunk::add_constant(Value value) {
    constants_.push_back(std::move(value));
    return constants_.size() - 1;
}

void Chunk::disassemble(std::string_view name) const {
    std::cout << "== " << name << " ==\n";
    for (size_t offset = 0; offset < code_.size();) {
        offset = disassemble_instruction(offset);
    }
}

size_t Chunk::disassemble_instruction(size_t offset) const {
    std::cout << std::setw(4) << std::setfill('0') << offset << " ";

    if (offset > 0 && lines_[offset] == lines_[offset - 1]) {
        std::cout << "   | ";
    } else {
        std::cout << std::setw(4) << std::setfill(' ') << lines_[offset] << " ";
    }

    uint8_t instruction = code_[offset];
    OpCode op = static_cast<OpCode>(instruction);

    switch (op) {
        case OpCode::OP_CONSTANT: {
            uint8_t constant_idx = code_[offset + 1];
            std::cout << std::left << std::setw(16) << "OP_CONSTANT" << " "
                      << std::setw(4) << static_cast<int>(constant_idx) << " '"
                      << constants_[constant_idx].inspect() << "'\n";
            return offset + 2;
        }
        case OpCode::OP_GET_LOCAL:
        case OpCode::OP_SET_LOCAL:
        case OpCode::OP_CALL:
        case OpCode::OP_BUILD_ARRAY:
        case OpCode::OP_BUILD_OBJECT:
        case OpCode::OP_PRINT: {
            uint8_t slot = code_[offset + 1];
            std::cout << std::left << std::setw(16) << opcode_to_string(op) << " "
                      << static_cast<int>(slot) << "\n";
            return offset + 2;
        }
        case OpCode::OP_GET_GLOBAL:
        case OpCode::OP_SET_GLOBAL:
        case OpCode::OP_DEFINE_GLOBAL: {
            uint8_t constant_idx = code_[offset + 1];
            std::cout << std::left << std::setw(16) << opcode_to_string(op) << " "
                      << std::setw(4) << static_cast<int>(constant_idx) << " '"
                      << constants_[constant_idx].to_string() << "'\n";
            return offset + 2;
        }
        case OpCode::OP_JUMP:
        case OpCode::OP_JUMP_IF_FALSE:
        case OpCode::OP_LOOP: {
            uint16_t jump = static_cast<uint16_t>((code_[offset + 1] << 8) | code_[offset + 2]);
            std::cout << std::left << std::setw(16) << opcode_to_string(op) << " "
                      << std::setw(4) << offset << " -> "
                      << (op == OpCode::OP_LOOP ? offset + 3 - jump : offset + 3 + jump) << "\n";
            return offset + 3;
        }
        default:
            std::cout << opcode_to_string(op) << "\n";
            return offset + 1;
    }
}

} // namespace nextviper
