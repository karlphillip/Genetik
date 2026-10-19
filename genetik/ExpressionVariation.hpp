#pragma once
#include "Regression.hpp"
#include <random>
#include <cstdint>

struct ExpressionGrammar {
    std::vector<ExpressionKind> operators{ExpressionKind::Add, ExpressionKind::Subtract, ExpressionKind::Multiply};
    std::vector<double> constants{-2, -1, 0, 1, 2};
    std::size_t initialMaxDepth = 4;
    std::size_t mutationMaxDepth = 3;
    double growBinaryProbability = .65;
    std::size_t proposalLimit = 8;
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
        const auto kind = nodes[i].kind();
        if (kind == ExpressionKind::Variable || kind == ExpressionKind::Constant) starts.push_back(i);
        else starts.pop_back(); // right is consumed; left start remains
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
    for (const auto& node : next) {
        const auto kind = node.kind();
        if (kind == ExpressionKind::Constant) {
            if (std::abs(node.constantValue()) > limits.maxAbsConstant) return {};
            depths.push_back(1);
        } else if (kind == ExpressionKind::Variable) depths.push_back(1);
        else {
            const auto right = depths.back(); depths.pop_back();
            const auto child = std::max(depths.back(), right);
            if (child >= limits.maxDepth) return {}; // destination depth checked in the complete tree
            depths.back() = child + 1;
        }
    }
    return Expression(next, parent.variableCount(), limits);
}

class ExpressionVariation {
public:
    ExpressionVariation(std::shared_ptr<const RegressionProblem> problem, ExpressionGrammar grammar = {})
        : problem_(std::move(problem)), grammar_(std::move(grammar)) {
        if (!problem_ || (grammar_.constants.empty() && problem_->dimensions() == 0) || grammar_.initialMaxDepth == 0 ||
            grammar_.mutationMaxDepth == 0 || grammar_.proposalLimit == 0 ||
            !std::isfinite(grammar_.growBinaryProbability) || grammar_.growBinaryProbability < 0 || grammar_.growBinaryProbability > 1)
            throw std::invalid_argument("invalid expression grammar");
        for (auto kind : grammar_.operators) (void)ExpressionNode::binary(kind);
        for (double value : grammar_.constants)
            if (!std::isfinite(value) || std::abs(value) > problem_->limits().maxAbsConstant)
                throw std::invalid_argument("terminal outside problem constant bound");
    }
    VariationStatistics statistics() const noexcept { return statistics_; }
    ExpressionCandidate initialize(std::mt19937& rng) {
        count(statistics_.initializations);
        return ExpressionCandidate(randomExpression(rng, grammar_.initialMaxDepth), problem_);
    }
    ExpressionCandidate mutation(const ExpressionCandidate& parent, float rate, std::mt19937& rng) {
        requireProblem(parent);
        ExpressionCandidate child = parent;
        if (std::uniform_real_distribution<float>(0, 1)(rng) >= rate) return child;
        for (std::size_t i = 0; i < grammar_.proposalLimit; ++i) {
            count(statistics_.mutationProposals);
            const auto root = index(parent.expression().nodeCount(), rng);
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
        struct Task { std::size_t budget, depth; bool emit; ExpressionKind kind; };
        const auto limits = problem_->limits();
        const auto depth = 1 + index(std::min(maximumDepth, limits.maxDepth), rng);
        const bool full = std::bernoulli_distribution(.5)(rng);
        std::vector<Task> tasks{{limits.maxNodes, depth, false, ExpressionKind::Add}};
        std::vector<ExpressionNode> nodes;
        while (!tasks.empty()) {
            const auto task = tasks.back(); tasks.pop_back();
            if (task.emit) { nodes.push_back(ExpressionNode::binary(task.kind)); continue; }
            const bool binary = !grammar_.operators.empty() && task.budget >= 3 && task.depth > 1 &&
                (full || std::bernoulli_distribution(grammar_.growBinaryProbability)(rng));
            if (binary) {
                const auto kind = grammar_.operators[index(grammar_.operators.size(), rng)];
                const auto left = 1 + index(task.budget - 2, rng);
                tasks.push_back({0, 0, true, kind});
                tasks.push_back({task.budget - 1 - left, task.depth - 1, false, kind});
                tasks.push_back({left, task.depth - 1, false, kind});
            } else if (problem_->dimensions() && (grammar_.constants.empty() || std::bernoulli_distribution(.5)(rng)))
                nodes.push_back(ExpressionNode::variable(index(problem_->dimensions(), rng)));
            else nodes.push_back(ExpressionNode::constant(grammar_.constants[index(grammar_.constants.size(), rng)]));
        }
        return Expression(nodes, problem_->dimensions(), limits);
    }
    std::shared_ptr<const RegressionProblem> problem_;
    const ExpressionGrammar grammar_;
    VariationStatistics statistics_;
};
