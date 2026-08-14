cmake_minimum_required(VERSION 3.16)
project(NextViper VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings and optimization flags
if(MSVC)
    add_compile_options(/W4 /EHsc /O2)
else()
    add_compile_options(-Wall -Wextra -O3 -pthread)
endif()

# Include directories
include_directories(include)

# Core Library Sources
set(CORE_SOURCES
    src/token.cpp
    src/diagnostic.cpp
    src/lexer.cpp
    src/ast.cpp
    src/parser.cpp
    src/type.cpp
    src/type_checker.cpp
    src/value.cpp
    src/environment.cpp
    src/interpreter.cpp
    src/module.cpp
    src/tensor.cpp
    src/dataset.cpp
    src/ai_model.cpp
    src/ir.cpp
    src/native_compiler.cpp
    src/formatter.cpp
    src/chunk.cpp
    src/compiler.cpp
    src/vm.cpp
    src/repl.cpp
)

# Core library target
add_library(nextviper_core STATIC ${CORE_SOURCES})

# CLI Executable
add_executable(nextviper src/main.cpp)
target_link_libraries(nextviper PRIVATE nextviper_core)

# Test Suite Executable
set(TEST_SOURCES
    tests/test_runner.cpp
    tests/test_lexer.cpp
    tests/test_parser.cpp
    tests/test_interpreter.cpp
    tests/test_type_system.cpp
    tests/test_collections.cpp
    tests/test_loops.cpp
    tests/test_modules.cpp
    tests/test_ai_data.cpp
    tests/test_native_compiler.cpp
    tests/test_formatter.cpp
    tests/test_tooling.cpp
    tests/test_vm.cpp
    tests/test_diagnostics.cpp
    tests/test_fuzz.cpp
)

add_executable(test_runner ${TEST_SOURCES})
target_link_libraries(test_runner PRIVATE nextviper_core)

# Output directories
set_target_properties(nextviper PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(test_runner PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
