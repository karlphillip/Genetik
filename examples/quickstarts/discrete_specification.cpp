#include <DiscreteExpressionParsing.hpp>
#include <DiscreteExpressionSpecification.hpp>

#include <iostream>
#include <vector>

int main() {
    std::vector<DiscreteSpecificationObservation> rows;
    for (const std::int64_t x : {-2, -1, 0, 1, 2})
        rows.push_back({{x}, DiscreteValue::integer(x + 2)});
    const DiscreteSpecification specification(1, std::move(rows));
    const auto candidate = parseDiscreteExpression("x0 + 2", 1);
    const auto result = validateDiscreteSpecification(specification, candidate);

    std::cout << "status=" << discreteSpecificationValidationStatusName(result.status)
              << " rows=" << result.work.rowsMatched << " node_visits=" << result.work.nodeVisits
              << " operations=" << result.work.operations
              << " retained_bytes_estimate=" << result.work.retainedBytesEstimate
              << " row_workspace_allowance=" << result.work.peakRowWorkspaceAllowanceBytes << '\n';
    if (result.status != DiscreteSpecificationValidationStatus::ValidatedFiniteDomain ||
        !result.certificate)
        return 1;
    std::cout << "finite scope only: " << result.certificate->checkedRows << " of "
              << result.certificate->declaredRows << " declared observations matched.\n";
    return 0;
}
