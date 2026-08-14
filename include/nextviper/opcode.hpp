#pragma once

#include <cstdint>
#include <string_view>

namespace nextviper {

enum class OpCode : uint8_t {
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_DUP,

    // Variables
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_DEFINE_GLOBAL,

    // Comparisons
    OP_EQUAL,
    OP_NOT_EQUAL,
    OP_GREATER,
    OP_GREATER_EQUAL,
    OP_LESS,
    OP_LESS_EQUAL,

    // Arithmetic
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_MODULO,
    OP_POWER,
    OP_NEGATE,
    OP_NOT,

    // Control Flow
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,

    // Functions
    OP_CALL,
    OP_CLOSURE,
    OP_RETURN,

    // Data Structures
    OP_BUILD_ARRAY,
    OP_BUILD_OBJECT,
    OP_INDEX_GET,
    OP_INDEX_SET,

    // Built-in
    OP_PRINT
};

std::string_view opcode_to_string(OpCode op);

} // namespace nextviper
