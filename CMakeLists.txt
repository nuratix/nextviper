cmake_minimum_required(VERSION 3.16)
project(NextViper VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Threads REQUIRED)
find_package(Vulkan REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(CURL 7.85 REQUIRED)
option(NEXTVIPER_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
if(MSVC)
    add_compile_options(/W4 /EHsc)
else()
    add_compile_options(-Wall -Wextra)
    if(NEXTVIPER_SANITIZERS)
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()
set(CORE_SOURCES
    src/token.cpp
    src/diagnostic.cpp
    src/error_registry.cpp
    src/lexer.cpp
    src/ast.cpp
    src/parser.cpp
    src/type.cpp
    src/type_checker.cpp
    src/value.cpp
    src/environment.cpp
    src/interpreter.cpp
    src/module.cpp
    src/http_transport.cpp
    src/interactive_process.cpp
    src/tensor.cpp
    src/gpu_backend.cpp
    src/gpu_kernels.cpp
    src/dataset.cpp
    src/ai_model.cpp
    src/autograd.cpp
    src/ai_layers.cpp
    src/ai_loss.cpp
    src/ai_optimizer.cpp
    src/ai_metrics.cpp
    src/ai_serialization.cpp
    src/ai_subsystem.cpp
    src/ir.cpp
    src/native_compiler.cpp
    src/formatter.cpp
    src/chunk.cpp
    src/compiler.cpp
    src/vm.cpp
    src/repl.cpp
    src/package_manager.cpp
    src/data_subsystem.cpp
    src/lsp.cpp
    src/linter.cpp
)
add_library(nextviper_core STATIC ${CORE_SOURCES})
target_include_directories(nextviper_core PUBLIC include)
target_link_libraries(nextviper_core PUBLIC Threads::Threads Vulkan::Vulkan PostgreSQL::PostgreSQL CURL::libcurl)
add_executable(nextviper src/main.cpp)
target_link_libraries(nextviper PRIVATE nextviper_core)
add_executable(nextviper-lsp src/lsp_main.cpp)
target_link_libraries(nextviper-lsp PRIVATE nextviper_core)
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
    tests/test_data_subsystem.cpp
    tests/test_ai_subsystem.cpp
    tests/test_gpu_subsystem.cpp
    tests/test_native_compiler.cpp
    tests/test_stdlib.cpp
    tests/test_package_manager.cpp
    tests/test_formatter.cpp
    tests/test_tooling.cpp
    tests/test_vm.cpp
    tests/test_diagnostics.cpp
    tests/test_fuzz.cpp
    tests/test_lsp.cpp
    tests/test_error_system.cpp
    tests/test_linter.cpp
)
add_executable(test_runner ${TEST_SOURCES})
target_link_libraries(test_runner PRIVATE nextviper_core)
set_target_properties(nextviper nextviper-lsp test_runner PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
enable_testing()
add_test(NAME unit COMMAND test_runner)
set_tests_properties(unit PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
if(UNIX)
    add_test(NAME cli COMMAND bash "${CMAKE_SOURCE_DIR}/tests/test_cli.sh")
    add_test(NAME errors COMMAND bash "${CMAKE_SOURCE_DIR}/tests/test_error_codes.sh")
    set_tests_properties(cli errors PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        ENVIRONMENT "NEXTVIPER_BIN=$<TARGET_FILE:nextviper>;NEXTVIPER_LSP_BIN=$<TARGET_FILE:nextviper-lsp>")
endif()
install(TARGETS nextviper nextviper-lsp RUNTIME DESTINATION bin)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME regressions COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/test_regressions.py")
set_tests_properties(regressions PROPERTIES ENVIRONMENT "NEXTVIPER_BIN=$<TARGET_FILE:nextviper>")

if(UNIX)
    add_test(NAME transports COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/test_transport.py")
    set_tests_properties(transports PROPERTIES ENVIRONMENT "NEXTVIPER_BIN=$<TARGET_FILE:nextviper>")
endif()
