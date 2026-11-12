#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <DiscreteExpressionSpecification.hpp>

namespace genetik_fdi {

enum class Policy { Disagreement, Ascending };
enum class StopReason {
    Active, OneFiniteBehaviorClass, EmptyCompatibleSet, QueryCap, ResourceExhausted,
    Cancelled, InvalidCatalogue
};

inline const char* policyName(Policy policy) noexcept {
    return policy == Policy::Disagreement ? "disagreement" : "ascending";
}
inline const char* stopName(StopReason reason) noexcept {
    switch (reason) {
    case StopReason::Active: return "active";
    case StopReason::OneFiniteBehaviorClass: return "one_finite_behavior_class";
    case StopReason::EmptyCompatibleSet: return "empty_compatible_set";
    case StopReason::QueryCap: return "query_cap";
    case StopReason::ResourceExhausted: return "resource_exhausted";
    case StopReason::Cancelled: return "cancelled";
    case StopReason::InvalidCatalogue: return "invalid_catalogue";
    }
    return "invalid_catalogue";
}

struct Candidate {
    std::string id;
    DiscreteExpression expression;
};

struct Limits {
    std::size_t maxCandidates = 8;
    std::size_t maxNodesPerExpression = 21;
    std::size_t maxDepthPerExpression = 6;
    std::size_t maxQueries = 8;
    std::size_t maxCandidateRowEvaluations = 1024;
    std::size_t maxChargedWork = 100000;
    std::size_t maxLogicalStorageBytes = 1024 * 1024;
    std::size_t maxNodeVisits = 100000;
    std::size_t maxOperations = 100000;
    std::size_t maxIterations = 100000;
    std::size_t maxSpecificationComparisons = 100000;
    std::size_t maxRowWorkspaceBytes = 1024 * 1024;
};

struct Work {
    std::size_t catalogueCandidates = 0;
    std::size_t discoveredStructures = 0;
    std::size_t candidateRowEvaluations = 0;
    std::size_t signatureComparisons = 0;
    std::size_t partitionComparisons = 0;
    std::size_t validatorRowsAttempted = 0;
    std::size_t validatorRowsMatched = 0;
    std::size_t duplicateTupleComparisons = 0;
    std::size_t specificationComparisons = 0;
    std::size_t nodeVisits = 0;
    std::size_t operations = 0;
    std::size_t iterations = 0;
    std::size_t iterationOperations = 0;
    std::size_t initialOracleQueries = 0;
    std::size_t oracleQueries = 0;
    std::size_t chargedWork = 0;
    std::size_t logicalStorageBytes = 0;
};

struct ValidationEvent {
    std::string candidateId;
    DiscreteSpecificationValidationStatus status =
        DiscreteSpecificationValidationStatus::InvalidSpecification;
    std::size_t rowsAttempted = 0;
    std::size_t rowsMatched = 0;
    std::size_t nodeVisits = 0;
    std::size_t operations = 0;
    std::size_t iterations = 0;
    std::size_t duplicateTupleComparisons = 0;
    std::size_t specificationComparisons = 0;
    std::optional<std::size_t> mismatchOrdinal;
    std::optional<std::int64_t> mismatchInput;
    std::optional<std::int64_t> mismatchExpected;
    std::optional<std::int64_t> mismatchActual;
};

struct Partition {
    DiscreteValue value = DiscreteValue::integer(0);
    std::vector<std::string> behaviorClassIds;
};

struct QueryStep {
    std::int64_t input = 0;
    std::size_t largestPartition = 0;
    std::vector<Partition> partitions;
    DiscreteValue response = DiscreteValue::integer(0);
    std::vector<std::string> eliminatedCandidateIds;
    std::vector<std::string> compatibleCandidateIds;
    std::vector<std::string> compatibleBehaviorClassIds;
    std::vector<ValidationEvent> validations;
    std::string predecessorSpecificationIdentity;
    std::string specificationIdentity;
};

struct Transcript {
    Policy policy = Policy::Disagreement;
    StopReason stopReason = StopReason::InvalidCatalogue;
    std::vector<std::int64_t> domain;
    std::vector<std::pair<std::int64_t, DiscreteValue>> initialObservations;
    std::vector<std::string> candidateIds;
    std::vector<std::string> candidateExpressions;
    std::vector<ValidationEvent> signatureChecks;
    std::vector<ValidationEvent> initialCompatibility;
    std::vector<QueryStep> steps;
    std::vector<std::string> finalCompatibleCandidateIds;
    std::vector<std::string> finalBehaviorClassIds;
    std::string specificationIdentity;
    Work work;
    std::string diagnostic;
};

inline DiscreteExpressionLimits expressionLimits() {
    DiscreteExpressionLimits limits;
    limits.maxNodes = 21;
    limits.maxDepth = 6;
    limits.maxVariables = 1;
    limits.maxConstants = 64;
    limits.maxStorageBytes = 1024 * 1024;
    return limits;
}

inline DiscreteExpression var() { return DiscreteExpression::variable(0, 1, expressionLimits()); }
inline DiscreteExpression lit(std::int64_t value) { return DiscreteExpression::integer(value, 1, expressionLimits()); }
inline DiscreteExpression bin(DiscreteKind kind, const DiscreteExpression& left,
                              const DiscreteExpression& right) {
    return DiscreteExpression::binary(kind, left, right, expressionLimits());
}
inline DiscreteExpression ite(const DiscreteExpression& condition, const DiscreteExpression& yes,
                              const DiscreteExpression& no) {
    return DiscreteExpression::conditional(condition, yes, no, expressionLimits());
}

inline std::vector<Candidate> catalogue(const std::string& family) {
    const auto n = var();
    const auto zero = lit(0), one = lit(1), two = lit(2), three = lit(3), five = lit(5);
    const auto plusZero = [&](const DiscreteExpression& e) { return bin(DiscreteKind::Add, e, zero); };
    std::vector<DiscreteExpression> base;
    if (family == "arithmetic") {
        base.push_back(bin(DiscreteKind::Add, n, two));
        base.push_back(bin(DiscreteKind::Add, bin(DiscreteKind::Multiply, two, n), two));
        base.push_back(bin(DiscreteKind::Add, bin(DiscreteKind::Multiply, n, n), two));
        base.push_back(two);
    } else if (family == "modular") {
        base.push_back(bin(DiscreteKind::Modulo, n, three));
        base.push_back(bin(DiscreteKind::Modulo, n, five));
        base.push_back(bin(DiscreteKind::Modulo, bin(DiscreteKind::Modulo, n, three), two));
        base.push_back(bin(DiscreteKind::Modulo, n, two));
    } else if (family == "threshold") {
        const auto negative = bin(DiscreteKind::Subtract, zero, n);
        base.push_back(ite(bin(DiscreteKind::LessThan, n, zero), negative, n));
        base.push_back(n);
        base.push_back(ite(bin(DiscreteKind::LessThan, n, lit(-1)), negative, n));
        base.push_back(ite(bin(DiscreteKind::LessThan, n, zero),
                           bin(DiscreteKind::Add, negative, one), n));
    } else if (family == "conditional-modular") {
        const auto q = bin(DiscreteKind::Subtract, bin(DiscreteKind::Multiply, n, n), two);
        const auto predicate = bin(DiscreteKind::Equal, bin(DiscreteKind::Modulo, n, three), one);
        base.push_back(ite(predicate, bin(DiscreteKind::Add, n, two), q));
        base.push_back(ite(predicate, bin(DiscreteKind::Add, n, three), q));
        base.push_back(ite(predicate, bin(DiscreteKind::Multiply, two, n), q));
        base.push_back(q);
    } else if (family == "ambiguity") {
        base.push_back(n);
        base.push_back(bin(DiscreteKind::Add, n, zero));
        base.push_back(ite(bin(DiscreteKind::GreaterThan, n, lit(8)),
                           bin(DiscreteKind::Add, n, one), n));
    } else {
        return {};
    }
    std::vector<Candidate> result;
    for (std::size_t i = 0; i < base.size(); ++i) {
        result.push_back({"c" + std::to_string(i), base[i]});
        if (family != "ambiguity")
            result.push_back({"c" + std::to_string(i) + "_plus0", plusZero(base[i])});
    }
    return result;
}

class Selector {
public:
    Selector(std::vector<Candidate> candidates,
             std::vector<DiscreteSpecificationObservation> initial,
             std::vector<std::int64_t> domain, Policy policy, Limits limits = {})
        : candidates_(std::move(candidates)), known_(std::move(initial)), domain_(std::move(domain)),
          policy_(policy), limits_(limits) {}

    Transcript run() {
        transcript_.policy = policy_;
        transcript_.stopReason = StopReason::Active;
        transcript_.domain = domain_;
        transcript_.work.catalogueCandidates = candidates_.size();
        if (!admitInputs()) return finish();
        if (!buildSignatures()) return finish();
        if (!revalidate(true)) return finish();
        updateStop();
        prepareNextQuery();
        return finish();
    }

    // Continue one oracle turn. The reference identity never enters this object;
    // callers may only provide the queried input's exact response.
    bool observe(std::int64_t input, DiscreteValue response) {
        if (transcript_.stopReason != StopReason::Active || !nextQuery_ || *nextQuery_ != input)
            return false;
        if (response.type() != DiscreteType::Int64) return fail(StopReason::InvalidCatalogue,
                                                                "oracle response type is not Int64");
        QueryStep step;
        step.input = input;
        step.response = response;
        step.predecessorSpecificationIdentity = transcript_.specificationIdentity;
        step.partitions = lastPartitions_;
        step.largestPartition = lastLargestPartition_;
        if (!chargeWork(1 + step.predecessorSpecificationIdentity.size())) return false;
        const auto stepBytes = sizeof(QueryStep) + sizeof(DiscreteSpecificationObservation) +
            sizeof(std::int64_t) + step.partitions.size() * sizeof(Partition) +
            step.predecessorSpecificationIdentity.size() + 1;
        if (!chargeStorage(stepBytes)) return false;
        ++transcript_.work.oracleQueries;
        known_.push_back({{input}, response});
        transcript_.steps.push_back(std::move(step));
        nextQuery_.reset();
        if (!revalidate(false)) return false;
        auto& stored = transcript_.steps.back();
        stored.compatibleCandidateIds = viableCandidateIds_;
        stored.compatibleBehaviorClassIds = viableClassIds_;
        stored.validations = compatibilityEvents_;
        stored.specificationIdentity = transcript_.specificationIdentity;
        updateStop();
        prepareNextQuery();
        return transcript_.stopReason != StopReason::ResourceExhausted &&
               transcript_.stopReason != StopReason::Cancelled;
    }

    std::optional<std::int64_t> pendingQuery() const noexcept { return nextQuery_; }
    const Transcript& transcript() const noexcept { return transcript_; }

private:
    struct Record {
        Candidate candidate;
        std::string expression;
        std::vector<DiscreteValue> signature;
        std::optional<std::size_t> behaviorClass;
        bool signatureValid = true;
    };

    bool chargeWork(std::size_t amount) {
        if (amount > limits_.maxChargedWork - std::min(transcript_.work.chargedWork,
                                                        limits_.maxChargedWork))
            return fail(StopReason::ResourceExhausted, "charged-work allowance exhausted");
        transcript_.work.chargedWork += amount;
        return true;
    }

    bool chargeRows(std::size_t amount = 1) {
        if (amount > limits_.maxCandidateRowEvaluations -
                         std::min(transcript_.work.candidateRowEvaluations,
                                  limits_.maxCandidateRowEvaluations))
            return fail(StopReason::ResourceExhausted, "candidate-row allowance exhausted");
        transcript_.work.candidateRowEvaluations += amount;
        return true;
    }

    bool chargeStorage(std::size_t amount) {
        if (amount > limits_.maxLogicalStorageBytes -
                         std::min(transcript_.work.logicalStorageBytes,
                                  limits_.maxLogicalStorageBytes))
            return fail(StopReason::ResourceExhausted, "logical-storage allowance exhausted");
        transcript_.work.logicalStorageBytes += amount;
        return true;
    }

    bool fail(StopReason reason, std::string message) {
        transcript_.stopReason = reason;
        transcript_.diagnostic = std::move(message);
        return false;
    }

    bool admitInputs() {
        if (candidates_.empty() || candidates_.size() > limits_.maxCandidates ||
            domain_.empty() || known_.empty())
            return fail(StopReason::InvalidCatalogue, "empty or oversized selector input");
        if (candidates_.size() > limits_.maxCandidateRowEvaluations ||
            domain_.size() > limits_.maxCandidateRowEvaluations / candidates_.size())
            return fail(StopReason::ResourceExhausted, "signature matrix exceeds candidate-row allowance");
        for (std::size_t i = 1; i < domain_.size(); ++i) {
            if (!chargeWork(1)) return false;
            if (domain_[i] <= domain_[i - 1])
                return fail(StopReason::InvalidCatalogue, "query domain must be strictly increasing");
            if (domain_[i - 1] == std::numeric_limits<std::int64_t>::max() ||
                domain_[i] != domain_[i - 1] + 1)
                return fail(StopReason::InvalidCatalogue, "frozen query domain must be contiguous");
        }
        if (domain_.size() > (std::numeric_limits<std::size_t>::max() - sizeof(Transcript)) /
                                 sizeof(std::int64_t))
            return fail(StopReason::ResourceExhausted, "query-domain storage overflow");
        std::size_t bytes = sizeof(Transcript) + domain_.size() * sizeof(std::int64_t);
        for (const auto& row : known_) {
            if (!chargeWork(1)) return false;
            if (row.inputs.size() != 1 || row.expected.type() != DiscreteType::Int64)
                return fail(StopReason::InvalidCatalogue, "selector requires one-dimensional Int64 rows");
            if (sizeof(DiscreteSpecificationObservation) + sizeof(std::int64_t) >
                std::numeric_limits<std::size_t>::max() - bytes)
                return fail(StopReason::ResourceExhausted, "initial specification storage overflow");
            bytes += sizeof(DiscreteSpecificationObservation) + sizeof(std::int64_t);
        }
        if (!chargeStorage(candidates_.size() * sizeof(Candidate))) return false;
        for (std::size_t candidateIndex = 0; candidateIndex < candidates_.size(); ++candidateIndex) {
            const auto& candidate = candidates_[candidateIndex];
            if (!chargeWork(1)) return false;
            if (candidate.id.empty() || candidate.expression.dimensions() != 1 ||
                candidate.expression.nodeCount() > limits_.maxNodesPerExpression ||
                candidate.expression.depth() > limits_.maxDepthPerExpression ||
                !candidate.expression.outerBindings().empty() ||
                candidate.expression.type() != DiscreteType::Int64)
                return fail(StopReason::InvalidCatalogue, "candidate violates frozen type or size limits");
            for (std::size_t previous = 0; previous < candidateIndex; ++previous) {
                if (!chargeWork(1)) return false;
                if (candidates_[previous].id == candidate.id)
                    return fail(StopReason::InvalidCatalogue, "candidate identifiers must be unique");
            }
            if (candidate.expression.nodeCount() >
                std::numeric_limits<std::size_t>::max() / DiscreteExpression::storageBytesPerNode())
                return fail(StopReason::ResourceExhausted, "candidate logical storage overflow");
            const auto candidateBytes = candidate.expression.nodeCount() *
                                        DiscreteExpression::storageBytesPerNode();
            if (candidateBytes > std::numeric_limits<std::size_t>::max() - bytes)
                return fail(StopReason::ResourceExhausted, "catalogue storage overflow");
            bytes += candidateBytes;
        }
        if (!chargeStorage(bytes)) return false;
        transcript_.work.initialOracleQueries = known_.size();
        if (!chargeWork(known_.size())) return false;
        for (const auto& candidate : candidates_) {
            constexpr std::size_t textBytesPerNode = 128;
            if (candidate.expression.nodeCount() >
                (std::numeric_limits<std::size_t>::max() - 16) / textBytesPerNode)
                return fail(StopReason::ResourceExhausted, "candidate text size overflow");
            const auto textReservation = candidate.expression.nodeCount() * textBytesPerNode + 16;
            if (!chargeWork(candidate.expression.nodeCount() + textReservation)) return false;
            if (!chargeStorage(textReservation + candidate.id.size() + 2 +
                               sizeof(std::string) + sizeof(std::vector<std::string>))) return false;
            const auto expression = candidate.expression.toString();
            if (expression.size() > textReservation)
                return fail(StopReason::ResourceExhausted, "candidate text exceeded reserved bound");
            transcript_.candidateIds.push_back(candidate.id);
            transcript_.candidateExpressions.push_back(expression);
        }
        return true;
    }

    DiscreteSpecificationValidationLimits validationLimits() const {
        DiscreteSpecificationValidationLimits limits;
        limits.maxRows = known_.size();
        limits.maxSpecificationComparisons = limits_.maxSpecificationComparisons -
            std::min(transcript_.work.specificationComparisons, limits_.maxSpecificationComparisons);
        limits.maxNodeVisits = limits_.maxNodeVisits -
            std::min(transcript_.work.nodeVisits, limits_.maxNodeVisits);
        limits.maxOperations = limits_.maxOperations -
            std::min(transcript_.work.operations, limits_.maxOperations);
        limits.maxIterations = limits_.maxIterations -
            std::min(transcript_.work.iterations, limits_.maxIterations);
        limits.maxRetainedBytes = limits_.maxLogicalStorageBytes -
            std::min(transcript_.work.logicalStorageBytes, limits_.maxLogicalStorageBytes);
        limits.maxPeakRowWorkspaceBytes = limits_.maxRowWorkspaceBytes;
        limits.evaluation.maxNodeVisits = limits.maxNodeVisits;
        limits.evaluation.maxOperations = limits.maxOperations;
        limits.evaluation.maxAggregateIterations = limits.maxIterations;
        limits.evaluation.maxIterationsPerOperation = limits_.maxIterations;
        limits.evaluation.maxWorkspaceBytes = limits_.maxRowWorkspaceBytes;
        return limits;
    }

    bool recordValidation(const Candidate& candidate,
                          const DiscreteSpecificationValidationResult& result,
                          std::vector<ValidationEvent>& sink) {
        const auto& work = result.work;
        const auto rows = work.rowsAttempted;
        if (!chargeRows(rows)) return false;
        const auto charged = work.duplicateTupleComparisons + work.specificationComparisons +
            work.nodeVisits + work.operations + work.iterations + rows +
            candidate.expression.nodeCount();
        if (!chargeWork(charged)) return false;
        if (!chargeStorage(work.retainedBytesEstimate)) return false;
        transcript_.work.validatorRowsAttempted += work.rowsAttempted;
        transcript_.work.validatorRowsMatched += work.rowsMatched;
        transcript_.work.duplicateTupleComparisons += work.duplicateTupleComparisons;
        transcript_.work.specificationComparisons += work.specificationComparisons;
        transcript_.work.nodeVisits += work.nodeVisits;
        transcript_.work.operations += work.operations;
        transcript_.work.iterations += work.iterations;
        transcript_.work.iterationOperations += work.iterationOperations;
        ValidationEvent event;
        event.candidateId = candidate.id;
        event.status = result.status;
        event.rowsAttempted = work.rowsAttempted;
        event.rowsMatched = work.rowsMatched;
        event.nodeVisits = work.nodeVisits;
        event.operations = work.operations;
        event.iterations = work.iterations;
        event.duplicateTupleComparisons = work.duplicateTupleComparisons;
        event.specificationComparisons = work.specificationComparisons;
        if (result.counterexample) {
            event.mismatchOrdinal = result.counterexample->ordinal;
            if (!result.counterexample->inputs.empty()) event.mismatchInput = result.counterexample->inputs[0];
            if (result.counterexample->expected.type() == DiscreteType::Int64)
                event.mismatchExpected = result.counterexample->expected.integer();
            if (result.counterexample->actual.type() == DiscreteType::Int64)
                event.mismatchActual = result.counterexample->actual.integer();
        }
        if (!chargeStorage(sizeof(ValidationEvent) + candidate.id.size() + 1 +
                           (event.mismatchInput ? sizeof(std::int64_t) : 0))) return false;
        sink.push_back(std::move(event));
        return true;
    }

    bool buildSignatures() {
        const std::size_t recordBytes = candidates_.size() * sizeof(Record);
        if (candidates_.size() > std::numeric_limits<std::size_t>::max() / sizeof(Record) ||
            !chargeStorage(recordBytes))
            return fail(StopReason::ResourceExhausted, "record storage allowance exhausted");
        records_.reserve(candidates_.size());
        for (const auto& candidate : candidates_) {
            if (!chargeStorage(candidate.expression.nodeCount() *
                               DiscreteExpression::storageBytesPerNode())) return false;
            Record record{candidate, candidate.expression.toString(), {}, std::nullopt, true};
            if (domain_.size() > std::numeric_limits<std::size_t>::max() / sizeof(DiscreteValue) ||
                !chargeStorage(domain_.size() * sizeof(DiscreteValue))) return false;
            record.signature.reserve(domain_.size());
            for (const auto inputValue : domain_) {
                if (!chargeRows()) return false;
                if (!chargeWork(candidate.expression.nodeCount() + 1)) return false;
                DiscreteEvaluationLimits eval;
                eval.maxNodeVisits = limits_.maxNodeVisits -
                    std::min(transcript_.work.nodeVisits, limits_.maxNodeVisits);
                eval.maxOperations = limits_.maxOperations -
                    std::min(transcript_.work.operations, limits_.maxOperations);
                eval.maxAggregateIterations = limits_.maxIterations -
                    std::min(transcript_.work.iterations, limits_.maxIterations);
                eval.maxIterationsPerOperation = limits_.maxIterations;
                eval.maxWorkspaceBytes = limits_.maxRowWorkspaceBytes;
                const std::array<std::int64_t, 1> input{inputValue};
                const auto prediction = candidate.expression.evaluate(input, eval);
                transcript_.work.nodeVisits += prediction.nodeVisits;
                transcript_.work.operations += prediction.operations;
                transcript_.work.iterations += prediction.iterations;
                transcript_.work.iterationOperations += prediction.iterationOperations;
                transcript_.work.chargedWork += prediction.nodeVisits + prediction.operations +
                                                prediction.iterations;
                if (transcript_.work.chargedWork > limits_.maxChargedWork)
                    return fail(StopReason::ResourceExhausted, "signature work allowance exhausted");
                if (prediction.status == DiscreteEvaluationStatus::ResourceExhausted ||
                    prediction.status == DiscreteEvaluationStatus::Cancelled)
                    return fail(prediction.status == DiscreteEvaluationStatus::Cancelled
                                    ? StopReason::Cancelled : StopReason::ResourceExhausted,
                                prediction.reason ? prediction.reason : "candidate signature incomplete");
                if (!prediction.valid() || !prediction.value) {
                    record.signatureValid = false;
                    break;
                }
                const auto expected = *prediction.value;
                const DiscreteSpecification oneRow(1, {{{inputValue}, expected}});
                const auto checked = validateDiscreteSpecification(oneRow, candidate.expression,
                                                                    validationLimits());
                if (!recordValidation(candidate, checked, transcript_.signatureChecks)) return false;
                if (checked.status == DiscreteSpecificationValidationStatus::InconclusiveCancelled)
                    return fail(StopReason::Cancelled, checked.diagnostic);
                if (checked.status == DiscreteSpecificationValidationStatus::InconclusiveResourceExhausted)
                    return fail(StopReason::ResourceExhausted, checked.diagnostic);
                if (checked.status != DiscreteSpecificationValidationStatus::ValidatedFiniteDomain) {
                    record.signatureValid = false;
                    break;
                }
                record.signature.push_back(expected);
            }
            if (record.signatureValid && record.signature.size() == domain_.size()) {
                std::size_t classIndex = 0;
                for (; classIndex < signatures_.size(); ++classIndex) {
                    bool equal = signatures_[classIndex].size() == record.signature.size();
                    for (std::size_t i = 0; equal && i < record.signature.size(); ++i) {
                        if (!chargeWork(1)) return false;
                        ++transcript_.work.signatureComparisons;
                        equal = signatures_[classIndex][i] == record.signature[i];
                    }
                    if (!equal) continue;
                    break;
                }
                if (classIndex == signatures_.size()) {
                    if (!chargeStorage(record.signature.size() * sizeof(DiscreteValue) +
                                       sizeof(std::vector<DiscreteValue>) + sizeof(std::string) + 8)) return false;
                    signatures_.push_back(record.signature);
                    classIds_.push_back("b" + std::to_string(classIndex));
                }
                record.behaviorClass = classIndex;
            }
            records_.push_back(std::move(record));
        }
        return true;
    }

    bool revalidate(bool initial) {
        std::size_t rowBytes = 0;
        for (const auto& row : known_) {
            const auto bytes = sizeof(DiscreteSpecificationObservation) +
                               row.inputs.size() * sizeof(std::int64_t);
            if (bytes > std::numeric_limits<std::size_t>::max() - rowBytes)
                return fail(StopReason::ResourceExhausted, "specification workspace size overflow");
            rowBytes += bytes;
        }
        if (rowBytes > std::numeric_limits<std::size_t>::max() / (initial ? 3U : 2U))
            return fail(StopReason::ResourceExhausted, "specification workspace size overflow");
        const auto copyBytes = rowBytes * (initial ? 3U : 2U);
        if (copyBytes > std::numeric_limits<std::size_t>::max() - sizeof(DiscreteSpecification) ||
            !chargeStorage(copyBytes + sizeof(DiscreteSpecification))) return false;
        std::vector<DiscreteSpecificationObservation> rows = known_;
        DiscreteSpecification specification(1, rows);
        if (initial) {
            transcript_.initialObservations.clear();
            for (const auto& row : known_)
                transcript_.initialObservations.push_back({row.inputs[0], row.expected});
        }
        std::vector<std::string> viable;
        std::vector<bool> viableClasses(signatures_.size(), false);
        auto& events = initial ? transcript_.initialCompatibility : compatibilityEvents_;
        events.clear();
        for (const auto& record : records_) {
            if (!record.signatureValid) continue;
            const auto checked = validateDiscreteSpecification(specification,
                                                              record.candidate.expression,
                                                              validationLimits());
            if (!recordValidation(record.candidate, checked, events)) return false;
            if (checked.status == DiscreteSpecificationValidationStatus::InconclusiveCancelled)
                return fail(StopReason::Cancelled, checked.diagnostic);
            if (checked.status == DiscreteSpecificationValidationStatus::InconclusiveResourceExhausted)
                return fail(StopReason::ResourceExhausted, checked.diagnostic);
            if (checked.status == DiscreteSpecificationValidationStatus::ValidatedFiniteDomain) {
                if (!chargeStorage(record.candidate.id.size() + sizeof(std::string) + 1)) return false;
                viable.push_back(record.candidate.id);
                viableClasses[*record.behaviorClass] = true;
            }
        }
        if (!initial && !transcript_.steps.empty()) {
            auto& step = transcript_.steps.back();
            for (const auto& record : records_) {
                if (!record.signatureValid) continue;
                bool wasViable = false;
                for (const auto& id : viableCandidateIds_) {
                    if (!chargeWork(1)) return false;
                    if (id == record.candidate.id) { wasViable = true; break; }
                }
                bool remainsViable = false;
                if (wasViable) {
                    for (const auto& id : viable) {
                        if (!chargeWork(1)) return false;
                        if (id == record.candidate.id) { remainsViable = true; break; }
                    }
                }
                if (wasViable && !remainsViable) {
                    step.eliminatedCandidateIds.push_back(record.candidate.id);
                    if (!chargeStorage(record.candidate.id.size() + 1)) return false;
                }
            }
        }
        viableCandidateIds_ = std::move(viable);
        viableClassIds_.clear();
        for (std::size_t i = 0; i < viableClasses.size(); ++i)
            if (viableClasses[i]) {
                if (!chargeStorage(classIds_[i].size() + 1)) return false;
                viableClassIds_.push_back(classIds_[i]);
            }
        const auto identityBytes = genetik_specification_detail::logicalIdentityBytes(specification);
        if (!identityBytes || !chargeWork(*identityBytes) || !chargeStorage(*identityBytes)) return false;
        transcript_.specificationIdentity = specification.identity();
        return true;
    }

    bool prepareNextQuery() {
        nextQuery_.reset();
        if (transcript_.stopReason != StopReason::Active) return true;
        if (transcript_.steps.size() >= limits_.maxQueries) {
            transcript_.stopReason = StopReason::QueryCap;
            return true;
        }
        const auto query = chooseQuery();
        if (!query) return fail(StopReason::ResourceExhausted, "no unobserved query input remains");
        nextQuery_ = query;
        return true;
    }

    void updateStop() {
        if (transcript_.stopReason == StopReason::ResourceExhausted ||
            transcript_.stopReason == StopReason::Cancelled ||
            transcript_.stopReason == StopReason::InvalidCatalogue)
            return;
        std::size_t finalBytes = viableCandidateIds_.size() * sizeof(std::string) +
                                 viableClassIds_.size() * sizeof(std::string);
        for (const auto& id : viableCandidateIds_) finalBytes += id.size() + 1;
        for (const auto& id : viableClassIds_) finalBytes += id.size() + 1;
        if (!chargeStorage(finalBytes)) return;
        transcript_.finalCompatibleCandidateIds = viableCandidateIds_;
        transcript_.finalBehaviorClassIds = viableClassIds_;
        if (viableCandidateIds_.empty()) transcript_.stopReason = StopReason::EmptyCompatibleSet;
        else if (viableClassIds_.size() <= 1) transcript_.stopReason = StopReason::OneFiniteBehaviorClass;
        else if (transcript_.steps.size() >= limits_.maxQueries) transcript_.stopReason = StopReason::QueryCap;
        else transcript_.stopReason = StopReason::Active;
    }

    std::optional<std::int64_t> chooseQuery() {
        std::vector<std::pair<std::int64_t, std::size_t>> unobserved;
        for (const auto x : domain_) {
            bool seen = false;
            for (const auto& row : known_) {
                if (!chargeWork(1)) return std::nullopt;
                if (!row.inputs.empty() && row.inputs[0] == x) { seen = true; break; }
            }
            if (!seen) {
                if (!chargeStorage(sizeof(std::pair<std::int64_t, std::size_t>))) return std::nullopt;
                unobserved.push_back({x, 0});
            }
        }
        if (unobserved.empty()) return std::nullopt;
        lastPartitions_.clear();
        lastLargestPartition_ = 0;
        std::optional<std::int64_t> bestInput;
        std::size_t bestLargest = std::numeric_limits<std::size_t>::max();
        std::vector<Partition> bestPartitions;
        for (const auto& [x, _] : unobserved) {
            if (viableClassIds_.size() > 0 &&
                viableClassIds_.size() > std::numeric_limits<std::size_t>::max() / sizeof(Partition))
                return fail(StopReason::ResourceExhausted, "partition storage estimate overflow"), std::nullopt;
            if (!chargeStorage(sizeof(Partition) * viableClassIds_.size())) return std::nullopt;
            std::vector<Partition> partitions;
            for (const auto& classId : viableClassIds_) {
                const auto classIndex = static_cast<std::size_t>(std::stoul(classId.substr(1)));
                const auto value = signatures_[classIndex][static_cast<std::size_t>(x - domain_.front())];
                auto found = partitions.end();
                for (auto it = partitions.begin(); it != partitions.end(); ++it) {
                    if (!chargeWork(1)) return std::nullopt;
                    ++transcript_.work.partitionComparisons;
                    if (it->value == value) { found = it; break; }
                }
                if (found == partitions.end()) {
                    if (!chargeStorage(sizeof(Partition) + classId.size() + 1)) return std::nullopt;
                    partitions.push_back({value, {classId}});
                } else {
                    if (!chargeStorage(classId.size() + 1)) return std::nullopt;
                    found->behaviorClassIds.push_back(classId);
                }
            }
            std::size_t largest = 0;
            for (const auto& partition : partitions) {
                if (!chargeWork(1)) return std::nullopt;
                largest = std::max(largest, partition.behaviorClassIds.size());
            }
            if (!bestInput || largest < bestLargest || (largest == bestLargest && x < *bestInput)) {
                bestInput = x;
                bestLargest = largest;
                if (!chargeStorage(partitions.size() * sizeof(Partition))) return std::nullopt;
                bestPartitions = std::move(partitions);
            }
            if (policy_ == Policy::Ascending) break;
        }
        lastPartitions_ = std::move(bestPartitions);
        lastLargestPartition_ = bestLargest;
        return bestInput;
    }

    Transcript finish() {
        if (transcript_.stopReason == StopReason::Active && nextQuery_)
            transcript_.diagnostic = "awaiting the next reference value for the selected finite query";
        return transcript_;
    }

    std::vector<Candidate> candidates_;
    std::vector<DiscreteSpecificationObservation> known_;
    std::vector<std::int64_t> domain_;
    Policy policy_;
    Limits limits_;
    Transcript transcript_;
    std::vector<Record> records_;
    std::vector<std::vector<DiscreteValue>> signatures_;
    std::vector<std::string> classIds_;
    std::vector<std::string> viableCandidateIds_;
    std::vector<std::string> viableClassIds_;
    std::vector<ValidationEvent> compatibilityEvents_;
    std::vector<Partition> lastPartitions_;
    std::size_t lastLargestPartition_ = 0;
    std::optional<std::int64_t> nextQuery_;
};

} // namespace genetik_fdi
