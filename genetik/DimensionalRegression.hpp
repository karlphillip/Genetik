#pragma once
#include "PhysicalDimensions.hpp"
#include "SymbolicRegression.hpp"

struct DimensionalGrammarLimits {
    std::size_t maxUnitStates = 48, maxPlanningSteps = 50000, maxGenerationSteps = 10000,
                maxWorkspaceBytes = 8 * 1024 * 1024;
};
// Unit checking precedes numerical measurement, including every fitting probe.
class DimensionalRegressionProblem final : public RegressionProblem {
  public:
    DimensionalRegressionProblem(const std::vector<RegressionRow> &rows,
                                 const std::vector<PhysicalDimension> &variables, PhysicalDimension output,
                                 ExpressionLimits limits = {}, DimensionalLimits inference = {})
        : RegressionProblem(rows, limits), variables_(variables), output_(output), inference_(inference) {
        if (variables.size() != dimensions())
            throw std::invalid_argument("dimensional regression schema");
        if (!inference.maxNodes || !inference.maxWorkspaceBytes || !inference.maxInferenceSteps)
            throw std::invalid_argument("dimensional limits");
    }
    bool parallelEvaluationSafe() const noexcept override { return true; }
    bool accepts(const Expression &e) const {
        try {
            return inferDimensions(e, variables_, {}, inference_).back() == output_;
        } catch (const std::invalid_argument &) {
            return false;
        }
    }
    RegressionMetrics measure(const Expression &e, std::uint64_t *attempts = nullptr) const override {
        if (!accepts(e))
            return {Fitness::invalid(), {}, 0};
        return RegressionProblem::measure(e, attempts);
    }
    std::span<const PhysicalDimension> variables() const noexcept { return variables_; }
    const PhysicalDimension &output() const noexcept { return output_; }
    DimensionalLimits inferenceLimits() const noexcept { return inference_; }

  private:
    std::vector<PhysicalDimension> variables_;
    PhysicalDimension output_;
    DimensionalLimits inference_;
};
// Freeze constant exponent occurrences for dimensionful bases; changing an
// exponent is a type change, rather than ordinary coefficient refinement.
class DimensionalConstantSelection final : public ConstantParameterSelection {
  public:
    explicit DimensionalConstantSelection(std::vector<PhysicalDimension> units) : units_(std::move(units)) {}
    bool parallelEvaluationSafe() const noexcept override { return true; }
    std::vector<std::size_t> parameters(const Expression &e) const override {
        auto units = inferDimensions(e, units_);
        auto nodes = e.nodes();
        std::vector<bool> frozen(nodes.size());
        for (std::size_t i = 0; i < nodes.size(); ++i)
            if (nodes[i].kind() == ExpressionKind::Pow) {
                auto bounds = expressionSubtree(e, i - 1);
                if (bounds.first > 0 && !units[bounds.first - 1].dimensionless() &&
                    nodes[i - 1].kind() == ExpressionKind::Constant)
                    frozen[i - 1] = true;
            }
        std::vector<std::size_t> result;
        for (std::size_t i = 0; i < nodes.size(); ++i)
            if (nodes[i].kind() == ExpressionKind::Constant && !frozen[i])
                result.push_back(i);
        return result;
    }

  private:
    std::vector<PhysicalDimension> units_;
};
// Finite constructive unit grammar. Planning never reads observations or RNG.
// One smallest discovered witness per unit; the bounded closure is intentionally
// incomplete. Unreachable output units fail before evolution starts.
class DimensionalVariation {
    struct Witness {
        PhysicalDimension unit;
        Expression expression;
    };
    struct Production {
        ExpressionKind kind;
        std::array<std::size_t, 3> args{};
        unsigned exponent = 0;
        std::optional<double> realExponent;
    };

  public:
    DimensionalVariation(std::shared_ptr<const DimensionalRegressionProblem> problem,
                         ExpressionGrammar grammar = {},
                         std::shared_ptr<RegressionPreparation> preparation = {},
                         DimensionalGrammarLimits limits = {})
        : problem_(std::move(problem)), grammar_(std::move(grammar)), preparation_(std::move(preparation)),
          limits_(limits) {
        if (!problem_ || !limits.maxUnitStates || limits.maxUnitStates > 256 || !limits.maxPlanningSteps ||
            !limits.maxGenerationSteps || limits.maxGenerationSteps > 1000000 ||
            limits.maxPlanningSteps > 10000000 || !limits.maxWorkspaceBytes ||
            grammar_.initialMaxDepth > 64 || grammar_.mutationMaxDepth > 64)
            throw std::invalid_argument("bounded dimensional grammar limits");
        genetik_detail::validateExpressionGrammar(problem_->dimensions(), problem_->limits(), grammar_);
        if (problem_->limits().maxNodes >
            limits.maxWorkspaceBytes / (limits.maxUnitStates * sizeof(ExpressionNode) * 2))
            throw DimensionResourceExhausted("dimensional planner witness storage");
        for (std::size_t i = 0; i < problem_->dimensions(); ++i)
            add(problem_->variables()[i],
                Expression::variable(i, problem_->dimensions(), problem_->limits()));
        for (double c : grammar_.constants)
            add({}, Expression::constant(c, problem_->dimensions(), problem_->limits()));
        // Fixed rounds, insertion order, and left/right index order define planning.
        for (std::size_t round = 0; round < problem_->limits().maxDepth; ++round) {
            const auto count = pool_.size();
            bool changed = false;
            for (auto kind : grammar_.operators) {
                if (planningStopped_)
                    break;
                const auto arity = expressionArity(kind);
                for (std::size_t a = 0; a < count && !planningStopped_; ++a) {
                    if (arity == 1) {
                        for (unsigned exponent : powerSource(kind))
                            changed = plan(kind, {a, 0, 0}, exponent) || changed;
                    } else if (arity == 2) {
                        if (kind == ExpressionKind::Pow) {
                            if (grammar_.powerExponentsAreConstants)
                                for (double exponent : grammar_.realExponents)
                                    changed = plan(kind, {a, 0, 0}, 0, exponent) || changed;
                            else
                                for (std::size_t b = 0; b < count && !planningStopped_; ++b)
                                    changed = plan(kind, {a, b, 0}) || changed;
                        } else
                            for (std::size_t b = 0; b < count && !planningStopped_; ++b)
                                changed = plan(kind, {a, b, 0}) || changed;
                    } else if (arity == 3)
                        changed = plan(kind, {a, a, a}) || changed;
                }
            }
            if (!changed || planningStopped_)
                break;
        }
        target_ = find(problem_->output());
        if (target_ && pool_[*target_].expression.depth() > grammar_.initialMaxDepth)
            throw std::invalid_argument("initial depth cannot construct output units");
        if (!target_)
            throw std::invalid_argument(
                "output unit unreachable within declared dimensional planning limits");
    }
    void markElite(ExpressionCandidate &c) const { c.setPreparationOrigin(PreparationOrigin::Elite); }
    void markOffspring(ExpressionCandidate &c, const ExpressionCandidate &parent) const {
        c.setPreparationOrigin(c.expression().sameRepresentation(parent.expression())
                                   ? PreparationOrigin::UnchangedOffspring
                                   : PreparationOrigin::ChangedOffspring);
    }
    VariationStatistics statistics() const noexcept { return statistics_; }
    void restoreStatistics(const VariationStatistics &s) {
        if (statistics_ != VariationStatistics{} || s.mutationChanges > s.mutationProposals ||
            s.crossoverProposals > UINT64_MAX / 2 || s.crossoverChanges > 2 * s.crossoverProposals)
            throw CheckpointError("dimensional variation counters");
        statistics_ = s;
    }
    ExpressionCandidate initialize(std::mt19937 &rng) {
        increment(statistics_.initializations);
        std::size_t steps = 0;
        return ExpressionCandidate(generate(*target_, grammar_.initialMaxDepth, rng, steps), problem_,
                                   preparation_);
    }
    ExpressionCandidate mutation(const ExpressionCandidate &parent, float rate, std::mt19937 &rng) {
        require(parent);
        auto result = parent;
        if (std::uniform_real_distribution<float>(0, 1)(rng) >= rate)
            return result;
        auto units = inferDimensions(parent.expression(), problem_->variables());
        for (std::size_t attempt = 0; attempt < grammar_.proposalLimit; ++attempt) {
            increment(statistics_.mutationProposals);
            auto root = index(units.size(), rng);
            auto unit = find(units[root]);
            if (!unit || pool_[*unit].expression.depth() > grammar_.mutationMaxDepth) {
                increment(statistics_.structuralRejections);
                continue;
            }
            std::size_t steps = 0;
            auto body = generate(*unit, grammar_.mutationMaxDepth, rng, steps);
            auto next = replaceExpressionSubtree(parent.expression(), root, body, problem_->limits());
            if (next && problem_->accepts(*next) && genetik_detail::extendedGrammarAccepts(*next, grammar_)) {
                result.replaceExpression(*next);
                increment(statistics_.mutationChanges, result.text() != parent.text());
                return result;
            }
            increment(statistics_.structuralRejections);
        }
        increment(statistics_.mutationFallbacks);
        return result;
    }
    void crossover(const ExpressionCandidate &father, const ExpressionCandidate &mother,
                   ExpressionCandidate &a, ExpressionCandidate &b, float rate, std::mt19937 &rng) {
        require(father);
        require(mother);
        a = father;
        b = mother;
        if (std::uniform_real_distribution<float>(0, 1)(rng) >= rate)
            return;
        auto fu = inferDimensions(father.expression(), problem_->variables()),
             mu = inferDimensions(mother.expression(), problem_->variables());
        for (std::size_t attempt = 0; attempt < grammar_.proposalLimit; ++attempt) {
            increment(statistics_.crossoverProposals);
            auto f = index(fu.size(), rng), m = index(mu.size(), rng);
            if (fu[f] != mu[m]) {
                increment(statistics_.structuralRejections);
                continue;
            }
            auto left = replaceExpressionSubtree(father.expression(), f, subtree(mother.expression(), m),
                                                 problem_->limits());
            auto right = replaceExpressionSubtree(mother.expression(), m, subtree(father.expression(), f),
                                                  problem_->limits());
            if (left && right && problem_->accepts(*left) && problem_->accepts(*right) &&
                genetik_detail::extendedGrammarAccepts(*left, grammar_) &&
                genetik_detail::extendedGrammarAccepts(*right, grammar_)) {
                a.replaceExpression(*left);
                b.replaceExpression(*right);
                increment(statistics_.crossoverChanges,
                          (a.text() != father.text()) + (b.text() != mother.text()));
                return;
            }
            increment(statistics_.structuralRejections);
        }
        increment(statistics_.crossoverFallbacks);
    }
    std::size_t plannedUnits() const noexcept { return pool_.size(); }

  private:
    static void increment(std::uint64_t &count, std::uint64_t n = 1) {
        if (n > UINT64_MAX - count)
            throw std::overflow_error("dimensional variation counter");
        count += n;
    }
    static std::size_t index(std::size_t size, std::mt19937 &rng) {
        return std::uniform_int_distribution<std::size_t>(0, size - 1)(rng);
    }
    void require(const ExpressionCandidate &c) const {
        if (c.problem() != problem_ || !problem_->accepts(c.expression()))
            throw std::invalid_argument("dimensional candidate context");
    }
    static Expression subtree(const Expression &e, std::size_t root) {
        auto [a, b] = expressionSubtree(e, root);
        auto nodes = e.nodes();
        return Expression(std::span(nodes).subspan(a, b - a), e.variableCount(), e.limits());
    }
    std::optional<std::size_t> find(const PhysicalDimension &unit) const {
        for (std::size_t i = 0; i < pool_.size(); ++i)
            if (pool_[i].unit == unit)
                return i;
        return {};
    }
    bool add(const PhysicalDimension &unit, const Expression &e) {
        auto found = find(unit);
        if (found) {
            if (e.nodeCount() < pool_[*found].expression.nodeCount() ||
                (e.nodeCount() == pool_[*found].expression.nodeCount() &&
                 e.depth() < pool_[*found].expression.depth())) {
                pool_[*found].expression = e;
                return true;
            }
            return false;
        }
        if (pool_.size() == limits_.maxUnitStates)
            return false;
        pool_.push_back({unit, e});
        return true;
    }
    std::vector<unsigned> powerSource(ExpressionKind k) const {
        if (k == ExpressionKind::Powi)
            return grammar_.exponents;
        if (k == ExpressionKind::NegativePowi)
            return grammar_.negativeExponents;
        return {0};
    }
    Expression compose(const Production &p, const std::array<Expression, 3> &args) const {
        auto k = p.kind;
        auto arity = expressionArity(k);
        if (k == ExpressionKind::Powi)
            return Expression::power(args[0], p.exponent);
        if (k == ExpressionKind::NegativePowi)
            return Expression::integerPower(args[0], -static_cast<int>(p.exponent));
        if (arity == 1)
            return Expression::unary(k, args[0]);
        if (arity == 2)
            return Expression::binary(
                k, args[0],
                p.realExponent
                    ? Expression::constant(*p.realExponent, problem_->dimensions(), problem_->limits())
                    : args[1]);
        return Expression::ternary(k, args[0], args[1], args[2]);
    }
    bool plan(ExpressionKind kind, std::array<std::size_t, 3> args, unsigned exponent = 0,
              std::optional<double> real = {}) {
        if (planningSteps_ == limits_.maxPlanningSteps) {
            planningStopped_ = true;
            return false;
        }
        ++planningSteps_;
        std::array<PhysicalDimension, 3> units{pool_[args[0]].unit, pool_[args[1]].unit, pool_[args[2]].unit};
        if (real)
            units[1] = {};
        try {
            auto unit =
                scalarResultDimension(kind, std::span(units).first(expressionArity(kind)), real, exponent);
            std::array<Expression, 3> expressions{pool_[args[0]].expression, pool_[args[1]].expression,
                                                  pool_[args[2]].expression};
            return add(unit, compose({kind, args, exponent, real}, expressions));
        } catch (const std::length_error &) {
            return false;
        } catch (const std::invalid_argument &) {
            return false;
        } catch (const DimensionResourceExhausted &) {
            return false;
        }
    }
    Expression generate(std::size_t target, std::size_t depth, std::mt19937 &rng, std::size_t &steps) const {
        // Witness fallback is finite, unit-valid and independent of observations.
        const auto fallback = [&] { return pool_[target].expression; };
        if (!depth || steps >= limits_.maxGenerationSteps)
            return fallback();
        ++steps;
        std::vector<Production> choices;
        const auto cap = std::min(limits_.maxGenerationSteps - steps,
                                  limits_.maxWorkspaceBytes / (128 * sizeof(Production)));
        auto consider = [&](Production p) {
            if (choices.size() == cap)
                return;
            std::array<PhysicalDimension, 3> units{pool_[p.args[0]].unit, pool_[p.args[1]].unit,
                                                   pool_[p.args[2]].unit};
            if (p.realExponent)
                units[1] = {};
            try {
                if (scalarResultDimension(p.kind, std::span(units).first(expressionArity(p.kind)),
                                          p.realExponent, p.exponent) != pool_[target].unit)
                    return;
                for (std::size_t j = 0; j < expressionArity(p.kind); ++j)
                    if (!(p.realExponent && j == 1) && pool_[p.args[j]].expression.depth() >= depth)
                        return;
                choices.push_back(p);
            } catch (const std::invalid_argument &) {
            } catch (const DimensionResourceExhausted &) {
            }
        };
        std::size_t scans = 0;
        for (auto k : grammar_.operators) {
            if (scans >= cap)
                break;
            const auto arity = expressionArity(k);
            for (std::size_t a = 0; a < pool_.size() && scans < cap; ++a) {
                if (arity == 1) {
                    for (auto e : powerSource(k)) {
                        if (scans++ >= cap)
                            break;
                        consider({k, {a, 0, 0}, e, {}});
                    }
                } else if (k == ExpressionKind::Pow && grammar_.powerExponentsAreConstants) {
                    for (double e : grammar_.realExponents) {
                        if (scans++ >= cap)
                            break;
                        consider({k, {a, 0, 0}, 0, e});
                    }
                } else if (arity == 2) {
                    for (std::size_t b = 0; b < pool_.size() && scans < cap; ++b) {
                        ++scans;
                        consider({k, {a, b, 0}, 0, {}});
                    }
                } else {
                    ++scans;
                    consider({k, {a, a, a}, 0, {}});
                }
            }
        }
        steps += std::min(scans, cap);
        // A separate terminal choice retains variables and coefficient diversity.
        if (choices.empty() || std::bernoulli_distribution(1 - grammar_.growBinaryProbability)(rng)) {
            if (pool_[target].unit.dimensionless() && !grammar_.constants.empty() &&
                std::bernoulli_distribution(.5)(rng))
                return Expression::constant(grammar_.constants[index(grammar_.constants.size(), rng)],
                                            problem_->dimensions(), problem_->limits());
            return fallback();
        }
        auto p = choices[index(choices.size(), rng)];
        std::array<Expression, 3> args{fallback(), fallback(), fallback()};
        for (std::size_t j = 0; j < expressionArity(p.kind); ++j)
            if (!(p.realExponent && j == 1))
                args[j] = generate(p.args[j], depth - 1, rng, steps);
        try {
            return compose(p, args);
        } catch (const std::length_error &) {
            return fallback();
        }
    }
    std::shared_ptr<const DimensionalRegressionProblem> problem_;
    ExpressionGrammar grammar_;
    std::shared_ptr<RegressionPreparation> preparation_;
    DimensionalGrammarLimits limits_;
    std::vector<Witness> pool_;
    std::optional<std::size_t> target_;
    VariationStatistics statistics_;
    std::size_t planningSteps_ = 0;
    bool planningStopped_ = false;
};
inline SymbolicRegressionResult
dimensionalRegression(std::shared_ptr<const DimensionalRegressionProblem> problem,
                      const SymbolicRegressionConfig &config, RandomContext &random,
                      DimensionalGrammarLimits limits = {}) {
    if (random.evaluationAttempts() != 0 || !config.population || config.elites >= config.population ||
        !config.fitEveryGenerations || !std::isfinite(config.trainingLossThreshold) ||
        config.trainingLossThreshold < 0)
        throw std::invalid_argument("fresh dimensional run/configuration required");
    auto objective =
        std::make_shared<TrainingObjective>(problem, config.maxTrainingLossAttempts, config.diagnostics);
    auto selection = std::make_shared<DimensionalConstantSelection>(
        std::vector<PhysicalDimension>(problem->variables().begin(), problem->variables().end()));
    auto preparation = std::make_shared<ConstantRefinement>(objective, config.fitting,
                                                            config.fitEveryGenerations, selection);
    DimensionalVariation variation(problem, config.grammar, preparation, limits);
    return genetik_detail::runSymbolicSearch(problem, config, random, objective, preparation,
                                             std::move(variation));
}
