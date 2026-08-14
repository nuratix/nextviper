CXX = g++
CXXFLAGS = -std=c++20 -O3 -Wall -Wextra -Iinclude -pthread -MMD -MP
LDFLAGS = 

SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
BIN_DIR = bin
BUILD_DIR = build

# Core library source files (excluding main.cpp)
CORE_SRCS = $(SRC_DIR)/token.cpp \
            $(SRC_DIR)/diagnostic.cpp \
            $(SRC_DIR)/lexer.cpp \
            $(SRC_DIR)/ast.cpp \
            $(SRC_DIR)/parser.cpp \
            $(SRC_DIR)/type.cpp \
            $(SRC_DIR)/type_checker.cpp \
            $(SRC_DIR)/value.cpp \
            $(SRC_DIR)/environment.cpp \
            $(SRC_DIR)/interpreter.cpp \
            $(SRC_DIR)/module.cpp \
            $(SRC_DIR)/tensor.cpp \
            $(SRC_DIR)/dataset.cpp \
            $(SRC_DIR)/ai_model.cpp \
            $(SRC_DIR)/ir.cpp \
            $(SRC_DIR)/native_compiler.cpp \
            $(SRC_DIR)/formatter.cpp \
            $(SRC_DIR)/chunk.cpp \
            $(SRC_DIR)/compiler.cpp \
            $(SRC_DIR)/vm.cpp \
            $(SRC_DIR)/repl.cpp

CORE_OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(CORE_SRCS))

MAIN_SRC = $(SRC_DIR)/main.cpp
MAIN_OBJ = $(BUILD_DIR)/main.o

TEST_SRCS = $(TEST_DIR)/test_runner.cpp \
            $(TEST_DIR)/test_lexer.cpp \
            $(TEST_DIR)/test_parser.cpp \
            $(TEST_DIR)/test_interpreter.cpp \
            $(TEST_DIR)/test_type_system.cpp \
            $(TEST_DIR)/test_collections.cpp \
            $(TEST_DIR)/test_loops.cpp \
            $(TEST_DIR)/test_modules.cpp \
            $(TEST_DIR)/test_ai_data.cpp \
            $(TEST_DIR)/test_native_compiler.cpp \
            $(TEST_DIR)/test_formatter.cpp \
            $(TEST_DIR)/test_tooling.cpp \
            $(TEST_DIR)/test_vm.cpp \
            $(TEST_DIR)/test_diagnostics.cpp \
            $(TEST_DIR)/test_fuzz.cpp

TEST_OBJS = $(patsubst $(TEST_DIR)/%.cpp, $(BUILD_DIR)/tests/%.o, $(TEST_SRCS))

TARGET = $(BIN_DIR)/nextviper
TEST_TARGET = $(BIN_DIR)/test_runner

.PHONY: all clean test examples directories

all: directories $(TARGET)

directories:
	@mkdir -p $(BIN_DIR) $(BUILD_DIR) $(BUILD_DIR)/tests

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/tests/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(CORE_OBJS) $(MAIN_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "\033[1;32m[Built]\033[0m $(TARGET)"

$(TEST_TARGET): $(CORE_OBJS) $(TEST_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "\033[1;32m[Built]\033[0m $(TEST_TARGET)"

test: directories $(TEST_TARGET) $(TARGET)
	@echo "\n\033[1;34m=== Running Unit Tests ===\033[0m"
	@./$(TEST_TARGET)
	@echo "\n\033[1;34m=== Running CLI Integration Tests ===\033[0m"
	@bash $(TEST_DIR)/test_cli.sh

examples: $(TARGET)
	@echo "\n\033[1;34m=== Running Hello World Example ===\033[0m"
	@./$(TARGET) run examples/hello_world.nv
	@echo "\n\033[1;34m=== Running Basics Example ===\033[0m"
	@./$(TARGET) run examples/basics.nv
	@echo "\n\033[1;34m=== Running Functions Example ===\033[0m"
	@./$(TARGET) run examples/functions.nv
	@echo "\n\033[1;34m=== Running Math Example ===\033[0m"
	@./$(TARGET) run examples/math_ops.nv
	@echo "\n\033[1;34m=== Running Data Pipeline Example ===\033[0m"
	@./$(TARGET) run examples/data_pipeline.nv

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "\033[1;33m[Cleaned]\033[0m Build artifacts removed"

-include $(wildcard $(BUILD_DIR)/*.d) $(wildcard $(BUILD_DIR)/tests/*.d)

