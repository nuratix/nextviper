#pragma once
#include "nextviper/value.hpp"
#include <functional>
#include <map>

namespace nextviper {
using TransportInvoke = std::function<Value(const Value&, const std::vector<Value>&, SourceSpan)>;
Value http_exchange(const Value& config, const Value& on_chunk, const TransportInvoke& invoke, SourceSpan span);
Value spawn_interactive(const Value& config, SourceSpan span);
}
