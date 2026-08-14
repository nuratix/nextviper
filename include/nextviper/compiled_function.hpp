#pragma once

#include "nextviper/chunk.hpp"
#include <string>
#include <memory>

namespace nextviper {

class CompiledFunction {
public:
    CompiledFunction(std::string name = "", size_t arity = 0)
        : name_(std::move(name)), arity_(arity) {}

    const std::string& name() const { return name_; }
    size_t arity() const { return arity_; }
    Chunk& chunk() { return chunk_; }
    const Chunk& chunk() const { return chunk_; }

private:
    std::string name_;
    size_t arity_ = 0;
    Chunk chunk_;
};

} // namespace nextviper
