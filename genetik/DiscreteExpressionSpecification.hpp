#pragma once

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "DiscreteExpression.hpp"

namespace genetik_specification_detail {
template <class Integer> inline void appendNumber(std::string& out, Integer value) {
    char buffer[std::numeric_limits<Integer>::digits10 + 4]{};
    const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value);
    if (converted.ec != std::errc{})
        throw std::logic_error("canonical identity integer formatting failed");
    out.append(buffer, converted.ptr);
}
} // namespace genetik_specification_detail

class DiscreteSpecification;
namespace genetik_specification_detail {
std::optional<std::size_t> logicalIdentityBytes(const DiscreteSpecification&) noexcept;
} // namespace genetik_specification_detail

// A finite, exact table of required input/output behavior. The rows are
// copied on construction and exposed only through a const span.
struct DiscreteSpecificationObservation {
    std::vector<std::int64_t> inputs;
    DiscreteValue expected = DiscreteValue::integer(0);
};

class DiscreteSpecification {
public:
    DiscreteSpecification(std::size_t dimensions,
                          std::vector<DiscreteSpecificationObservation> observations)
        : dimensions_(dimensions), observations_(std::move(observations)) {}

    std::size_t dimensions() const noexcept {
        return dimensions_;
    }
    std::span<const DiscreteSpecificationObservation> observations() const noexcept {
        return observations_;
    }

    // Full canonical identities are returned instead of short hashes so the
    // certificate has no hash-collision qualification.
    std::string identity() const {
        const auto size = genetik_specification_detail::logicalIdentityBytes(*this);
        if (!size)
            throw std::length_error("specification identity size overflow");
        std::string out;
        out.reserve(*size);
        out += "discrete-spec-v1;d=";
        genetik_specification_detail::appendNumber(out, dimensions_);
        out += ";rows=";
        genetik_specification_detail::appendNumber(out, observations_.size());
        out += ';';
        for (const auto& row : observations_) {
            out += "[";
            for (auto input : row.inputs) {
                genetik_specification_detail::appendNumber(out, input);
                out += ',';
            }
            out += "]=";
            appendValue(out, row.expected);
            out += ";";
        }
        return out;
    }

private:
    static void appendValue(std::string& out, DiscreteValue value) {
        if (value.type() == DiscreteType::Int64) {
            out += 'i';
            genetik_specification_detail::appendNumber(out, value.integer());
        } else {
            out += value.boolean() ? "b1" : "b0";
        }
    }

    std::size_t dimensions_;
    std::vector<DiscreteSpecificationObservation> observations_;
};

enum class DiscreteSpecificationValidationStatus {
    ValidatedFiniteDomain,
    CounterexampleFound,
    InconclusiveResourceExhausted,
    InconclusiveCancelled,
    InvalidSpecification,
    InvalidCandidate
};

inline const char*
discreteSpecificationValidationStatusName(DiscreteSpecificationValidationStatus status) noexcept {
    switch (status) {
    case DiscreteSpecificationValidationStatus::ValidatedFiniteDomain:
        return "ValidatedFiniteDomain";
    case DiscreteSpecificationValidationStatus::CounterexampleFound:
        return "CounterexampleFound";
    case DiscreteSpecificationValidationStatus::InconclusiveResourceExhausted:
        return "InconclusiveResourceExhausted";
    case DiscreteSpecificationValidationStatus::InconclusiveCancelled:
        return "InconclusiveCancelled";
    case DiscreteSpecificationValidationStatus::InvalidSpecification:
        return "InvalidSpecification";
    case DiscreteSpecificationValidationStatus::InvalidCandidate:
        return "InvalidCandidate";
    }
    return "InvalidSpecification";
}

struct DiscreteSpecificationValidationLimits {
    std::size_t maxRows = 100000;
    std::size_t maxSpecificationComparisons = 5000000;
    std::size_t maxNodeVisits = 10000000;
    std::size_t maxOperations = 10000000;
    std::size_t maxIterations = 1000000;
    // These are logical admission estimates/allowances, not process RSS.
    std::size_t maxRetainedBytes = 64 * 1024 * 1024;
    std::size_t maxPeakRowWorkspaceBytes = 1024 * 1024;
    DiscreteEvaluationLimits evaluation{};
};

struct DiscreteSpecificationValidationWork {
    std::size_t duplicateTupleComparisons = 0;
    std::size_t specificationComparisons = 0;
    std::size_t rowsAttempted = 0;
    std::size_t rowsMatched = 0;
    std::size_t nodeVisits = 0;
    std::size_t operations = 0;
    std::size_t iterations = 0;
    std::size_t iterationOperations = 0;
    std::size_t retainedBytesEstimate = 0;
    std::size_t peakRowWorkspaceAllowanceBytes = 0;
};

struct DiscreteSpecificationCounterexample {
    std::size_t ordinal = 0;
    std::vector<std::int64_t> inputs;
    DiscreteValue expected = DiscreteValue::integer(0);
    DiscreteValue actual = DiscreteValue::integer(0);
    std::string specificationIdentity;
    std::string candidateIdentity;
};

struct DiscreteSpecificationEvaluationFailure {
    std::size_t ordinal = 0;
    std::vector<std::int64_t> inputs;
    DiscreteEvaluationStatus status = DiscreteEvaluationStatus::MathematicalInvalid;
    std::string specificationIdentity;
    std::string candidateIdentity;
};

struct DiscreteSpecificationCertificate {
    std::string specificationIdentity;
    std::string candidateIdentity;
    std::size_t declaredRows = 0;
    std::size_t checkedRows = 0;
    DiscreteSpecificationValidationWork work;
};

struct DiscreteSpecificationValidationResult {
    DiscreteSpecificationValidationStatus status =
        DiscreteSpecificationValidationStatus::InvalidSpecification;
    std::optional<DiscreteSpecificationCertificate> certificate;
    std::optional<DiscreteSpecificationCounterexample> counterexample;
    std::optional<DiscreteSpecificationEvaluationFailure> evaluationFailure;
    std::optional<DiscreteEvaluationStatus> evaluatorStatus;
    std::optional<std::string> specificationIdentity;
    std::optional<std::string> candidateIdentity;
    DiscreteSpecificationValidationWork work;
    std::string diagnostic;
};

namespace genetik_specification_detail {
inline bool addWithin(std::size_t current, std::size_t amount, std::size_t limit) noexcept {
    return amount <= limit && current <= limit - amount;
}

inline std::size_t unsignedDigits(std::uint64_t value) noexcept {
    std::size_t digits = 1;
    while (value >= 10) {
        value /= 10;
        ++digits;
    }
    return digits;
}

inline std::size_t integerDigits(std::int64_t value) noexcept {
    return unsignedDigits(genetik_discrete_detail::magnitude(value)) + (value < 0 ? 1U : 0U);
}

inline std::size_t sizeDigits(std::size_t value) noexcept {
    return unsignedDigits(static_cast<std::uint64_t>(value));
}

inline std::optional<std::size_t>
candidateIdentityBytes(const DiscreteExpression& candidate) noexcept {
    std::size_t bytes =
        sizeof("dexpr-v1;d=;scope=;nodes=;") - 1 + 1 + sizeDigits(candidate.dimensions()) +
        sizeDigits(candidate.outerBindings().size()) + sizeDigits(candidate.nodeCount());
    for (auto binder : candidate.outerBindings()) {
        const auto addition = sizeDigits(binder) + 1;
        if (addition > std::numeric_limits<std::size_t>::max() - bytes)
            return std::nullopt;
        bytes += addition;
    }
    for (const auto& node : candidate.nodes()) {
        const auto kindBytes = sizeDigits(static_cast<std::size_t>(node.kind()));
        std::size_t valueBytes = 0;
        if (node.kind() == DiscreteKind::Integer)
            valueBytes = integerDigits(node.integer());
        else if (node.kind() == DiscreteKind::Boolean)
            valueBytes = 1;
        else if (node.kind() == DiscreteKind::Variable)
            valueBytes = sizeDigits(node.variable());
        else if (node.kind() == DiscreteKind::Iterator || discreteIteration(node.kind()))
            valueBytes = sizeDigits(node.binder());
        const auto addition = kindBytes + 1 + valueBytes + 1;
        if (addition > std::numeric_limits<std::size_t>::max() - bytes)
            return std::nullopt;
        bytes += addition;
    }
    return bytes;
}

inline std::string candidateIdentity(const DiscreteExpression& candidate, std::size_t bytes) {
    std::string out;
    out.reserve(bytes);
    out += "dexpr-v1;d=";
    appendNumber(out, candidate.dimensions());
    out += ";scope=";
    appendNumber(out, candidate.outerBindings().size());
    out += ':';
    for (auto binder : candidate.outerBindings()) {
        appendNumber(out, binder);
        out += ',';
    }
    out += ";nodes=";
    appendNumber(out, candidate.nodeCount());
    out += ';';
    for (const auto& node : candidate.nodes()) {
        appendNumber(out, static_cast<std::size_t>(node.kind()));
        out += ':';
        if (node.kind() == DiscreteKind::Integer)
            appendNumber(out, node.integer());
        else if (node.kind() == DiscreteKind::Boolean)
            out += node.boolean() ? '1' : '0';
        else if (node.kind() == DiscreteKind::Variable)
            appendNumber(out, node.variable());
        else if (node.kind() == DiscreteKind::Iterator || discreteIteration(node.kind()))
            appendNumber(out, node.binder());
        out += ';';
    }
    return out;
}

inline std::optional<std::size_t>
logicalIdentityBytes(const DiscreteSpecification& specification) noexcept {
    // Exact size of identity() without constructing the potentially large text.
    std::size_t bytes = sizeof("discrete-spec-v1;d=;rows=;") - 1 +
                        unsignedDigits(specification.dimensions()) +
                        unsignedDigits(specification.observations().size());
    for (const auto& row : specification.observations()) {
        if (bytes > std::numeric_limits<std::size_t>::max() - 4)
            return std::nullopt;
        bytes += 4; // '[', ']', '=', ';'
        for (const auto input : row.inputs) {
            const auto digits = integerDigits(input);
            if (digits >= std::numeric_limits<std::size_t>::max() - bytes)
                return std::nullopt;
            bytes += digits + 1; // decimal value and comma
        }
        const auto expectedBytes = row.expected.type() == DiscreteType::Int64
                                       ? integerDigits(row.expected.integer()) + 1
                                       : 2;
        if (expectedBytes > std::numeric_limits<std::size_t>::max() - bytes)
            return std::nullopt;
        bytes += expectedBytes;
    }
    return bytes;
}

inline std::optional<std::size_t>
logicalSpecificationBytes(const DiscreteSpecification& specification) noexcept {
    std::size_t bytes = sizeof(specification);
    for (const auto& row : specification.observations()) {
        constexpr auto rowBytes = sizeof(DiscreteSpecificationObservation);
        if (bytes > std::numeric_limits<std::size_t>::max() - rowBytes)
            return std::nullopt;
        bytes += rowBytes;
        if (row.inputs.size() >
            (std::numeric_limits<std::size_t>::max() - bytes) / sizeof(std::int64_t))
            return std::nullopt;
        bytes += row.inputs.size() * sizeof(std::int64_t);
    }
    return bytes;
}

inline bool validLimits(const DiscreteSpecificationValidationLimits& limits) noexcept {
    return limits.evaluation.maxIterationDepth <= 128 &&
           limits.evaluation.maxIterationsPerOperation <= 1000000000 &&
           limits.evaluation.maxAggregateIterations <= 1000000000;
}
} // namespace genetik_specification_detail

// Exhaustively checks rows in their declared order. A certificate covers only
// those finite rows and makes no claim about other inputs.
template <class Stop>
inline DiscreteSpecificationValidationResult
validateDiscreteSpecificationWithStop(const DiscreteSpecification& specification,
                                      const DiscreteExpression& candidate,
                                      DiscreteSpecificationValidationLimits limits, Stop&& stop) {
    using Status = DiscreteSpecificationValidationStatus;
    using namespace genetik_specification_detail;
    DiscreteSpecificationValidationResult result;
    const auto rows = specification.observations();
    const auto invalid = [&](std::string message) {
        result.status = Status::InvalidSpecification;
        result.diagnostic = std::move(message);
        return result;
    };
    if (!validLimits(limits))
        return invalid("invalid validation evaluation limits");
    if (rows.empty())
        return invalid("specification must declare at least one observation");
    if (rows.size() > limits.maxRows) {
        result.status = Status::InconclusiveResourceExhausted;
        result.diagnostic = "declared row count exceeds validation limit";
        return result;
    }
    const auto outputType = rows.front().expected.type();
    for (const auto& row : rows) {
        if (row.inputs.size() != specification.dimensions() || row.expected.type() != outputType)
            return invalid("specification row schema or output type is inconsistent");
    }

    const auto specificationBytes = logicalSpecificationBytes(specification);
    if (!specificationBytes ||
        candidate.nodeCount() >
            std::numeric_limits<std::size_t>::max() / DiscreteExpression::storageBytesPerNode()) {
        result.status = Status::InconclusiveResourceExhausted;
        result.diagnostic = "logical retained-memory estimate overflowed";
        return result;
    }
    const auto candidateBytes = candidate.nodeCount() * DiscreteExpression::storageBytesPerNode();
    if (candidateBytes > std::numeric_limits<std::size_t>::max() - *specificationBytes) {
        result.status = Status::InconclusiveResourceExhausted;
        result.diagnostic = "logical retained-memory estimate overflowed";
        return result;
    }
    result.work.retainedBytesEstimate = *specificationBytes + candidateBytes;
    if (result.work.retainedBytesEstimate > limits.maxRetainedBytes) {
        result.status = Status::InconclusiveResourceExhausted;
        result.diagnostic = "row and candidate storage exceeds logical memory allowance";
        return result;
    }

    // Only after cheap owned-storage admission do full canonical identities
    // traverse input coordinates and expression nodes.
    const auto specificationIdentityBytes = logicalIdentityBytes(specification);
    const auto candidateIdentitySize = candidateIdentityBytes(candidate);
    if (!specificationIdentityBytes || !candidateIdentitySize) {
        result.status = Status::InconclusiveResourceExhausted;
        result.diagnostic = "logical identity-size estimate overflowed";
        return result;
    }
    constexpr auto resultOverhead = sizeof(DiscreteSpecificationValidationResult) + 128 + 22;
    if (specification.dimensions() >
        (std::numeric_limits<std::size_t>::max() - resultOverhead) / sizeof(std::int64_t)) {
        result.status = Status::InconclusiveResourceExhausted;
        result.diagnostic = "logical result-memory estimate overflowed";
        return result;
    }
    auto outputPayloadBytes = resultOverhead + specification.dimensions() * sizeof(std::int64_t);
    if (*specificationIdentityBytes >
            std::numeric_limits<std::size_t>::max() - outputPayloadBytes ||
        *candidateIdentitySize > std::numeric_limits<std::size_t>::max() -
                                     (outputPayloadBytes + *specificationIdentityBytes)) {
        result.status = Status::InconclusiveResourceExhausted;
        result.diagnostic = "logical result-memory estimate overflowed";
        return result;
    }
    // The result keeps one identity pair. A counterexample or mathematical
    // invalidation record also owns an identity pair and one copied tuple.
    outputPayloadBytes += *specificationIdentityBytes + *candidateIdentitySize;
    if (*specificationIdentityBytes >
            std::numeric_limits<std::size_t>::max() - (*specificationBytes + candidateBytes) ||
        *candidateIdentitySize >
            std::numeric_limits<std::size_t>::max() -
                (*specificationBytes + candidateBytes + *specificationIdentityBytes) ||
        outputPayloadBytes > std::numeric_limits<std::size_t>::max() -
                                 (*specificationBytes + candidateBytes +
                                  *specificationIdentityBytes + *candidateIdentitySize)) {
        result.status = Status::InconclusiveResourceExhausted;
        result.diagnostic = "logical retained-memory estimate overflowed";
        return result;
    }
    result.work.retainedBytesEstimate = *specificationBytes + candidateBytes +
                                        *specificationIdentityBytes + *candidateIdentitySize +
                                        outputPayloadBytes;
    if (result.work.retainedBytesEstimate > limits.maxRetainedBytes) {
        result.status = Status::InconclusiveResourceExhausted;
        result.diagnostic = "logical retained-memory allowance exhausted";
        return result;
    }

    // Duplicate validation is part of the immutable specification contract.
    // Each visited input coordinate is a charged comparison; a zero-dimensional
    // tuple still costs one tuple comparison so empty-input duplicates are bounded.
    for (std::size_t i = 0; i < rows.size(); ++i) {
        for (std::size_t j = 0; j < i; ++j) {
            ++result.work.duplicateTupleComparisons;
            bool equal = true;
            if (specification.dimensions() == 0) {
                if (result.work.specificationComparisons == limits.maxSpecificationComparisons) {
                    result.status = Status::InconclusiveResourceExhausted;
                    result.diagnostic = "specification comparison budget exhausted";
                    return result;
                }
                ++result.work.specificationComparisons;
            }
            for (std::size_t coordinate = 0; coordinate < specification.dimensions();
                 ++coordinate) {
                if (result.work.specificationComparisons == limits.maxSpecificationComparisons) {
                    result.status = Status::InconclusiveResourceExhausted;
                    result.diagnostic = "specification comparison budget exhausted";
                    return result;
                }
                ++result.work.specificationComparisons;
                if (rows[j].inputs[coordinate] != rows[i].inputs[coordinate]) {
                    equal = false;
                    break;
                }
            }
            if (equal)
                return invalid("specification contains a duplicate input tuple");
        }
    }

    try {
        if (candidate.dimensions() != specification.dimensions() ||
            !candidate.outerBindings().empty() || candidate.type() != outputType) {
            result.status = Status::InvalidCandidate;
            result.diagnostic =
                "candidate must be a closed expression with matching dimensions and output type";
            return result;
        }
    } catch (const std::logic_error&) {
        result.status = Status::InvalidCandidate;
        result.diagnostic = "candidate expression is not executable";
        return result;
    }

    result.specificationIdentity = specification.identity();
    result.candidateIdentity = candidateIdentity(candidate, *candidateIdentitySize);
    result.work.peakRowWorkspaceAllowanceBytes =
        std::min(limits.evaluation.maxWorkspaceBytes, limits.maxPeakRowWorkspaceBytes);
    if (result.work.peakRowWorkspaceAllowanceBytes == 0) {
        result.status = Status::InconclusiveResourceExhausted;
        result.diagnostic = "per-row workspace allowance is zero";
        return result;
    }

    for (std::size_t ordinal = 0; ordinal < rows.size(); ++ordinal) {
        if (stop()) {
            result.status = Status::InconclusiveCancelled;
            result.diagnostic = "validation cancelled before complete finite-domain check";
            return result;
        }
        auto evaluation = limits.evaluation;
        evaluation.maxNodeVisits =
            std::min(evaluation.maxNodeVisits, limits.maxNodeVisits - result.work.nodeVisits);
        evaluation.maxOperations =
            std::min(evaluation.maxOperations, limits.maxOperations - result.work.operations);
        evaluation.maxAggregateIterations = std::min(evaluation.maxAggregateIterations,
                                                     limits.maxIterations - result.work.iterations);
        evaluation.maxWorkspaceBytes = result.work.peakRowWorkspaceAllowanceBytes;
        ++result.work.rowsAttempted;
        auto evaluated = candidate.evaluateWithCancellation(rows[ordinal].inputs, evaluation, stop);
        // The per-row evaluator allowances above guarantee these sums cannot
        // exceed the configured cumulative limits; keep overflow checks as a
        // defensive invariant if evaluator accounting changes later.
        if (!addWithin(result.work.nodeVisits, evaluated.nodeVisits, limits.maxNodeVisits) ||
            !addWithin(result.work.operations, evaluated.operations, limits.maxOperations) ||
            !addWithin(result.work.iterations, evaluated.iterations, limits.maxIterations) ||
            evaluated.iterationOperations >
                std::numeric_limits<std::size_t>::max() - result.work.iterationOperations) {
            result.status = Status::InconclusiveResourceExhausted;
            result.diagnostic = "evaluator work accounting exceeded the admitted allowance";
            return result;
        }
        result.work.nodeVisits += evaluated.nodeVisits;
        result.work.operations += evaluated.operations;
        result.work.iterations += evaluated.iterations;
        result.work.iterationOperations += evaluated.iterationOperations;
        if (evaluated.status == DiscreteEvaluationStatus::ResourceExhausted) {
            result.status = Status::InconclusiveResourceExhausted;
            result.evaluatorStatus = evaluated.status;
            result.diagnostic = evaluated.reason;
            return result;
        }
        if (evaluated.status == DiscreteEvaluationStatus::Cancelled) {
            result.status = Status::InconclusiveCancelled;
            result.evaluatorStatus = evaluated.status;
            result.diagnostic = evaluated.reason;
            return result;
        }
        if (evaluated.status != DiscreteEvaluationStatus::Valid || !evaluated.value) {
            result.status = Status::InvalidCandidate;
            result.evaluatorStatus = evaluated.status;
            result.evaluationFailure = DiscreteSpecificationEvaluationFailure{
                ordinal, rows[ordinal].inputs, evaluated.status, *result.specificationIdentity,
                *result.candidateIdentity};
            result.diagnostic = evaluated.reason && *evaluated.reason
                                    ? evaluated.reason
                                    : "candidate evaluation was mathematically invalid";
            return result;
        }
        if (*evaluated.value != rows[ordinal].expected) {
            result.status = Status::CounterexampleFound;
            result.counterexample = DiscreteSpecificationCounterexample{
                ordinal,          rows[ordinal].inputs,          rows[ordinal].expected,
                *evaluated.value, *result.specificationIdentity, *result.candidateIdentity};
            result.diagnostic = "candidate differs from the finite specification";
            return result;
        }
        ++result.work.rowsMatched;
    }

    DiscreteSpecificationCertificate certificate;
    certificate.specificationIdentity = std::move(*result.specificationIdentity);
    certificate.candidateIdentity = std::move(*result.candidateIdentity);
    result.specificationIdentity.reset();
    result.candidateIdentity.reset();
    certificate.declaredRows = rows.size();
    certificate.checkedRows = result.work.rowsMatched;
    certificate.work = result.work;
    result.status = Status::ValidatedFiniteDomain;
    result.certificate = std::move(certificate);
    result.diagnostic = "all declared finite-domain observations matched exactly";
    return result;
}

inline DiscreteSpecificationValidationResult
validateDiscreteSpecification(const DiscreteSpecification& specification,
                              const DiscreteExpression& candidate,
                              DiscreteSpecificationValidationLimits limits = {},
                              const std::atomic<bool>* cancelled = nullptr) {
    return validateDiscreteSpecificationWithStop(specification, candidate, limits, [&] {
        return cancelled && cancelled->load(std::memory_order_relaxed);
    });
}
