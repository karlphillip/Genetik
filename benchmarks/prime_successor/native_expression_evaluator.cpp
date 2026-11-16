#include "DiscreteExpressionParsing.hpp"
#include "Version.hpp"

#include <atomic>
#include <charconv>
#include <csignal>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
std::atomic<bool> cancelled{false};
static_assert(std::atomic<bool>::is_always_lock_free, "signal cancellation flag must be lock-free");
void requestCancellation(int) {
    cancelled.store(true, std::memory_order_relaxed);
}

bool predictiveKind(DiscreteKind kind) {
    switch (kind) {
    case DiscreteKind::Integer:
    case DiscreteKind::Variable:
    case DiscreteKind::Add:
    case DiscreteKind::Subtract:
    case DiscreteKind::Multiply:
    case DiscreteKind::Negate:
    case DiscreteKind::Modulo:
    case DiscreteKind::Equal:
    case DiscreteKind::NotEqual:
    case DiscreteKind::LessThan:
    case DiscreteKind::LessEqual:
    case DiscreteKind::GreaterThan:
    case DiscreteKind::GreaterEqual:
    case DiscreteKind::IfThenElse:
        return true;
    default:
        return false;
    }
}

std::size_t limitArgument(const char* text, const char* name) {
    std::size_t value = 0;
    const std::string input(text);
    const auto result = std::from_chars(input.data(), input.data() + input.size(), value);
    if (result.ec != std::errc{} || result.ptr != input.data() + input.size() || value == 0)
        throw std::invalid_argument(std::string("invalid ") + name);
    return value;
}

const char* statusName(DiscreteEvaluationStatus status) {
    switch (status) {
    case DiscreteEvaluationStatus::Valid:
        return "valid";
    case DiscreteEvaluationStatus::MathematicalInvalid:
        return "mathematical_invalid";
    case DiscreteEvaluationStatus::ResourceExhausted:
        return "resource_exhausted";
    case DiscreteEvaluationStatus::Cancelled:
        return "cancelled";
    case DiscreteEvaluationStatus::NoWitness:
        return "no_witness";
    }
    return "unknown";
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--self-test") {
            const std::int64_t input = 5;
            const auto expression = parseDiscreteExpression("x0 + 1 + 2", 1);
            const auto cancelledResult = expression.evaluateWithCancellation(
                std::span<const std::int64_t>(&input, 1), {}, [] { return true; });
            if (cancelledResult.status != DiscreteEvaluationStatus::Cancelled)
                throw std::runtime_error("native evaluator cancellation contract failed");
            DiscreteEvaluationLimits limits;
            limits.maxNodeVisits = 1;
            if (expression.evaluate(std::span<const std::int64_t>(&input, 1), limits).status !=
                DiscreteEvaluationStatus::ResourceExhausted)
                throw std::runtime_error("native evaluator node visit limit failed");
            limits = {};
            limits.maxOperations = 1;
            if (expression.evaluate(std::span<const std::int64_t>(&input, 1), limits).status !=
                DiscreteEvaluationStatus::ResourceExhausted)
                throw std::runtime_error("native evaluator operation limit failed");
            limits = {};
            limits.maxWorkspaceBytes = 1;
            if (expression.evaluate(std::span<const std::int64_t>(&input, 1), limits).status !=
                DiscreteEvaluationStatus::ResourceExhausted)
                throw std::runtime_error("native evaluator workspace limit failed");
            return 0;
        }
        if (argc == 3 && std::string_view(argv[1]) == "--validate") {
            auto expression = parseDiscreteExpression(argv[2], 1);
            for (const auto& node : expression.nodes())
                if (!predictiveKind(node.kind()))
                    throw std::invalid_argument(
                        "expression contains an operator outside the native predictive track");
            if (expression.type() != DiscreteType::Int64)
                throw std::invalid_argument("predictive expression must return Int64");
            std::cout << expression.nodeCount() << '\t' << expression.constantCount() << '\t'
                      << genetikVersion << '\n';
            return 0;
        }
        if (argc != 5) {
            std::cerr << "usage: genetik_prime_successor_native EXPRESSION MAX_NODE_VISITS "
                         "MAX_OPERATIONS MAX_WORKSPACE_BYTES\n";
            return 2;
        }
        std::signal(SIGINT, requestCancellation);
        std::signal(SIGTERM, requestCancellation);
        auto expression = parseDiscreteExpression(argv[1], 1);
        for (const auto& node : expression.nodes())
            if (!predictiveKind(node.kind()))
                throw std::invalid_argument(
                    "expression contains an operator outside the native predictive track");
        if (expression.type() != DiscreteType::Int64)
            throw std::invalid_argument("predictive expression must return Int64");

        DiscreteEvaluationLimits limits;
        limits.maxNodeVisits = limitArgument(argv[2], "node visit limit");
        limits.maxOperations = limitArgument(argv[3], "operation limit");
        limits.maxWorkspaceBytes = limitArgument(argv[4], "workspace limit");
        std::cout << "#native\t1\tnodes\t" << expression.nodeCount() << "\tconstants\t"
                  << expression.constantCount() << "\tversion\t" << genetikVersion << '\n';
        std::string line;
        while (std::getline(std::cin, line)) {
            std::int64_t input = 0;
            const auto parsed = std::from_chars(line.data(), line.data() + line.size(), input);
            if (parsed.ec != std::errc{} || parsed.ptr != line.data() + line.size())
                throw std::invalid_argument("input row must be one Int64");
            const auto result =
                expression.evaluate(std::span<const std::int64_t>(&input, 1), limits, &cancelled);
            std::cout << statusName(result.status) << '\t';
            if (result.valid())
                std::cout << result.value->integer();
            else
                std::cout << "-";
            std::cout << '\t' << result.nodeVisits << '\t' << result.operations << '\t'
                      << result.iterations << '\t' << result.iterationOperations << '\n';
            if (result.status == DiscreteEvaluationStatus::Cancelled)
                return 130;
        }
        if (cancelled.load(std::memory_order_relaxed))
            return 130;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
