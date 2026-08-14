cmake_minimum_required(VERSION 3.16)
project(NextViper VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include directories
include_directories(include)

# Core Library Sources
set(CORE_SOURCES
    src/token.cpp
    src/diagnostic.cpp
    src/lexer.cpp
    src/ast.cpp
    src/parser.cpp
    src/value.cpp
    src/environment.cpp
    src/interpreter.cpp
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
    tests/test_vm.cpp
    tests/test_diagnostics.cpp
)

add_executable(test_runner ${TEST_SOURCES})
target_link_libraries(test_runner PRIVATE nextviper_core)

# Output directories
set_target_properties(nextviper PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(test_runner PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
