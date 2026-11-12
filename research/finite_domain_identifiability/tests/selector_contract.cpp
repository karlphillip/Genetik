#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../FiniteDomainIdentifiability.hpp"

namespace fdi = genetik_fdi;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<std::int64_t> domain() {
    std::vector<std::int64_t> values;
    for (std::int64_t x = -8; x <= 8; ++x) values.push_back(x);
    return values;
}

DiscreteSpecificationObservation row(std::int64_t x, std::int64_t y) {
    return {{x}, DiscreteValue::integer(y)};
}

void ambiguity_is_finite_only() {
    const auto candidates = fdi::catalogue("ambiguity");
    fdi::Selector selector(candidates, {row(0, 0)}, domain(), fdi::Policy::Disagreement);
    const auto transcript = selector.run();
    require(transcript.stopReason == fdi::StopReason::OneFiniteBehaviorClass,
            "ambiguous catalogue should stop at one finite behavior class");
    require(transcript.finalCompatibleCandidateIds.size() == 3,
            "structural representatives must survive finite signature equality");
    require(transcript.finalBehaviorClassIds.size() == 1,
            "ambiguity candidates should share a signature on the declared domain");
    require(transcript.steps.empty(), "finite-equivalent candidates need no query");
}

void arithmetic_counterexample_refinement_is_authenticated() {
    const auto candidates=fdi::catalogue("arithmetic");
    fdi::Selector selector(candidates,{row(0,2)},domain(),fdi::Policy::Disagreement);
    auto before=selector.run();
    require(before.stopReason==fdi::StopReason::Active,"arithmetic fixture should need refinement");
    require(selector.pendingQuery()==std::optional<std::int64_t>(-8),
            "disagreement tie-break must choose the frozen first input");
    const auto predecessor=before.specificationIdentity;
    require(selector.observe(-8,DiscreteValue::integer(-6)),
            "independently computed n+2 response must be accepted");
    const auto& after=selector.transcript();
    require(after.steps.size()==1,"one counterexample must create one revision");
    require(after.steps.front().predecessorSpecificationIdentity==predecessor,
            "revision must bind the exact predecessor specification");
    require(after.stopReason==fdi::StopReason::OneFiniteBehaviorClass,
            "counterexample should isolate one finite behavior class");
    require(after.finalCompatibleCandidateIds.size()==2,
            "finite-equivalent c0/c1 structures remain explicit");
}

void partitions_and_policy_tie_breaks() {
    const auto candidates = fdi::catalogue("modular");
    fdi::Selector disagree(candidates, {row(0, 0), row(1, 1)}, domain(), fdi::Policy::Disagreement);
    auto d = disagree.run();
    require(d.stopReason == fdi::StopReason::Active, "disagreement selector should remain active");
    require(d.steps.empty(), "queries are requested lazily");
    const auto dq = disagree.pendingQuery();
    require(dq.has_value(), "disagreement selector must propose a query");
    const auto dClassCount = d.finalBehaviorClassIds.size();
    require(dClassCount > 1, "modular signatures should remain distinguishable");

    fdi::Selector ascending(candidates, {row(0, 0), row(1, 1)}, domain(), fdi::Policy::Ascending);
    const auto a = ascending.run();
    require(ascending.pendingQuery() == std::optional<std::int64_t>(-8),
            "ascending policy must choose the smallest unobserved input");
    require(a.stopReason == fdi::StopReason::Active, "ascending selector should remain active");

    // Query partitions are recorded with the response, including for ascending baseline.
    const auto answer = static_cast<std::int64_t>((*dq % 3 + 3) % 3);
    require(disagree.observe(*dq, DiscreteValue::integer(answer)), "valid oracle answer should be accepted");
    const auto& after = disagree.transcript();
    require(after.steps.size() == 1, "one answer should create one transcript step");
    const auto& step = after.steps.front();
    require(!step.partitions.empty() && step.largestPartition > 0,
            "disagreement partition must be persisted");
    std::size_t partitioned = 0;
    for (const auto& p : step.partitions) partitioned += p.behaviorClassIds.size();
    require(partitioned == dClassCount, "partitions must cover every viable finite class");

    const auto aq = *ascending.pendingQuery();
    require(ascending.observe(aq, DiscreteValue::integer((aq % 3 + 3) % 3)),
            "ascending answer should be accepted");
    const auto& astep = ascending.transcript().steps.front();
    require(!astep.partitions.empty(), "ascending baseline must persist its input partition");
}

void outside_catalogue_is_empty_not_recovery() {
    fdi::Selector selector(fdi::catalogue("arithmetic"), {row(0, (std::int64_t{1} << 40))},
                           domain(), fdi::Policy::Disagreement);
    const auto transcript = selector.run();
    require(transcript.stopReason == fdi::StopReason::EmptyCompatibleSet,
            "outside-catalogue observation must eliminate every candidate");
    require(transcript.finalCompatibleCandidateIds.empty(), "outside catalogue must remain explicit");
    require(transcript.finalBehaviorClassIds.empty(), "outside catalogue cannot report finite identification");
}

void resource_exhaustion_is_not_success() {
    fdi::Limits limits;
    limits.maxCandidateRowEvaluations = 1;
    fdi::Selector selector(fdi::catalogue("arithmetic"), {row(0, 2)}, domain(),
                           fdi::Policy::Disagreement, limits);
    const auto transcript = selector.run();
    require(transcript.stopReason == fdi::StopReason::ResourceExhausted,
            "candidate-row exhaustion must be explicit");
    require(transcript.diagnostic.find("allowance") != std::string::npos,
            "resource stop must retain a deterministic reason");
}

int main() {
    try {
        ambiguity_is_finite_only();
        arithmetic_counterexample_refinement_is_authenticated();
        partitions_and_policy_tie_breaks();
        outside_catalogue_is_empty_not_recovery();
        resource_exhaustion_is_not_success();
    } catch (const std::exception& error) {
        std::cerr << "selector contract failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "selector contract passed\n";
    return 0;
}
