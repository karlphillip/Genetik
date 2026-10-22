#pragma once
#include "Regression.hpp"
#include <random>
#include <cstdint>

struct ExpressionGrammar {
    std::vector<ExpressionKind> operators{ExpressionKind::Add, ExpressionKind::Subtract, ExpressionKind::Multiply};
    std::vector<double> constants{-2, -1, 0, 1, 2};
    std::vector<unsigned> exponents{0,1,2,3,4,5,6,7,8};
    std::size_t initialMaxDepth = 4;
    std::size_t mutationMaxDepth = 3;
    double growBinaryProbability = .65;
    std::size_t proposalLimit = 8;
    // Default preserves the original discrete-only draw sequence. With a positive continuous
    // probability, choose uniform [lower,upper) instead of a discrete terminal at that frequency.
    double continuousProbability = 0, continuousLower = -8, continuousUpper = 8;
};
struct VariationStatistics {
    std::uint64_t initializations = 0, mutationProposals = 0, crossoverProposals = 0;
    std::uint64_t structuralRejections = 0, mutationFallbacks = 0, crossoverFallbacks = 0;
    std::uint64_t mutationChanges = 0, crossoverChanges = 0;
    bool operator==(const VariationStatistics&) const = default;
};

// Half-open postfix subtree interval; operand order is never changed by slicing.
inline std::pair<std::size_t, std::size_t> expressionSubtree(const Expression& expression, std::size_t root) {
    const auto nodes = expression.nodes();
    if (root >= nodes.size()) throw std::out_of_range("subtree root outside expression");
    std::vector<std::size_t> starts;
    for (std::size_t i = 0; i <= root; ++i) {
        const auto arity = expressionArity(nodes[i].kind());
        if (arity == 0) starts.push_back(i);
        else if (arity == 2) starts.pop_back(); // consume right; unary/left start remains
    }
    return {starts.back(), root + 1};
}

// nullopt means a bounded structural rejection, never an unexpected exception.
inline std::optional<Expression> replaceExpressionSubtree(const Expression& parent, std::size_t root,
                          const Expression& replacement, ExpressionLimits limits) {
    if (parent.variableCount() != replacement.variableCount()) throw std::invalid_argument("subtree schemas differ");
    (void)Expression::constant(0, parent.variableCount(), limits); // reject bad configuration, not a proposal
    const auto [begin, end] = expressionSubtree(parent, root);
    const auto original = parent.nodes(), inserted = replacement.nodes();
    const auto remaining = original.size() - (end - begin);
    if (remaining > limits.maxNodes || inserted.size() > limits.maxNodes - remaining) return {};
    std::vector<ExpressionNode> next;
    next.reserve(remaining + inserted.size());
    next.insert(next.end(), original.begin(), original.begin() + begin);
    next.insert(next.end(), inserted.begin(), inserted.end());
    next.insert(next.end(), original.begin() + end, original.end());
    std::vector<std::size_t> depths;
    std::size_t work=0;
    for (const auto& node : next) {
        const auto kind = node.kind();
        if (kind == ExpressionKind::Constant) {
            if (std::abs(node.constantValue()) > limits.maxAbsConstant) return {};
            depths.push_back(1);
        } else if (kind == ExpressionKind::Variable) depths.push_back(1);
        else {
            const std::size_t cost = kind==ExpressionKind::Powi ? (node.exponent()?node.exponent()-1:0) : 1;
            if(cost>limits.maxArithmeticOperations-work)return {};
            work+=cost;
            auto child = depths.back();
            if (expressionArity(kind) == 2) { depths.pop_back(); child = std::max(depths.back(), child); }
            if (child >= limits.maxDepth) return {}; // destination depth checked in the complete tree
            depths.back() = child + 1;
        }
    }
    return Expression(next, parent.variableCount(), limits);
}

namespace genetik_detail {
inline void validateExpressionGrammar(std::size_t dimensions,ExpressionLimits limits,const ExpressionGrammar& grammar) {
        if ((grammar.constants.empty() && grammar.continuousProbability == 0 && dimensions == 0) || grammar.initialMaxDepth == 0 ||
            grammar.mutationMaxDepth == 0 || grammar.proposalLimit == 0 ||
            !std::isfinite(grammar.growBinaryProbability) || grammar.growBinaryProbability < 0 || grammar.growBinaryProbability > 1)
            throw std::invalid_argument("invalid expression grammar");
        if (!std::isfinite(grammar.continuousProbability) || grammar.continuousProbability < 0 || grammar.continuousProbability > 1 ||
            (grammar.continuousProbability > 0 && (!std::isfinite(grammar.continuousLower) || !std::isfinite(grammar.continuousUpper) ||
             grammar.continuousLower > grammar.continuousUpper || std::abs(grammar.continuousLower) > limits.maxAbsConstant ||
             std::abs(grammar.continuousUpper) > limits.maxAbsConstant ||
             !std::isfinite(grammar.continuousUpper - grammar.continuousLower))) ||
            (grammar.constants.empty() && grammar.continuousProbability > 0 && grammar.continuousProbability < 1))
            throw std::invalid_argument("invalid continuous constant source");
        genetik_detail::validateExponents(grammar.exponents);
        for (auto kind : grammar.operators)
            if (expressionArity(kind) == 0) throw std::invalid_argument("grammar operator cannot be a terminal");
        for (double value : grammar.constants)
            if (!std::isfinite(value) || std::abs(value) > limits.maxAbsConstant)
                throw std::invalid_argument("terminal outside problem constant bound");
}
inline Expression generateExpression(std::mt19937& rng,std::size_t maximumDepth,std::size_t dimensions,
       ExpressionLimits limits,const ExpressionGrammar& grammar,const std::vector<std::size_t>* allowed=nullptr) {
        struct Task { std::size_t budget, depth; bool emit; ExpressionKind kind; unsigned exponent=0; std::size_t work=std::numeric_limits<std::size_t>::max(); };
        const auto variableCount=allowed ? allowed->size():dimensions;
        const auto index=[](std::size_t n,std::mt19937& engine){return std::uniform_int_distribution<std::size_t>(0,n-1)(engine);};
        const auto depth = 1 + index(std::min(maximumDepth, limits.maxDepth), rng);
        const bool full = std::bernoulli_distribution(.5)(rng);
        std::vector<Task> tasks{{limits.maxNodes, depth, false, ExpressionKind::Add, 0, limits.maxArithmeticOperations}};
        std::vector<ExpressionNode> nodes;
        while (!tasks.empty()) {
            const auto task = tasks.back(); tasks.pop_back();
            if (task.emit) {
                nodes.push_back(task.kind==ExpressionKind::Powi ? ExpressionNode::power(task.exponent) : expressionArity(task.kind) == 1 ? ExpressionNode::unary(task.kind) : ExpressionNode::binary(task.kind));
                continue;
            }
            std::vector<ExpressionKind> feasible;
            if (task.depth > 1) for (auto kind : grammar.operators)
                if (task.budget > expressionArity(kind) && (kind==ExpressionKind::Powi ?
                    std::any_of(grammar.exponents.begin(),grammar.exponents.end(),[&](unsigned e){return (e?e-1:0)<=task.work;}) : task.work>0)) feasible.push_back(kind);
            const bool operation = !feasible.empty() &&
                (full || std::bernoulli_distribution(grammar.growBinaryProbability)(rng));
            if (operation) {
                const auto kind = feasible[index(feasible.size(), rng)];
                unsigned exponent=0;
                if(kind==ExpressionKind::Powi) {
                    std::vector<unsigned> options;for(auto e:grammar.exponents)if((e?e-1:0)<=task.work)options.push_back(e);
                    exponent=options[index(options.size(),rng)];
                }
                const auto cost=kind==ExpressionKind::Powi ? (exponent?exponent-1:0) : 1;
                const auto remainingWork=task.work-cost;
                tasks.push_back({0, 0, true, kind, exponent});
                if (expressionArity(kind) == 1) tasks.push_back({task.budget - 1, task.depth - 1, false, kind, 0, remainingWork});
                else {
                    const auto left = 1 + index(task.budget - 2, rng);
                    const auto leftWork=limits.maxArithmeticOperations==std::numeric_limits<std::size_t>::max() ? remainingWork : remainingWork/2;
                    const auto rightWork=limits.maxArithmeticOperations==std::numeric_limits<std::size_t>::max() ? remainingWork : remainingWork-leftWork;
                    tasks.push_back({task.budget - 1 - left, task.depth - 1, false, kind, 0, rightWork});
                    tasks.push_back({left, task.depth - 1, false, kind, 0, leftWork});
                }
            } else if (variableCount && ((grammar.constants.empty() && grammar.continuousProbability == 0) || std::bernoulli_distribution(.5)(rng)))
                nodes.push_back(ExpressionNode::variable(allowed ? (*allowed)[index(variableCount,rng)]:index(dimensions,rng)));
            else {
                const bool continuous = grammar.continuousProbability > 0 && std::bernoulli_distribution(grammar.continuousProbability)(rng);
                const double value = continuous ? std::uniform_real_distribution<double>(grammar.continuousLower, grammar.continuousUpper)(rng)
                                                : grammar.constants[index(grammar.constants.size(), rng)];
                nodes.push_back(ExpressionNode::constant(value));
            }
        }
        return Expression(nodes, dimensions, limits);
}
}

class ExpressionVariation {
public:
    ExpressionVariation(std::shared_ptr<const RegressionProblem> problem, ExpressionGrammar grammar = {},
                        std::shared_ptr<RegressionPreparation> preparation = {})
        : problem_(std::move(problem)), grammar_(std::move(grammar)), preparation_(std::move(preparation)) {
        if(!problem_)throw std::invalid_argument("expression problem required");
        genetik_detail::validateExpressionGrammar(problem_->dimensions(),problem_->limits(),grammar_);
    }
    void markElite(ExpressionCandidate& candidate) const {candidate.setPreparationOrigin(PreparationOrigin::Elite);}
    void markOffspring(ExpressionCandidate& candidate,const ExpressionCandidate& parent) const {
        candidate.setPreparationOrigin(candidate.expression().sameRepresentation(parent.expression()) ?
            PreparationOrigin::UnchangedOffspring:PreparationOrigin::ChangedOffspring);
    }
    VariationStatistics statistics() const noexcept { return statistics_; }
    void restoreStatistics(const VariationStatistics& state) {
        if(statistics_!=VariationStatistics{} || state.mutationChanges>state.mutationProposals ||
           state.crossoverProposals>std::numeric_limits<std::uint64_t>::max()/2 || state.crossoverChanges>state.crossoverProposals*2)
            throw std::invalid_argument("invalid variation checkpoint state");
        statistics_=state;
    }
    ExpressionCandidate initialize(std::mt19937& rng) {
        count(statistics_.initializations);
        return ExpressionCandidate(randomExpression(rng, grammar_.initialMaxDepth), problem_, preparation_);
    }
    ExpressionCandidate mutation(const ExpressionCandidate& parent, float rate, std::mt19937& rng) {
        requireProblem(parent);
        ExpressionCandidate child = parent;
        if (std::uniform_real_distribution<float>(0, 1)(rng) >= rate) return child;
        for (std::size_t i = 0; i < grammar_.proposalLimit; ++i) {
            count(statistics_.mutationProposals);
            const auto root = index(parent.expression().nodeCount(), rng);
            if (std::find(grammar_.operators.begin(),grammar_.operators.end(),ExpressionKind::Powi)!=grammar_.operators.end() &&
                parent.expression().nodes()[root].kind() == ExpressionKind::Powi &&
                std::bernoulli_distribution(.5)(rng)) {
                std::optional<Expression> changed;
                try { changed=parent.expression().withExponent(root,grammar_.exponents[index(grammar_.exponents.size(),rng)]); }
                catch(const std::length_error&){count(statistics_.structuralRejections);continue;}
                child.replaceExpression(*changed);
                count(statistics_.mutationChanges, child.text()!=parent.text()); return child;
            }
            const auto subtree = randomExpression(rng, grammar_.mutationMaxDepth);
            auto next = replaceExpressionSubtree(parent.expression(), root, subtree, problem_->limits());
            if (!next) { count(statistics_.structuralRejections); continue; }
            child.replaceExpression(*next);
            count(statistics_.mutationChanges, child.text() != parent.text());
            return child;
        }
        count(statistics_.mutationFallbacks);
        return child;
    }
    void crossover(const ExpressionCandidate& father, const ExpressionCandidate& mother,
                   ExpressionCandidate& a, ExpressionCandidate& b, float rate, std::mt19937& rng) {
        requireProblem(father); requireProblem(mother);
        ExpressionCandidate left = father, right = mother;
        if (std::uniform_real_distribution<float>(0, 1)(rng) < rate) {
            bool accepted = false;
            for (std::size_t i = 0; i < grammar_.proposalLimit; ++i) {
                count(statistics_.crossoverProposals);
                const auto fr = index(father.expression().nodeCount(), rng), mr = index(mother.expression().nodeCount(), rng);
                const auto fs = subtree(father.expression(), fr), ms = subtree(mother.expression(), mr);
                auto la = replaceExpressionSubtree(father.expression(), fr, ms, problem_->limits());
                auto rb = replaceExpressionSubtree(mother.expression(), mr, fs, problem_->limits());
                if (!la || !rb) { count(statistics_.structuralRejections); continue; }
                left.replaceExpression(*la); right.replaceExpression(*rb);
                count(statistics_.crossoverChanges, (left.text() != father.text()) + (right.text() != mother.text()));
                accepted = true; break;
            }
            if (!accepted) count(statistics_.crossoverFallbacks);
        }
        a = std::move(left); b = std::move(right);
    }
private:
    static void count(std::uint64_t& value, std::uint64_t amount = 1) {
        if (amount > std::numeric_limits<std::uint64_t>::max() - value) throw std::overflow_error("variation counter exhausted");
        value += amount;
    }
    static std::size_t index(std::size_t count, std::mt19937& rng) {
        return std::uniform_int_distribution<std::size_t>(0, count - 1)(rng);
    }
    void requireProblem(const ExpressionCandidate& c) const {
        if (c.problem() != problem_) throw std::invalid_argument("variation problem differs from candidate");
    }
    static Expression subtree(const Expression& e, std::size_t root) {
        const auto [begin, end] = expressionSubtree(e, root);
        const auto nodes = e.nodes();
        return Expression(std::span(nodes.data() + begin, end - begin), e.variableCount(), e.limits());
    }
    Expression randomExpression(std::mt19937& rng, std::size_t maximumDepth) const {
        return genetik_detail::generateExpression(rng,maximumDepth,problem_->dimensions(),problem_->limits(),grammar_);
    }
    std::shared_ptr<const RegressionProblem> problem_;
    const ExpressionGrammar grammar_;
    VariationStatistics statistics_;
    std::shared_ptr<RegressionPreparation> preparation_;
};
