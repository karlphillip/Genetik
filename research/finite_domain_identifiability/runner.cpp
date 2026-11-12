#include <array>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "FiniteDomainIdentifiability.hpp"

namespace fdi = genetik_fdi;

std::string escape(std::string_view value) {
    std::string out;
    for (char c : value) {
        if (c == '"' || c == '\\') { out.push_back('\\'); out.push_back(c); }
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out.push_back(c);
    }
    return out;
}

void strings(std::ostream& out, const std::vector<std::string>& values) {
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out << ',';
        out << '"' << escape(values[i]) << '"';
    }
    out << ']';
}

void ids(std::ostream& out, const std::vector<std::string>& values) { strings(out, values); }
void validations(std::ostream& out, const std::vector<fdi::ValidationEvent>& events) {
    out << '[';
    for (std::size_t i = 0; i < events.size(); ++i) {
        if (i) out << ',';
        const auto& e = events[i];
        out << "{\"candidate_id\":\"" << escape(e.candidateId) << "\",\"status\":\""
            << discreteSpecificationValidationStatusName(e.status) << "\",\"rows_attempted\":" << e.rowsAttempted
            << ",\"rows_matched\":" << e.rowsMatched << ",\"node_visits\":" << e.nodeVisits
            << ",\"operations\":" << e.operations << ",\"iterations\":" << e.iterations
            << ",\"duplicate_tuple_comparisons\":" << e.duplicateTupleComparisons
            << ",\"specification_comparisons\":" << e.specificationComparisons
            << ",\"mismatch_ordinal\":";
        if (e.mismatchOrdinal) out << *e.mismatchOrdinal; else out << "null";
        out << ",\"mismatch_input\":";
        if (e.mismatchInput) out << *e.mismatchInput; else out << "null";
        out << ",\"mismatch_expected\":";
        if (e.mismatchExpected) out << *e.mismatchExpected; else out << "null";
        out << ",\"mismatch_actual\":";
        if (e.mismatchActual) out << *e.mismatchActual; else out << "null";
        out << '}';
    }
    out << ']';
}

bool safeId(const std::string& s) {
    if (s.empty()) return false;
    for (unsigned char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    return true;
}

bool parseSize(const char* text, std::size_t& value) {
    const std::string_view view(text);
    const auto result = std::from_chars(view.data(), view.data() + view.size(), value);
    return result.ec == std::errc{} && result.ptr == view.data() + view.size();
}

bool parseInt64(std::string_view text, std::int64_t& value) {
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

void printValue(std::ostream& out, DiscreteValue value) {
    out << "{\"type\":\"int64\",\"value\":" << value.integer() << '}';
}

int main(int argc, char** argv) {
    std::string runId, control, policyName, trainingFile;
    std::size_t seedMetadata = 0;
    if ((argc - 1) % 2 != 0) return 2;
    std::vector<std::string> seenArguments;
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::string arg(argv[i]);
        if (std::find(seenArguments.begin(), seenArguments.end(), arg) != seenArguments.end()) return 2;
        seenArguments.push_back(arg);
        if (arg == "--run-id") runId = argv[i + 1];
        else if (arg == "--catalogue") control = argv[i + 1];
        else if (arg == "--policy") policyName = argv[i + 1];
        else if (arg == "--seed-metadata") { if (!parseSize(argv[i + 1], seedMetadata)) return 2; }
        else if (arg == "--training-file") trainingFile = argv[i + 1];
        else return 2;
    }
    if (argc < 2 || !safeId(runId) || trainingFile.empty() ||
        (policyName != "disagreement" && policyName != "ascending")) return 2;
    if (control != "arithmetic" && control != "modular" && control != "threshold" &&
        control != "conditional-modular" && control != "ambiguity" && control != "outside") return 2;

    std::vector<std::int64_t> domain;
    for (std::int64_t x = -8; x <= 8; ++x) domain.push_back(x);
    std::vector<std::int64_t> initialInputs;
    if (control == "arithmetic" || control == "ambiguity" || control == "outside") initialInputs = {0};
    else if (control == "modular") initialInputs = {0, 1};
    else if (control == "threshold") initialInputs = {0, 1, 2};
    else initialInputs = {0, 2, 3};
    std::vector<DiscreteSpecificationObservation> initial;
    std::ifstream input(trainingFile);
    if (!input) return 2;
    std::string line;
    if (!std::getline(input, line) || line != "genetik-fdi-training-v1") return 2;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        std::istringstream row(line);
        std::int64_t x = 0, y = 0;
        std::string trailing;
        if (!(row >> x >> y) || (row >> trailing)) return 2;
        initial.push_back({{x}, DiscreteValue::integer(y)});
    }
    if (initial.size() != initialInputs.size()) return 2;
    for (std::size_t i = 0; i < initial.size(); ++i)
        if (initial[i].inputs[0] != initialInputs[i]) return 2;

    const auto candidates = fdi::catalogue(control == "outside" ? "arithmetic" : control);
    fdi::Limits limits;
    limits.maxChargedWork = 99936; // Reserve at most 64 controller-side oracle operations.
    fdi::Selector selector(candidates, std::move(initial), std::move(domain),
        policyName == "disagreement" ? fdi::Policy::Disagreement : fdi::Policy::Ascending, limits);
    auto transcript = selector.run();
    std::size_t queryOrdinal = 0;
    while (const auto query = selector.pendingQuery()) {
        ++queryOrdinal;
        const auto queryId = "q" + std::to_string(queryOrdinal);
        std::cout << "{\"type\":\"query\",\"id\":\"" << queryId
                  << "\",\"payload\":{\"input\":" << *query << "}}\n" << std::flush;
        std::string responseLine;
        if (!std::getline(std::cin, responseLine)) return 3;
        const std::string prefix = "{\"type\":\"response\",\"id\":\"" + queryId + "\",\"value\":";
        if (!responseLine.starts_with(prefix) || responseLine.empty() || responseLine.back() != '}') return 3;
        const std::string_view valueText(responseLine.data() + prefix.size(),
                                         responseLine.size() - prefix.size() - 1);
        std::int64_t value = 0;
        if (!parseInt64(valueText, value)) return 3;
        if (!selector.observe(*query, DiscreteValue::integer(value))) break;
    }
    transcript = selector.transcript();

    std::cout << "{\"type\":\"final\",\"id\":\"final\",\"transcript\":{\"run_id\":\"" << escape(runId) << "\",\"control\":\"" << escape(control)
              << "\",\"policy\":\"" << fdi::policyName(transcript.policy)
              << "\",\"seed_metadata\":" << seedMetadata << ",\"stop_reason\":\""
              << fdi::stopName(transcript.stopReason) << "\",\"diagnostic\":\""
              << escape(transcript.diagnostic) << "\",\"domain\":[";
    for (std::size_t i = 0; i < transcript.domain.size(); ++i) { if (i) std::cout << ','; std::cout << transcript.domain[i]; }
    std::cout << "],\"initial_observations\":[";
    for (std::size_t i = 0; i < transcript.initialObservations.size(); ++i) {
        if (i) std::cout << ',';
        std::cout << "{\"input\":" << transcript.initialObservations[i].first << ",\"expected\":";
        printValue(std::cout, transcript.initialObservations[i].second);
        std::cout << '}';
    }
    std::cout << "],\"candidate_ids\":";
    strings(std::cout, transcript.candidateIds);
    std::cout << ",\"candidate_expressions\":[";
    for (std::size_t i = 0; i < transcript.candidateExpressions.size(); ++i) {
        if (i) std::cout << ',';
        std::cout << '"' << escape(transcript.candidateExpressions[i]) << '"';
    }
    std::cout << "],\"signature_checks\":";
    validations(std::cout, transcript.signatureChecks);
    std::cout << ",\"initial_compatibility\":";
    validations(std::cout, transcript.initialCompatibility);
    std::cout << ",\"steps\":[";
    for (std::size_t i = 0; i < transcript.steps.size(); ++i) {
        if (i) std::cout << ',';
        const auto& step = transcript.steps[i];
        const auto queryId = "q" + std::to_string(i + 1);
        std::cout << "{\"input\":" << step.input << ",\"query_id\":\"" << queryId
                  << "\",\"revision\":" << (i + 1) << ",\"revision_id\":\"revision_"
                  << (i + 1) << "\",\"predecessor_specification_identity\":\""
                  << escape(step.predecessorSpecificationIdentity) << "\",\"counterexample\":{\"query_id\":\""
                  << queryId << "\",\"observation\":{\"inputs\":[" << step.input
                  << "],\"expected\":{\"type\":\"int64\",\"value\":" << step.response.integer()
                  << "}}},\"response\":";
        printValue(std::cout, step.response);
        std::cout << ",\"largest_partition\":" << step.largestPartition << ",\"partitions\":[";
        for (std::size_t j = 0; j < step.partitions.size(); ++j) {
            if (j) std::cout << ',';
            std::cout << "{\"value\":";
            printValue(std::cout, step.partitions[j].value);
            std::cout << ",\"classes\":";
            strings(std::cout, step.partitions[j].behaviorClassIds);
            std::cout << '}';
        }
        std::cout << "],\"eliminated_candidate_ids\":"; ids(std::cout, step.eliminatedCandidateIds);
        std::cout << ",\"compatible_candidate_ids\":"; ids(std::cout, step.compatibleCandidateIds);
        std::cout << ",\"compatible_behavior_class_ids\":"; ids(std::cout, step.compatibleBehaviorClassIds);
        std::cout << ",\"specification_identity\":\"" << escape(step.specificationIdentity)
                  << "\",\"validations\":";
        validations(std::cout, step.validations);
        std::cout << '}';
    }
    const auto& w = transcript.work;
    std::cout << "],\"final_compatible_candidate_ids\":"; ids(std::cout, transcript.finalCompatibleCandidateIds);
    std::cout << ",\"final_behavior_class_ids\":"; ids(std::cout, transcript.finalBehaviorClassIds);
    std::cout << ",\"specification_identity\":\"" << escape(transcript.specificationIdentity)
              << "\",\"work\":{\"catalogue_candidates\":" << w.catalogueCandidates
              << ",\"discovered_structures\":" << w.discoveredStructures
              << ",\"candidate_row_evaluations\":" << w.candidateRowEvaluations
              << ",\"signature_comparisons\":" << w.signatureComparisons
              << ",\"partition_comparisons\":" << w.partitionComparisons
              << ",\"validator_rows_attempted\":" << w.validatorRowsAttempted
              << ",\"validator_rows_matched\":" << w.validatorRowsMatched
              << ",\"duplicate_tuple_comparisons\":" << w.duplicateTupleComparisons
              << ",\"specification_comparisons\":" << w.specificationComparisons
              << ",\"node_visits\":" << w.nodeVisits << ",\"operations\":" << w.operations
              << ",\"iterations\":" << w.iterations << ",\"iteration_operations\":" << w.iterationOperations
              << ",\"initial_oracle_queries\":" << w.initialOracleQueries
              << ",\"oracle_queries\":" << w.oracleQueries
              << ",\"charged_work\":" << w.chargedWork
              << ",\"logical_storage_bytes\":" << w.logicalStorageBytes << "}}}\n";
}
