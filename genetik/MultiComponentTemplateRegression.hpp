#pragma once

#include "ScalarTemplateRegression.hpp"

#include <array>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

// A fixed expression tree with two disjoint, independently named substitution
// occurrences. The source expressions at the sites are placeholders only.
class MultiScalarExpressionTemplate final {
public:
    MultiScalarExpressionTemplate(const Expression& source, std::size_t firstRoot,
                                  std::size_t secondRoot,
                                  std::array<ExpressionLimits, 2> componentLimits,
                                  ExpressionLimits modelLimits,
                                  std::size_t maximumMaterializationBytes = 1'048'576)
        : maximumMaterializationBytes_(maximumMaterializationBytes),
          source_(checkedSource(source, modelLimits, maximumMaterializationBytes)),
          componentLimits_(componentLimits), modelLimits_(modelLimits) {
        if (maximumMaterializationBytes_ == 0)
            throw std::invalid_argument("template materialization allowance must be positive");
        const auto firstRange = expressionSubtree(source_, firstRoot);
        const auto secondRange = expressionSubtree(source_, secondRoot);
        if (firstRange.first < secondRange.second && secondRange.first < firstRange.second)
            throw std::invalid_argument("template sites must be disjoint subtree occurrences");
        roots_ = {firstRoot, secondRoot};
        ranges_ = {firstRange, secondRange};
        frozenNodes_ = source_.nodeCount() - (firstRange.second - firstRange.first) -
                       (secondRange.second - secondRange.first);
        if (frozenNodes_ > modelLimits_.maxNodes || modelLimits_.maxNodes - frozenNodes_ < 2)
            throw std::invalid_argument("complete model limit leaves no room for both sites");
        frozenWork_ = arithmeticCostOutsideSites();
        if (frozenWork_ > modelLimits_.maxArithmeticOperations ||
            modelLimits_.maxArithmeticOperations - frozenWork_ < 2)
            throw std::invalid_argument("complete model work limit leaves no room for both sites");
        for (auto& limits : componentLimits_) {
            (void)Expression::constant(0.0, source_.variableCount(), limits);
            limits.maxNodes = std::min(limits.maxNodes, modelLimits_.maxNodes - frozenNodes_ - 1);
            limits.maxArithmeticOperations =
                std::min(limits.maxArithmeticOperations,
                         modelLimits_.maxArithmeticOperations - frozenWork_ - 1);
            limits.maxAbsConstant = std::min(limits.maxAbsConstant, modelLimits_.maxAbsConstant);
            if (limits.maxNodes == 0 || limits.maxDepth == 0)
                throw std::invalid_argument("component limits must allow a terminal");
        }
        if (source_.nodeCount() > maximumMaterializationBytes_ / slotBytes)
            throw std::length_error("template source exceeds materialization allowance");
    }

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

    Expression materialize(const std::array<Expression, 2>& components) const {
        if (components[0].variableCount() != variableCount() ||
            components[1].variableCount() != variableCount())
            throw std::invalid_argument("template component schema differs");
        for (std::size_t site = 0; site < 2; ++site) {
            const auto nodes = components[site].nodeCount();
            if (nodes > componentLimits_[site].maxNodes ||
                components[site].depth() > componentLimits_[site].maxDepth ||
                components[site].arithmeticCost() > componentLimits_[site].maxArithmeticOperations)
                throw std::length_error("component exceeds its expression limits");
        }
        const auto order = ranges_[0].first < ranges_[1].first ? std::array<std::size_t, 2>{0, 1}
                                                               : std::array<std::size_t, 2>{1, 0};
        const auto firstSite = order[0];
        const auto secondSite = order[1];
        const auto size = checkedMaterializedSize(components);
        std::vector<ExpressionNode> nodes;
        nodes.reserve(size);
        const auto sourceNodes = source_.nodes();
        nodes.insert(nodes.end(), sourceNodes.begin(),
                     sourceNodes.begin() + ranges_[firstSite].first);
        append(nodes, components[firstSite]);
        nodes.insert(nodes.end(), sourceNodes.begin() + ranges_[firstSite].second,
                     sourceNodes.begin() + ranges_[secondSite].first);
        append(nodes, components[secondSite]);
        nodes.insert(nodes.end(), sourceNodes.begin() + ranges_[secondSite].second,
                     sourceNodes.end());
        return Expression(nodes, variableCount(), modelLimits_);
    }

    std::array<std::pair<std::size_t, std::size_t>, 2>
    componentRanges(const Expression& model) const {
        if (model.variableCount() != variableCount() || model.nodeCount() > modelLimits_.maxNodes)
            throw std::invalid_argument("model does not match multi-component template schema");
        if (model.nodeCount() > std::numeric_limits<std::size_t>::max() - source_.nodeCount() ||
            source_.nodeCount() + model.nodeCount() > maximumMaterializationBytes_ / slotBytes)
            throw std::length_error("template matching workspace limit");
        const Expression checkedModel(model.nodes(), variableCount(), modelLimits_);
        const auto sourceTree = tree(source_);
        const auto modelTree = tree(checkedModel);
        std::array<std::optional<std::pair<std::size_t, std::size_t>>, 2> captures;
        std::vector<std::pair<std::size_t, std::size_t>> pending{
            {source_.nodeCount() - 1, model.nodeCount() - 1}};
        while (!pending.empty()) {
            const auto [sourceIndex, modelIndex] = pending.back();
            pending.pop_back();
            const auto site = selectedSite(sourceIndex);
            if (site < 2) {
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
        if (!captures[0] || !captures[1])
            throw std::logic_error("template matcher did not reach both sites");
        return {*captures[0], *captures[1]};
    }

    std::array<Expression, 2> components(const Expression& model) const {
        const auto ranges = componentRanges(model);
        return {slice(model, ranges[0], componentLimits_[0]),
                slice(model, ranges[1], componentLimits_[1])};
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

    std::size_t checkedMaterializedSize(const std::array<Expression, 2>& components) const {
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
        return (index >= ranges_[0].first && index < ranges_[0].second) ||
               (index >= ranges_[1].first && index < ranges_[1].second);
    }

    std::size_t selectedSite(std::size_t sourceIndex) const noexcept {
        if (sourceIndex == roots_[0])
            return 0;
        if (sourceIndex == roots_[1])
            return 1;
        return 2;
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
    std::array<std::size_t, 2> roots_{};
    std::array<std::pair<std::size_t, std::size_t>, 2> ranges_{};
    std::array<ExpressionLimits, 2> componentLimits_;
    ExpressionLimits modelLimits_;
    std::size_t frozenNodes_ = 0;
    std::size_t frozenWork_ = 0;
};

class MultiComponentTemplateProblem final {
public:
    MultiComponentTemplateProblem(const MultiScalarExpressionTemplate& scaffold,
                                  std::shared_ptr<const RegressionProblem> problem,
                                  std::array<std::vector<std::size_t>, 2> allowedVariables,
                                  std::array<std::vector<ExpressionKind>, 2> allowedOperators)
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
        for (std::size_t site = 0; site < 2; ++site) {
            validateUnique(allowedVariables_[site], problem_->dimensions(), "template variables");
            validateOperators(allowedOperators_[site]);
        }
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
        if (site > 1 || component.variableCount() != problem_->dimensions())
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

    Expression materialize(const std::array<Expression, 2>& components) const {
        validateComponent(0, components[0]);
        validateComponent(1, components[1]);
        return scaffold_.materialize(components);
    }

    std::array<Expression, 2> components(const Expression& model) const {
        auto components = scaffold_.components(model);
        validateComponent(0, components[0]);
        validateComponent(1, components[1]);
        return components;
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
    const std::array<std::vector<std::size_t>, 2> allowedVariables_;
    const std::array<std::vector<ExpressionKind>, 2> allowedOperators_;
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
        (void)domain_->components(expression);
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
        const auto components = domain_->components(model);
        std::vector<std::size_t> result;
        // Constant fitting operates on full-model postfix indices. Locate the two
        // validated component representations in the frozen materialization.
        const auto selected = domain_->expressionTemplate().componentRanges(model);
        for (std::size_t site = 0; site < 2; ++site) {
            const auto nodes = components[site].nodes();
            for (std::size_t index = 0; index < nodes.size(); ++index)
                if (nodes[index].kind() == ExpressionKind::Constant)
                    result.push_back(selected[site].first + index);
        }
        return result;
    }

private:
    std::shared_ptr<const MultiComponentTemplateProblem> domain_;
};

class MultiComponentTemplateVariation final {
public:
    MultiComponentTemplateVariation(std::shared_ptr<const MultiComponentTemplateContext> context,
                                    std::array<ExpressionGrammar, 2> grammars)
        : context_(std::move(context)), grammars_(std::move(grammars)) {
        if (!context_)
            throw std::invalid_argument("multi-component template context required");
        for (std::size_t site = 0; site < 2; ++site) {
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
            std::array<Expression, 2> components{randomComponent(0, random, true),
                                                 randomComponent(1, random, true)};
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
        const auto site = index(2, random);
        const auto components = context_->domain()->components(parent.expression());
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
            const auto site = index(2, random);
            const auto firstComponents = context_->domain()->components(father.expression());
            const auto secondComponents = context_->domain()->components(mother.expression());
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
        return std::min(grammars_[0].proposalLimit, grammars_[1].proposalLimit);
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
    const std::array<ExpressionGrammar, 2> grammars_;
    VariationStatistics statistics_;
};

inline SymbolicRegressionResult
multiComponentTemplateRegression(std::shared_ptr<const MultiComponentTemplateProblem> domain,
                                 const std::array<ExpressionGrammar, 2>& grammars,
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
        for (std::size_t site = 0; site < 2; ++site)
            validateConstantFitConfigForLimit(
                *config.fitting, domain->expressionTemplate().componentLimits(site).maxAbsConstant);
    for (std::size_t site = 0; site < 2; ++site) {
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
