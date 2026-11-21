#pragma once
#include <cmath>
#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
#include "detail/ScalarOperators.hpp"

// Immutable application supplied scalar operation. IDs are stable checkpoint keys;
// the application must register the same ID and semantics when restoring a run.
struct CustomScalarOperator {
    std::string id;
    std::size_t arity = 0;
    std::function<std::optional<double>(std::span<const double>)> evaluate;
    std::function<std::optional<std::vector<double>>(std::span<const double>)> partials;
    void validate() const {
        if (id.empty() || (arity != 1 && arity != 2) || !evaluate)
            throw std::invalid_argument(
                "custom scalar operator needs a stable ID, unary/binary arity, and evaluator");
        for (unsigned char c : id)
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '_' || c == '.' || c == ':' || c == '/' || c == '-'))
                throw std::invalid_argument(
                    "custom scalar operator ID contains non-token characters");
        for (const auto& builtin : scalarOperators)
            if (builtin.arity && id == builtin.name)
                throw std::invalid_argument(
                    "custom scalar operator ID conflicts with built-in operator: " + id);
    }
};
