#pragma once

#include "ScalarTemplateRegression.hpp"

#include <array>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <initializer_list>
#include <memory>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// A fixed expression tree with disjoint, independently restricted substitution
// occurrences. The source expressions at the sites are placeholders only.
class MultiComponentExpressions final : public std::vector<Expression> {
public:
    using std::vector<Expression>::vector;
    MultiComponentExpressions(std::vector<Expression> components)
        : std::vector<Expression>(std::move(components)) {}
    operator std::array<Expression, 2>() const {
        if (size() != 2)
            throw std::invalid_argument("two-site conversion requires exactly two components");
        return {at(0), at(1)};
    }
};

class MultiComponentRanges final : public std::vector<std::pair<std::size_t, std::size_t>> {
public:
    using std::vector<std::pair<std::size_t, std::size_t>>::vector;
    MultiComponentRanges(std::vector<std::pair<std::size_t, std::size_t>> ranges)
        : std::vector<std::pair<std::size_t, std::size_t>>(std::move(ranges)) {}
    operator std::array<std::pair<std::size_t, std::size_t>, 2>() const {
        if (size() != 2)
            throw std::invalid_argument("two-site range conversion requires exactly two ranges");
        return {at(0), at(1)};
    }
};

class MultiScalarExpressionTemplate final {
public:
    MultiScalarExpressionTemplate(const Expression& source, std::vector<std::size_t> roots,
                                  std::vector<ExpressionLimits> componentLimits,
                                  ExpressionLimits modelLimits,
                                  std::size_t maximumMaterializationBytes = 1'048'576)
        : maximumMaterializationBytes_(maximumMaterializationBytes),
          source_(checkedSource(source, modelLimits, maximumMaterializationBytes)),
          componentLimits_(componentLimits), modelLimits_(modelLimits) {
        if (maximumMaterializationBytes_ == 0)
            throw std::invalid_argument("template materialization allowance must be positive");
        if (roots.empty() || roots.size() != componentLimits_.size())
            throw std::invalid_argument(
                "template roots and component limits must be nonempty and aligned");
        for (const auto root : roots) {
            const auto range = expressionSubtree(source_, root);
            for (const auto& prior : ranges_)
                if (range.first < prior.second && prior.first < range.second)
                    throw std::invalid_argument(
                        "template sites must be disjoint subtree occurrences");
            roots_.push_back(root);
            ranges_.push_back(range);
        }
        frozenNodes_ = source_.nodeCount();
        for (const auto& range : ranges_)
            frozenNodes_ -= range.second - range.first;
        if (frozenNodes_ > modelLimits_.maxNodes ||
            modelLimits_.maxNodes - frozenNodes_ < roots_.size())
            throw std::invalid_argument("complete model limit leaves no room for every site");
        frozenWork_ = arithmeticCostOutsideSites();
        if (frozenWork_ > modelLimits_.maxArithmeticOperations ||
            modelLimits_.maxArithmeticOperations - frozenWork_ < roots_.size())
            throw std::invalid_argument("complete model work limit leaves no room for every site");
        for (auto& limits : componentLimits_) {
            (void)Expression::constant(0.0, source_.variableCount(), limits);
            limits.maxNodes = std::min(limits.maxNodes,
                                       modelLimits_.maxNodes - frozenNodes_ - (roots_.size() - 1));
            limits.maxArithmeticOperations =
                std::min(limits.maxArithmeticOperations,
                         modelLimits_.maxArithmeticOperations - frozenWork_ - (roots_.size() - 1));
            limits.maxAbsConstant = std::min(limits.maxAbsConstant, modelLimits_.maxAbsConstant);
            if (limits.maxNodes == 0 || limits.maxDepth == 0)
                throw std::invalid_argument("component limits must allow a terminal");
        }
        if (source_.nodeCount() > maximumMaterializationBytes_ / slotBytes)
            throw std::length_error("template source exceeds materialization allowance");
    }

    MultiScalarExpressionTemplate(const Expression& source, std::size_t firstRoot,
                                  std::size_t secondRoot,
                                  std::array<ExpressionLimits, 2> componentLimits,
                                  ExpressionLimits modelLimits,
                                  std::size_t maximumMaterializationBytes = 1'048'576)
        : MultiScalarExpressionTemplate(
              source, std::vector<std::size_t>{firstRoot, secondRoot},
              std::vector<ExpressionLimits>{componentLimits[0], componentLimits[1]}, modelLimits,
              maximumMaterializationBytes) {}

    std::size_t variableCount() const noexcept {
        return source_.variableCount();
    }
    std::size_t frozenNodeCount() const noexcept {
        return frozenNodes_;
    }
    std::size_t maximumMaterializationBytes() const noexcept {
        return maximumMaterializationBytes_;
    }
    ExpressionLimits modelLimits() const noexcept {
        return modelLimits_;
    }
    ExpressionLimits componentLimits(std::size_t site) const {
        return componentLimits_.at(site);
    }

    Expression materialize(const std::vector<Expression>& components) const {
        if (components.size() != roots_.size())
            throw std::invalid_argument("template component count differs");
        for (const auto& component : components)
            if (component.variableCount() != variableCount())
                throw std::invalid_argument("template component schema differs");
        for (std::size_t site = 0; site < roots_.size(); ++site) {
            const auto nodes = components[site].nodeCount();
            if (nodes > componentLimits_[site].maxNodes ||
                components[site].depth() > componentLimits_[site].maxDepth ||
                components[site].arithmeticCost() > componentLimits_[site].maxArithmeticOperations)
                throw std::length_error("component exceeds its expression limits");
        }
        std::vector<std::size_t> order(roots_.size());
        for (std::size_t site = 0; site < order.size(); ++site)
            order[site] = site;
        std::sort(order.begin(), order.end(), [&](std::size_t left, std::size_t right) {
            return ranges_[left].first < ranges_[right].first;
        });
        const auto size = checkedMaterializedSize(components);
        std::vector<ExpressionNode> nodes;
        nodes.reserve(size);
        std::size_t cursor = 0;
        const auto sourceNodes = source_.nodes();
        for (const auto site : order) {
            nodes.insert(nodes.end(), sourceNodes.begin() + cursor,
                         sourceNodes.begin() + ranges_[site].first);
            append(nodes, components[site]);
            cursor = ranges_[site].second;
        }
        nodes.insert(nodes.end(), sourceNodes.begin() + cursor, sourceNodes.end());
        return Expression(nodes, variableCount(), modelLimits_);
    }
    template <std::size_t N>
    MultiScalarExpressionTemplate(const Expression& source, const std::array<std::size_t, N>& roots,
                                  std::array<ExpressionLimits, N> componentLimits,
                                  ExpressionLimits modelLimits,
                                  std::size_t maximumMaterializationBytes = 1'048'576)
        : MultiScalarExpressionTemplate(
              source, std::vector<std::size_t>(roots.begin(), roots.end()),
              std::vector<ExpressionLimits>(componentLimits.begin(), componentLimits.end()),
              modelLimits, maximumMaterializationBytes) {}

    MultiComponentRanges componentRangesVector(const Expression& model) const {
        if (model.variableCount() != variableCount() || model.nodeCount() > modelLimits_.maxNodes)
            throw std::invalid_argument("model does not match multi-component template schema");
        if (model.nodeCount() > std::numeric_limits<std::size_t>::max() - source_.nodeCount() ||
            source_.nodeCount() + model.nodeCount() > maximumMaterializationBytes_ / slotBytes)
            throw std::length_error("template matching workspace limit");
        const Expression checkedModel(model.nodes(), variableCount(), modelLimits_);
        const auto sourceTree = tree(source_);
        const auto modelTree = tree(checkedModel);
        std::vector<std::optional<std::pair<std::size_t, std::size_t>>> captures(roots_.size());
        std::vector<std::pair<std::size_t, std::size_t>> pending{
            {source_.nodeCount() - 1, model.nodeCount() - 1}};
        while (!pending.empty()) {
            const auto [sourceIndex, modelIndex] = pending.back();
            pending.pop_back();
            const auto site = selectedSite(sourceIndex);
            if (site < roots_.size()) {
                captures[site] = modelTree.ranges[modelIndex];
                continue;
            }
            const auto sourceNode = source_.nodes()[sourceIndex];
            const auto modelNode = checkedModel.nodes()[modelIndex];
            if (!sameNode(sourceNode, modelNode))
                throw std::invalid_argument("frozen multi-component scaffold changed");
            const auto arity = expressionArity(sourceNode.kind());
            for (std::size_t child = 0; child < arity; ++child)
                pending.emplace_back(sourceTree.children[sourceIndex][child],
                                     modelTree.children[modelIndex][child]);
        }
        MultiComponentRanges result;
        for (const auto& capture : captures) {
            if (!capture)
                throw std::logic_error("template matcher did not reach every site");
            result.push_back(*capture);
        }
        return result;
    }

    MultiComponentExpressions componentsVector(const Expression& model) const {
        const auto ranges = componentRangesVector(model);
        MultiComponentExpressions result;
        for (std::size_t site = 0; site < ranges.size(); ++site)
            result.push_back(slice(model, ranges[site], componentLimits_[site]));
        return result;
    }
    std::array<Expression, 2> components(const Expression& model) const {
        if (roots_.size() != 2)
            throw std::invalid_argument("two-site extraction requires exactly two components");
        const auto result = componentsVector(model);
        return {result[0], result[1]};
    }
    std::array<std::pair<std::size_t, std::size_t>, 2>
    componentRanges(const Expression& model) const {
        if (roots_.size() != 2)
            throw std::invalid_argument("two-site ranges require exactly two components");
        const auto ranges = componentRangesVector(model);
        return {ranges[0], ranges[1]};
    }
    std::size_t siteCount() const noexcept {
        return roots_.size();
    }
    template <std::size_t N>
    Expression materialize(const std::array<Expression, N>& components) const {
        return materialize(std::vector<Expression>(components.begin(), components.end()));
    }

private:
    struct TreeIndex {
        std::vector<std::array<std::size_t, 3>> children;
        std::vector<std::pair<std::size_t, std::size_t>> ranges;
    };
    static constexpr std::size_t slotBytes = sizeof(ExpressionNode) + sizeof(std::size_t) * 10;

    static Expression checkedSource(const Expression& source, ExpressionLimits modelLimits,
                                    std::size_t maximumBytes) {
        if (maximumBytes == 0 || source.nodeCount() > maximumBytes / slotBytes)
            throw std::length_error("template source exceeds materialization allowance");
        return Expression(source.nodes(), source.variableCount(), modelLimits);
    }

    static void append(std::vector<ExpressionNode>& output, const Expression& expression) {
        const auto nodes = expression.nodes();
        output.insert(output.end(), nodes.begin(), nodes.end());
    }

    static Expression slice(const Expression& expression, std::pair<std::size_t, std::size_t> range,
                            ExpressionLimits limits) {
        const auto nodes = expression.nodes();
        return Expression(std::span(nodes.data() + range.first, range.second - range.first),
                          expression.variableCount(), limits);
    }

    std::size_t checkedMaterializedSize(const std::vector<Expression>& components) const {
        if (components.size() != roots_.size())
            throw std::invalid_argument("template component count differs");
        std::size_t size = frozenNodes_;
        for (const auto& component : components) {
            if (component.nodeCount() > modelLimits_.maxNodes - size)
                throw std::length_error("complete model node limit");
            size += component.nodeCount();
        }
        if (size > maximumMaterializationBytes_ / slotBytes)
            throw std::length_error("template materialization allowance exceeded");
        return size;
    }

    std::size_t arithmeticCostOutsideSites() const {
        std::size_t cost = 0;
        for (std::size_t index = 0; index < source_.nodeCount(); ++index) {
            if (indexInSite(index))
                continue;
            const auto node = source_.nodes()[index];
            if (expressionArity(node.kind()) == 0)
                continue;
            const auto operation = expressionOperationCost(
                node.kind(), expressionPowerMetadata(node.kind()) ? node.exponent() : 0);
            if (operation > modelLimits_.maxArithmeticOperations - cost)
                throw std::length_error("frozen scaffold work exceeds model limit");
            cost += operation;
        }
        return cost;
    }

    bool indexInSite(std::size_t index) const noexcept {
        for (const auto& range : ranges_)
            if (index >= range.first && index < range.second)
                return true;
        return false;
    }

    std::size_t selectedSite(std::size_t sourceIndex) const noexcept {
        for (std::size_t site = 0; site < roots_.size(); ++site)
            if (sourceIndex == roots_[site])
                return site;
        return roots_.size();
    }

    static bool sameNode(const ExpressionNode& left, const ExpressionNode& right) {
        if (left.kind() != right.kind())
            return false;
        if (left.kind() == ExpressionKind::Variable)
            return left.variableIndex() == right.variableIndex();
        if (left.kind() == ExpressionKind::Constant)
            return std::bit_cast<std::uint64_t>(left.constantValue()) ==
                   std::bit_cast<std::uint64_t>(right.constantValue());
        return !expressionPowerMetadata(left.kind()) || left.exponent() == right.exponent();
    }

    static TreeIndex tree(const Expression& expression) {
        TreeIndex result;
        const auto nodes = expression.nodes();
        result.children.resize(nodes.size());
        result.ranges.resize(nodes.size());
        std::vector<std::size_t> roots;
        for (std::size_t index = 0; index < nodes.size(); ++index) {
            const auto arity = expressionArity(nodes[index].kind());
            auto first = index;
            for (std::size_t child = arity; child > 0; --child) {
                const auto childRoot = roots.back();
                roots.pop_back();
                result.children[index][child - 1] = childRoot;
                first = result.ranges[childRoot].first;
            }
            result.ranges[index] = {first, index + 1};
            roots.push_back(index);
        }
        return result;
    }

    std::size_t maximumMaterializationBytes_;
    Expression source_;
    std::vector<std::size_t> roots_;
    std::vector<std::pair<std::size_t, std::size_t>> ranges_;
    std::vector<ExpressionLimits> componentLimits_;
    ExpressionLimits modelLimits_;
    std::size_t frozenNodes_ = 0;
    std::size_t frozenWork_ = 0;
};

class MultiComponentTemplateProblem final {
public:
    MultiComponentTemplateProblem(const MultiScalarExpressionTemplate& scaffold,
                                  std::shared_ptr<const RegressionProblem> problem,
                                  std::vector<std::vector<std::size_t>> allowedVariables,
                                  std::vector<std::vector<ExpressionKind>> allowedOperators)
        : scaffold_(scaffold), problem_(std::move(problem)),
          allowedVariables_(std::move(allowedVariables)),
          allowedOperators_(std::move(allowedOperators)) {
        if (!problem_ || problem_->dimensions() != scaffold_.variableCount())
            throw std::invalid_argument("template and regression problem schemas differ");
        const auto problemLimits = problem_->limits();
        const auto modelLimits = scaffold_.modelLimits();
        if (problemLimits.maxNodes != modelLimits.maxNodes ||
            problemLimits.maxDepth != modelLimits.maxDepth ||
            problemLimits.maxAbsConstant != modelLimits.maxAbsConstant ||
            problemLimits.maxArithmeticOperations != modelLimits.maxArithmeticOperations)
            throw std::invalid_argument("template and complete-model limits differ");
        if (allowedVariables_.size() != scaffold_.siteCount() ||
            allowedOperators_.size() != scaffold_.siteCount())
            throw std::invalid_argument("template restrictions must match site count");
        for (std::size_t site = 0; site < scaffold_.siteCount(); ++site) {
            validateUnique(allowedVariables_[site], problem_->dimensions(), "template variables");
            validateOperators(allowedOperators_[site]);
        }
    }

    MultiComponentTemplateProblem(const MultiScalarExpressionTemplate& scaffold,
                                  std::shared_ptr<const RegressionProblem> problem,
                                  std::array<std::vector<std::size_t>, 2> allowedVariables,
                                  std::array<std::vector<ExpressionKind>, 2> allowedOperators)
        : MultiComponentTemplateProblem(
              scaffold, std::move(problem),
              std::vector<std::vector<std::size_t>>{allowedVariables[0], allowedVariables[1]},
              std::vector<std::vector<ExpressionKind>>{allowedOperators[0], allowedOperators[1]}) {}

    static std::shared_ptr<const MultiComponentTemplateProblem>
    createMany(const MultiScalarExpressionTemplate& scaffold,
               std::shared_ptr<const RegressionProblem> problem,
               std::vector<std::vector<std::size_t>> allowedVariables,
               std::vector<std::vector<ExpressionKind>> allowedOperators) {
        return std::make_shared<const MultiComponentTemplateProblem>(
            scaffold, std::move(problem), std::move(allowedVariables), std::move(allowedOperators));
    }
    static std::shared_ptr<const MultiComponentTemplateProblem>
    create(const MultiScalarExpressionTemplate& scaffold,
           std::shared_ptr<const RegressionProblem> problem,
           std::array<std::vector<std::size_t>, 2> allowedVariables,
           std::array<std::vector<ExpressionKind>, 2> allowedOperators) {
        return std::make_shared<const MultiComponentTemplateProblem>(
            scaffold, std::move(problem), std::move(allowedVariables), std::move(allowedOperators));
    }
    const MultiScalarExpressionTemplate& expressionTemplate() const noexcept {
        return scaffold_;
    }
    std::shared_ptr<const RegressionProblem> problem() const noexcept {
        return problem_;
    }
    const std::vector<std::size_t>& allowedVariables(std::size_t site) const {
        return allowedVariables_.at(site);
    }
    const std::vector<ExpressionKind>& allowedOperators(std::size_t site) const {
        return allowedOperators_.at(site);
    }

    void validateComponent(std::size_t site, const Expression& component) const {
        if (site >= scaffold_.siteCount() || component.variableCount() != problem_->dimensions())
            throw std::invalid_argument("template component schema differs");
        const auto limits = scaffold_.componentLimits(site);
        if (component.nodeCount() > limits.maxNodes || component.depth() > limits.maxDepth ||
            component.arithmeticCost() > limits.maxArithmeticOperations)
            throw std::length_error("component exceeds its individual limits");
        for (const auto& node : component.nodes()) {
            if (node.kind() == ExpressionKind::Constant &&
                std::abs(node.constantValue()) > limits.maxAbsConstant)
                throw std::length_error("component constant limit exceeded");
            if (node.kind() == ExpressionKind::Variable &&
                std::find(allowedVariables_[site].begin(), allowedVariables_[site].end(),
                          node.variableIndex()) == allowedVariables_[site].end())
                throw std::invalid_argument("component uses a forbidden variable");
            if (expressionArity(node.kind()) != 0 &&
                std::find(allowedOperators_[site].begin(), allowedOperators_[site].end(),
                          node.kind()) == allowedOperators_[site].end())
                throw std::invalid_argument("component uses a forbidden operator");
        }
    }

    Expression materialize(const std::vector<Expression>& components) const {
        if (components.size() != scaffold_.siteCount())
            throw std::invalid_argument("template component count differs");
        for (std::size_t site = 0; site < components.size(); ++site)
            validateComponent(site, components[site]);
        return scaffold_.materialize(components);
    }
    template <std::size_t N>
    Expression materialize(const std::array<Expression, N>& components) const {
        return materialize(std::vector<Expression>(components.begin(), components.end()));
    }
    std::vector<Expression> componentsVector(const Expression& model) const {
        auto result = scaffold_.componentsVector(model);
        for (std::size_t site = 0; site < result.size(); ++site)
            validateComponent(site, result[site]);
        return result;
    }
    std::array<Expression, 2> components(const Expression& model) const {
        if (scaffold_.siteCount() != 2)
            throw std::invalid_argument("two-site extraction requires exactly two components");
        const auto result = componentsVector(model);
        return {result[0], result[1]};
    }

private:
    static void validateUnique(const std::vector<std::size_t>& values, std::size_t upperBound,
                               const char* description) {
        for (std::size_t index = 0; index < values.size(); ++index)
            if (values[index] >= upperBound || std::find(values.begin(), values.begin() + index,
                                                         values[index]) != values.begin() + index)
                throw std::invalid_argument(description);
    }

    static void validateOperators(const std::vector<ExpressionKind>& operators) {
        for (std::size_t index = 0; index < operators.size(); ++index)
            if (expressionArity(operators[index]) == 0 ||
                std::find(operators.begin(), operators.begin() + index, operators[index]) !=
                    operators.begin() + index)
                throw std::invalid_argument("template operators must be distinct operations");
    }

    const MultiScalarExpressionTemplate scaffold_;
    const std::shared_ptr<const RegressionProblem> problem_;
    const std::vector<std::vector<std::size_t>> allowedVariables_;
    const std::vector<std::vector<ExpressionKind>> allowedOperators_;
};

// A named scalar function with one shared coefficient. The first supported form
// is a * h(t), where a is one selected constant and h does not contain a. Calls
// are expanded to an ordinary expression for validation/prediction, while fit()
// estimates the one coefficient jointly across every call and every row.
class SharedLearnableFunctionTemplate final {
public:
    struct FitResult {
        double coefficient = 0.0;
        double mse = 0.0;
        Expression expanded;
        std::size_t parameterCount = 1;
    };

    SharedLearnableFunctionTemplate(std::string name, Expression body, std::size_t argumentVariable,
                                    std::size_t coefficientNode, std::vector<Expression> callInputs,
                                    ExpressionLimits expandedLimits)
        : name_(std::move(name)), body_(std::move(body)), argumentVariable_(argumentVariable),
          coefficientNode_(coefficientNode), callInputs_(std::move(callInputs)),
          expandedLimits_(expandedLimits) {
        validate();
    }

    const std::string& name() const noexcept {
        return name_;
    }
    std::size_t parameterCount() const noexcept {
        return 1;
    }
    const Expression& body() const noexcept {
        return body_;
    }
    Expression expanded(double coefficient) const {
        if (!std::isfinite(coefficient))
            throw std::invalid_argument("shared coefficient must be finite");
        const auto fittedBody = body_.withConstant(coefficientNode_, coefficient);
        std::optional<Expression> sum;
        for (const auto& input : callInputs_) {
            auto call = expandCall(fittedBody, input);
            sum = sum ? Expression::binary(ExpressionKind::Add, *sum, call, expandedLimits_)
                      : std::optional<Expression>(std::move(call));
        }
        return Expression(sum->nodes(), body_.variableCount(), expandedLimits_);
    }

    // This analytic least-squares fitter is exact for the declared a*h(t) body
    // form. It never fits separate coefficients for separate call sites.
    FitResult fit(const RegressionProblem& problem, double lower, double upper) const {
        if (problem.dimensions() != body_.variableCount() || !std::isfinite(lower) ||
            !std::isfinite(upper) || lower > upper)
            throw std::invalid_argument("shared function fit schema or bounds differ");
        const auto limits = problem.limits();
        if (limits.maxNodes != expandedLimits_.maxNodes ||
            limits.maxDepth != expandedLimits_.maxDepth ||
            limits.maxAbsConstant != expandedLimits_.maxAbsConstant ||
            limits.maxArithmeticOperations != expandedLimits_.maxArithmeticOperations)
            throw std::invalid_argument("shared function and regression model limits differ");
        if (lower < -expandedLimits_.maxAbsConstant || upper > expandedLimits_.maxAbsConstant)
            throw std::invalid_argument("shared function fit interval exceeds coefficient limit");
        if (problem.objective().loss != RegressionLoss::MeanSquaredError)
            throw std::invalid_argument(
                "shared function coefficient fitting supports ordinary MSE only");
        // Keep intermediate magnitudes as a binary64 mantissa and an integer
        // exponent. This avoids relying on long double having a wider range,
        // and it lets the factor be evaluated without installing a synthetic
        // unit coefficient that could violate maxAbsConstant.
        struct Scaled {
            double mantissa = 0.0;
            int exponent = 0;
            static Scaled from(double value) {
                if (value == 0.0)
                    return {};
                int exponent = 0;
                return {std::frexp(value, &exponent), exponent};
            }
            void add(Scaled other) {
                if (other.mantissa == 0.0)
                    return;
                if (mantissa == 0.0) {
                    *this = other;
                    return;
                }
                if (other.exponent > exponent) {
                    mantissa = std::ldexp(mantissa, exponent - other.exponent) + other.mantissa;
                    exponent = other.exponent;
                } else {
                    mantissa += std::ldexp(other.mantissa, other.exponent - exponent);
                }
                if (mantissa == 0.0) {
                    exponent = 0;
                    return;
                }
                int adjustment = 0;
                mantissa = std::frexp(mantissa, &adjustment);
                exponent += adjustment;
            }
            static Scaled product(Scaled a, Scaled b) {
                if (a.mantissa == 0.0 || b.mantissa == 0.0)
                    return {};
                int adjustment = 0;
                const double m = std::frexp(a.mantissa * b.mantissa, &adjustment);
                return {m, a.exponent + b.exponent + adjustment};
            }
        };
        const auto rightRange = expressionSubtree(body_, body_.nodeCount() - 2);
        const auto leftRange = expressionSubtree(body_, rightRange.first - 1);
        const auto selected = std::pair{coefficientNode_, coefficientNode_ + 1};
        const auto factorRange = leftRange == selected ? rightRange : leftRange;
        const Expression factor(std::span(body_.nodes().data() + factorRange.first,
                                          factorRange.second - factorRange.first),
                                body_.variableCount(), expandedLimits_);
        Scaled numerator, denominator;
        for (const auto& row : problem.trainingRows()) {
            Scaled feature;
            for (const auto& input : callInputs_) {
                const auto call = expandCall(factor, input).evaluate(row.inputs);
                if (!call.valid())
                    throw std::domain_error("shared function fit encountered invalid call");
                feature.add(Scaled::from(call.value()));
            }
            if (!std::isfinite(feature.mantissa) || !std::isfinite(row.target))
                throw std::domain_error("shared function design values are non-finite");
            const auto target = Scaled::from(row.target);
            numerator.add(Scaled::product(feature, target));
            denominator.add(Scaled::product(feature, feature));
        }
        if (denominator.mantissa == 0.0)
            throw std::domain_error("shared function coefficient is not identifiable");
        double unconstrained = 0.0;
        if (numerator.mantissa != 0.0) {
            int adjustment = 0;
            const double ratio = std::frexp(numerator.mantissa / denominator.mantissa, &adjustment);
            const int exponent = numerator.exponent - denominator.exponent + adjustment;
            if (exponent > std::numeric_limits<double>::max_exponent)
                unconstrained = std::copysign(std::numeric_limits<double>::infinity(), ratio);
            else if (exponent < std::numeric_limits<double>::min_exponent -
                                    std::numeric_limits<double>::digits)
                unconstrained = std::copysign(0.0, ratio);
            else
                unconstrained = std::ldexp(ratio, exponent);
        }
        const double coefficient = std::clamp(unconstrained, lower, upper);
        auto model = expanded(coefficient);
        const auto loss = problem.measure(model).loss.score();
        return {coefficient, loss, std::move(model), 1};
    }

    SharedLearnableFunctionTemplate varyBody(Expression changedBody,
                                             std::size_t changedCoefficientNode) const {
        return SharedLearnableFunctionTemplate(name_, std::move(changedBody), argumentVariable_,
                                               changedCoefficientNode, callInputs_,
                                               expandedLimits_);
    }
    static SharedLearnableFunctionTemplate
    generateBody(std::string name, std::size_t argumentVariable, std::vector<Expression> callInputs,
                 ExpressionLimits limits, const ExpressionGrammar& grammar, std::mt19937& random) {
        if (callInputs.empty())
            throw std::invalid_argument("shared function requires call inputs");
        const auto dimensions = callInputs.front().variableCount();
        std::vector<std::size_t> allowed{argumentVariable};
        genetik_detail::validateExpressionGrammar(1, limits, grammar);
        if (argumentVariable >= dimensions)
            throw std::invalid_argument("shared function argument binding outside input schema");
        auto factor = genetik_detail::generateExpression(random, grammar.initialMaxDepth,
                                                         dimensions, limits, grammar, &allowed);
        const auto coefficient = Expression::constant(1.0, dimensions, limits);
        auto body = Expression::binary(ExpressionKind::Multiply, factor, coefficient, limits);
        return SharedLearnableFunctionTemplate(std::move(name), std::move(body), argumentVariable,
                                               factor.nodeCount(), std::move(callInputs), limits);
    }

    SharedLearnableFunctionTemplate mutateBody(const ExpressionGrammar& grammar,
                                               std::mt19937& random) const {
        const auto coefficientRange = expressionSubtree(body_, coefficientNode_);
        const auto rightRange = expressionSubtree(body_, body_.nodeCount() - 2);
        const auto leftRange = expressionSubtree(body_, rightRange.first - 1);
        const auto factorRange = leftRange == coefficientRange ? rightRange : leftRange;
        const auto factor = Expression(std::span(body_.nodes().data() + factorRange.first,
                                                 factorRange.second - factorRange.first),
                                       body_.variableCount(), expandedLimits_);
        genetik_detail::validateExpressionGrammar(1, expandedLimits_, grammar);
        std::vector<std::size_t> allowed{argumentVariable_};
        const auto root =
            std::uniform_int_distribution<std::size_t>(0, factor.nodeCount() - 1)(random);
        auto replacement = genetik_detail::generateExpression(random, grammar.mutationMaxDepth,
                                                              body_.variableCount(),
                                                              expandedLimits_, grammar, &allowed);
        const auto changed = replaceExpressionSubtree(factor, root, replacement, expandedLimits_);
        if (!changed || !genetik_detail::extendedGrammarAccepts(*changed, grammar))
            throw std::length_error("bounded shared body mutation was rejected");
        const auto coefficient =
            Expression::constant(body_.nodes()[coefficientNode_].constantValue(),
                                 body_.variableCount(), expandedLimits_);
        auto body =
            Expression::binary(ExpressionKind::Multiply, *changed, coefficient, expandedLimits_);
        return SharedLearnableFunctionTemplate(name_, std::move(body), argumentVariable_,
                                               changed->nodeCount(), callInputs_, expandedLimits_);
    }

private:
    void validate() const {
        if (name_.empty() || callInputs_.size() < 2 || argumentVariable_ >= body_.variableCount() ||
            coefficientNode_ >= body_.nodeCount() ||
            body_.nodes()[coefficientNode_].kind() != ExpressionKind::Constant)
            throw std::invalid_argument("invalid shared function name, argument or coefficient");
        for (const auto& input : callInputs_)
            if (input.variableCount() != body_.variableCount())
                throw std::invalid_argument("shared function call input schema differs");
        const auto nodes = body_.nodes();
        if (nodes.back().kind() != ExpressionKind::Multiply)
            throw std::invalid_argument("shared function body must have form a*h(t)");
        const auto right = expressionSubtree(body_, body_.nodeCount() - 2);
        const auto left = expressionSubtree(body_, right.first - 1);
        const auto selected = std::pair{coefficientNode_, coefficientNode_ + 1};
        if (left != selected && right != selected)
            throw std::invalid_argument("shared coefficient must be a direct factor of the body");
        const auto nonCoefficient = left == selected ? right : left;
        for (std::size_t i = nonCoefficient.first; i < nonCoefficient.second; ++i)
            if (nodes[i].kind() == ExpressionKind::Constant && i == coefficientNode_)
                throw std::invalid_argument("shared coefficient occurs more than once");
        for (const auto& node : nodes)
            if (node.kind() == ExpressionKind::Variable &&
                node.variableIndex() != argumentVariable_)
                throw std::invalid_argument("shared function body has an unbound variable");
        (void)expanded(0.0);
    }

    Expression expandCall(const Expression& body, const Expression& input) const {
        std::vector<Expression> stack;
        const auto nodes = body.nodes();
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            const auto node = nodes[i];
            if (node.kind() == ExpressionKind::Constant) {
                stack.push_back(Expression::constant(node.constantValue(), body.variableCount(),
                                                     expandedLimits_));
            } else if (node.kind() == ExpressionKind::Variable) {
                stack.push_back(input);
            } else {
                if (isCustomScalarOperator(node.kind()))
                    throw std::invalid_argument("shared function does not expand custom operators");
                const auto arity = expressionArity(node.kind());
                if (arity == 1) {
                    auto child = std::move(stack.back());
                    stack.pop_back();
                    if (node.kind() == ExpressionKind::Powi)
                        stack.push_back(Expression::power(child, node.exponent(), expandedLimits_));
                    else if (node.kind() == ExpressionKind::NegativePowi)
                        stack.push_back(Expression::integerPower(
                            child, -static_cast<int>(node.exponent()), expandedLimits_));
                    else
                        stack.push_back(Expression::unary(node.kind(), child, expandedLimits_));
                } else if (arity == 2) {
                    auto right = std::move(stack.back());
                    stack.pop_back();
                    auto left = std::move(stack.back());
                    stack.pop_back();
                    stack.push_back(Expression::binary(node.kind(), left, right, expandedLimits_));
                } else {
                    throw std::invalid_argument(
                        "shared function supports unary and binary body operators");
                }
            }
        }
        if (stack.size() != 1)
            throw std::logic_error("shared function body expansion failed");
        return std::move(stack.back());
    }

    std::string name_;
    Expression body_;
    std::size_t argumentVariable_;
    std::size_t coefficientNode_;
    std::vector<Expression> callInputs_;
    ExpressionLimits expandedLimits_;
};

class MultiComponentTemplateContext final : public RegressionCandidateContext {
public:
    MultiComponentTemplateContext(std::shared_ptr<const MultiComponentTemplateProblem> domain,
                                  std::shared_ptr<RegressionPreparation> preparation = {})
        : domain_(std::move(domain)), preparation_(std::move(preparation)) {
        if (!domain_)
            throw std::invalid_argument("multi-component template domain required");
    }
    std::shared_ptr<const RegressionProblem> problem() const override {
        return domain_->problem();
    }
    std::shared_ptr<RegressionPreparation> preparation() const override {
        return preparation_;
    }
    void validate(const Expression& expression) const override {
        (void)domain_->componentsVector(expression);
    }
    bool parallelEvaluationSafe() const noexcept override {
        return true;
    }
    std::shared_ptr<const MultiComponentTemplateProblem> domain() const noexcept {
        return domain_;
    }

private:
    const std::shared_ptr<const MultiComponentTemplateProblem> domain_;
    const std::shared_ptr<RegressionPreparation> preparation_;
};

class MultiComponentTemplateParameters final : public ConstantParameterSelection {
public:
    explicit MultiComponentTemplateParameters(
        std::shared_ptr<const MultiComponentTemplateProblem> domain)
        : domain_(std::move(domain)) {
        if (!domain_)
            throw std::invalid_argument("multi-component template domain required");
    }
    bool parallelEvaluationSafe() const noexcept override {
        return true;
    }
    std::vector<std::size_t> parameters(const Expression& model) const override {
        const auto components = domain_->componentsVector(model);
        std::vector<std::size_t> result;
        // Constant fitting operates on full-model postfix indices. Locate the two
        // validated component representations in the frozen materialization.
        const auto selected = domain_->expressionTemplate().componentRangesVector(model);
        for (std::size_t site = 0; site < domain_->expressionTemplate().siteCount(); ++site) {
            const auto nodes = components[site].nodes();
            for (std::size_t index = 0; index < nodes.size(); ++index)
                if (nodes[index].kind() == ExpressionKind::Constant)
                    result.push_back(selected[site].first + index);
        }
        std::sort(result.begin(), result.end());
        return result;
    }

private:
    std::shared_ptr<const MultiComponentTemplateProblem> domain_;
};

class MultiComponentTemplateVariation final {
public:
    MultiComponentTemplateVariation(std::shared_ptr<const MultiComponentTemplateContext> context,
                                    std::vector<ExpressionGrammar> grammars)
        : context_(std::move(context)), grammars_(std::move(grammars)) {
        if (!context_)
            throw std::invalid_argument("multi-component template context required");
        if (grammars_.size() != context_->domain()->expressionTemplate().siteCount())
            throw std::invalid_argument("template grammar count differs from site count");
        for (std::size_t site = 0; site < context_->domain()->expressionTemplate().siteCount();
             ++site) {
            const auto& domain = *context_->domain();
            for (const auto operation : grammars_[site].operators)
                if (std::find(domain.allowedOperators(site).begin(),
                              domain.allowedOperators(site).end(),
                              operation) == domain.allowedOperators(site).end())
                    throw std::invalid_argument("component grammar exceeds operator restrictions");
            genetik_detail::validateExpressionGrammar(
                domain.allowedVariables(site).size(),
                domain.expressionTemplate().componentLimits(site), grammars_[site]);
        }
    }
    MultiComponentTemplateVariation(std::shared_ptr<const MultiComponentTemplateContext> context,
                                    const std::array<ExpressionGrammar, 2>& grammars)
        : MultiComponentTemplateVariation(
              std::move(context), std::vector<ExpressionGrammar>{grammars[0], grammars[1]}) {}

    VariationStatistics statistics() const noexcept {
        return statistics_;
    }
    void markElite(ExpressionCandidate& candidate) const {
        candidate.setPreparationOrigin(PreparationOrigin::Elite);
    }
    void markOffspring(ExpressionCandidate& candidate, const ExpressionCandidate& parent) const {
        candidate.setPreparationOrigin(
            candidate.expression().sameRepresentation(parent.expression())
                ? PreparationOrigin::UnchangedOffspring
                : PreparationOrigin::ChangedOffspring);
    }

    ExpressionCandidate initialize(std::mt19937& random) {
        count(statistics_.initializations);
        for (std::size_t attempt = 0; attempt < proposalLimit(); ++attempt) {
            std::vector<Expression> components;
            for (std::size_t site = 0; site < grammars_.size(); ++site)
                components.push_back(randomComponent(site, random, true));
            try {
                return candidate(context_->domain()->materialize(components));
            } catch (const std::length_error&) {
                count(statistics_.structuralRejections);
            }
        }
        throw std::length_error("bounded initialization could not fit both template sites");
    }

    ExpressionCandidate mutation(const ExpressionCandidate& parent, float rate,
                                 std::mt19937& random) {
        require(parent);
        auto child = parent;
        if (std::uniform_real_distribution<float>(0.0f, 1.0f)(random) >= rate)
            return child;
        const auto site = index(grammars_.size(), random);
        const auto components = context_->domain()->componentsVector(parent.expression());
        const auto& grammar = grammars_[site];
        for (std::size_t attempt = 0; attempt < grammar.proposalLimit; ++attempt) {
            count(statistics_.mutationProposals);
            const auto root = index(components[site].nodeCount(), random);
            const auto replacement = randomComponent(site, random, false);
            auto changed = replaceExpressionSubtree(
                components[site], root, replacement,
                context_->domain()->expressionTemplate().componentLimits(site));
            if (!changed || !genetik_detail::extendedGrammarAccepts(*changed, grammar)) {
                count(statistics_.structuralRejections);
                continue;
            }
            auto next = components;
            next[site] = std::move(*changed);
            try {
                child.replaceExpression(context_->domain()->materialize(next));
            } catch (const std::length_error&) {
                count(statistics_.structuralRejections);
                continue;
            }
            count(statistics_.mutationChanges, child.text() != parent.text());
            return child;
        }
        count(statistics_.mutationFallbacks);
        return child;
    }

    void crossover(const ExpressionCandidate& father, const ExpressionCandidate& mother,
                   ExpressionCandidate& firstChild, ExpressionCandidate& secondChild, float rate,
                   std::mt19937& random) {
        require(father);
        require(mother);
        auto first = father;
        auto second = mother;
        if (std::uniform_real_distribution<float>(0.0f, 1.0f)(random) < rate) {
            const auto site = index(grammars_.size(), random);
            const auto firstComponents = context_->domain()->componentsVector(father.expression());
            const auto secondComponents = context_->domain()->componentsVector(mother.expression());
            bool accepted = false;
            for (std::size_t attempt = 0; attempt < grammars_[site].proposalLimit; ++attempt) {
                count(statistics_.crossoverProposals);
                const auto firstRoot = index(firstComponents[site].nodeCount(), random);
                const auto secondRoot = index(secondComponents[site].nodeCount(), random);
                const auto firstSubtree = subtree(secondComponents[site], secondRoot);
                const auto secondSubtree = subtree(firstComponents[site], firstRoot);
                const auto limits = context_->domain()->expressionTemplate().componentLimits(site);
                auto changedFirst = replaceExpressionSubtree(firstComponents[site], firstRoot,
                                                             firstSubtree, limits);
                auto changedSecond = replaceExpressionSubtree(secondComponents[site], secondRoot,
                                                              secondSubtree, limits);
                if (!changedFirst || !changedSecond ||
                    !genetik_detail::extendedGrammarAccepts(*changedFirst, grammars_[site]) ||
                    !genetik_detail::extendedGrammarAccepts(*changedSecond, grammars_[site])) {
                    count(statistics_.structuralRejections);
                    continue;
                }
                auto firstNext = firstComponents;
                auto secondNext = secondComponents;
                firstNext[site] = std::move(*changedFirst);
                secondNext[site] = std::move(*changedSecond);
                try {
                    first.replaceExpression(context_->domain()->materialize(firstNext));
                    second.replaceExpression(context_->domain()->materialize(secondNext));
                } catch (const std::length_error&) {
                    first = father;
                    second = mother;
                    count(statistics_.structuralRejections);
                    continue;
                }
                count(statistics_.crossoverChanges,
                      static_cast<std::uint64_t>(first.text() != father.text()) +
                          static_cast<std::uint64_t>(second.text() != mother.text()));
                accepted = true;
                break;
            }
            if (!accepted)
                count(statistics_.crossoverFallbacks);
        }
        firstChild = std::move(first);
        secondChild = std::move(second);
    }

private:
    static void count(std::uint64_t& counter, std::uint64_t increment = 1) {
        if (increment > std::numeric_limits<std::uint64_t>::max() - counter)
            throw std::overflow_error("multi-component variation counter exhausted");
        counter += increment;
    }
    static std::size_t index(std::size_t count, std::mt19937& random) {
        return std::uniform_int_distribution<std::size_t>(0, count - 1)(random);
    }
    static Expression subtree(const Expression& expression, std::size_t root) {
        const auto range = expressionSubtree(expression, root);
        const auto nodes = expression.nodes();
        return Expression(std::span(nodes.data() + range.first, range.second - range.first),
                          expression.variableCount(), expression.limits());
    }
    std::size_t proposalLimit() const noexcept {
        std::size_t result = std::numeric_limits<std::size_t>::max();
        for (const auto& grammar : grammars_)
            result = std::min(result, grammar.proposalLimit);
        return result;
    }
    Expression randomComponent(std::size_t site, std::mt19937& random, bool initialize) const {
        const auto& grammar = grammars_[site];
        return genetik_detail::generateExpression(
            random, initialize ? grammar.initialMaxDepth : grammar.mutationMaxDepth,
            context_->problem()->dimensions(),
            context_->domain()->expressionTemplate().componentLimits(site), grammar,
            &context_->domain()->allowedVariables(site));
    }
    ExpressionCandidate candidate(const Expression& model) const {
        return ExpressionCandidate::constrained(model, context_);
    }
    void require(const ExpressionCandidate& candidate) const {
        if (candidate.problemIdentity() != context_)
            throw std::invalid_argument("candidate belongs to a different template run");
        context_->validate(candidate.expression());
    }

    std::shared_ptr<const MultiComponentTemplateContext> context_;
    const std::vector<ExpressionGrammar> grammars_;
    VariationStatistics statistics_;
};

inline SymbolicRegressionResult
multiComponentTemplateRegression(std::shared_ptr<const MultiComponentTemplateProblem> domain,
                                 const std::vector<ExpressionGrammar>& grammars,
                                 const SymbolicRegressionConfig& config, RandomContext& random,
                                 std::shared_ptr<TrainingObjective> objective = {}) {
    if (!domain)
        throw std::invalid_argument("multi-component template domain required");
    if (random.evaluationAttempts() != 0 ||
        config.selectionStrategy != SymbolicRegressionConfig::SelectionStrategy::PredictionLoss ||
        config.fitEveryGenerations == 0 || config.population == 0 ||
        config.population > std::numeric_limits<unsigned int>::max() ||
        config.elites >= config.population || !std::isfinite(config.crossoverRate) ||
        config.crossoverRate < 0 || config.crossoverRate > 1 ||
        !std::isfinite(config.mutationRate) || config.mutationRate < 0 || config.mutationRate > 1 ||
        !std::isfinite(config.trainingLossThreshold) || config.trainingLossThreshold < 0)
        throw std::invalid_argument("invalid multi-component template search configuration");
    if (grammars.size() != domain->expressionTemplate().siteCount())
        throw std::invalid_argument("template grammar count differs");
    for (const auto& grammar : grammars)
        if (config.fitting) {
            for (const double terminal : grammar.constants)
                if (terminal < config.fitting->lower || terminal > config.fitting->upper)
                    throw std::invalid_argument("template terminal outside fitting interval");
            if (grammar.continuousProbability > 0 &&
                (grammar.continuousLower < config.fitting->lower ||
                 grammar.continuousUpper > config.fitting->upper))
                throw std::invalid_argument("template sampling outside fitting interval");
        }
    if (config.fitting)
        for (std::size_t site = 0; site < domain->expressionTemplate().siteCount(); ++site)
            validateConstantFitConfigForLimit(
                *config.fitting, domain->expressionTemplate().componentLimits(site).maxAbsConstant);
    for (std::size_t site = 0; site < domain->expressionTemplate().siteCount(); ++site) {
        for (const auto operation : grammars[site].operators)
            if (std::find(domain->allowedOperators(site).begin(),
                          domain->allowedOperators(site).end(),
                          operation) == domain->allowedOperators(site).end())
                throw std::invalid_argument("component grammar exceeds operator restrictions");
        genetik_detail::validateExpressionGrammar(
            domain->allowedVariables(site).size(),
            domain->expressionTemplate().componentLimits(site), grammars[site]);
    }
    if (!objective)
        objective = std::make_shared<TrainingObjective>(
            domain->problem(), config.maxTrainingLossAttempts, config.diagnostics);
    if (objective->problem() != domain->problem() ||
        objective->maximum() != config.maxTrainingLossAttempts ||
        objective->diagnostics() != config.diagnostics || objective->work() != TrainingWork{})
        throw std::invalid_argument("template search requires a fresh matching objective");
    if (random.execution().requested() > 1 &&
        (!objective->parallelEvaluationSafe() || config.diagnostics))
        throw std::invalid_argument("parallel template search requires qualified objective");
    if (random.execution().requested() > 1)
        (void)objective->forkParallel(random.execution());

    auto preparation = std::make_shared<ConstantRefinement>(
        objective, config.fitting, config.fitEveryGenerations,
        std::make_shared<MultiComponentTemplateParameters>(domain));
    auto context = std::make_shared<const MultiComponentTemplateContext>(domain, preparation);
    MultiComponentTemplateVariation variation(context, grammars);
    return genetik_detail::runSymbolicSearch(domain->problem(), config, random, objective,
                                             preparation, std::move(variation));
}

inline SymbolicRegressionResult
multiComponentTemplateRegression(std::shared_ptr<const MultiComponentTemplateProblem> domain,
                                 const std::array<ExpressionGrammar, 2>& grammars,
                                 const SymbolicRegressionConfig& config, RandomContext& random,
                                 std::shared_ptr<TrainingObjective> objective = {}) {
    return multiComponentTemplateRegression(
        std::move(domain), std::vector<ExpressionGrammar>{grammars[0], grammars[1]}, config, random,
        std::move(objective));
}

inline SymbolicRegressionResult
multiComponentTemplateRegression(std::shared_ptr<const MultiComponentTemplateProblem> domain,
                                 std::initializer_list<ExpressionGrammar> grammars,
                                 const SymbolicRegressionConfig& config, RandomContext& random,
                                 std::shared_ptr<TrainingObjective> objective = {}) {
    return multiComponentTemplateRegression(std::move(domain),
                                            std::vector<ExpressionGrammar>(grammars), config,
                                            random, std::move(objective));
}
