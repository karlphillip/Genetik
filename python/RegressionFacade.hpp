#pragma once

#include <SymbolicRegression.hpp>

#include <chrono>
#include <cctype>
#include <charconv>
#include <condition_variable>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

struct PythonRegressionConfig {
    std::uint32_t seed = 0;
    unsigned workers = 1;
    std::uint64_t evaluationBudget = 100000;
    std::uint64_t lossBudget = 100000;
    std::size_t population = 64;
    std::uint32_t maxGenerations = 2000;
    std::size_t maxNodes = 31;
    std::size_t maxDepth = 6;
    double maxAbsConstant = 8;
    bool fitConstants = true;
    std::uint64_t fitLossAttempts = 256;
    double continuousProbability = 0.5;
    std::vector<ExpressionKind> operators{ExpressionKind::Add, ExpressionKind::Subtract,
                                          ExpressionKind::Multiply};
    std::optional<double> maxTimeSeconds;
    double trainingThreshold = 1e-14;
    double fitThreshold = 1e-14;
};

struct PythonRegressionStats {
    bool fitted = false;
    bool timedOut = false;
    SearchStop stop = SearchStop::GenerationLimit;
    std::uint64_t lossAttempts = 0, rowAttempts = 0, fittingInvocations = 0;
    std::uint64_t fittingLossAttempts = 0, fittingProposals = 0, fittingAccepted = 0;
    std::uint64_t invalidLosses = 0, candidateEvaluations = 0;
    std::uint64_t attemptedGenerations = 0, publishedGenerations = 0;
    std::uint64_t bestGeneration = 0;
    std::optional<double> trainingMse;
    std::size_t nodes = 0, depth = 0;
    double searchSeconds = 0;
    std::string rngState;
    std::vector<std::string> rawInitialExpressions;
    std::vector<std::string> preparedInitialExpressions;
    std::vector<std::optional<double>> preparedInitialLosses;
};

class PythonRegressionFacade {
public:
    explicit PythonRegressionFacade(PythonRegressionConfig config) : config_(std::move(config)) {
        if (config_.workers == 0 || config_.workers > 256 || config_.population < 2 ||
            config_.population > 100000 || config_.evaluationBudget < config_.population ||
            config_.lossBudget < config_.population ||
            config_.maxGenerations == 0 || config_.maxNodes == 0 || config_.maxDepth == 0 ||
            !std::isfinite(config_.maxAbsConstant) || config_.maxAbsConstant < 0 ||
            config_.fitLossAttempts == 0 || !std::isfinite(config_.continuousProbability) ||
            config_.continuousProbability < 0 || config_.continuousProbability > 1 ||
            !std::isfinite(config_.trainingThreshold) || config_.trainingThreshold < 0 ||
            !std::isfinite(config_.fitThreshold) || config_.fitThreshold < 0 ||
            (config_.maxTimeSeconds && (!std::isfinite(*config_.maxTimeSeconds) ||
                                       *config_.maxTimeSeconds <= 0 ||
                                       *config_.maxTimeSeconds > 1.0e9)))
            throw std::invalid_argument("invalid Python regression configuration");
        if (config_.operators.empty()) throw std::invalid_argument("operator grammar must be nonempty");
    }

    void fit(const std::vector<RegressionRow>& rows) {
        const auto start = std::chrono::steady_clock::now();
        const auto problem = RegressionProblem::create(
            rows, {config_.maxNodes, config_.maxDepth, config_.maxAbsConstant, 1000});
        SymbolicRegressionConfig search;
        search.population = config_.population;
        search.maxGenerationAttempts = config_.maxGenerations;
        search.maxEvaluations = config_.evaluationBudget;
        search.maxTrainingLossAttempts = config_.lossBudget;
        search.trainingLossThreshold = config_.trainingThreshold;
        search.grammar.operators = config_.operators;
        search.grammar.constants.clear();
        for (double value : {-2.0, -1.0, 0.0, 1.0, 2.0})
            if (std::abs(value) <= config_.maxAbsConstant) search.grammar.constants.push_back(value);
        if (search.grammar.constants.empty()) search.grammar.constants.push_back(0.0);
        search.grammar.continuousProbability = config_.continuousProbability;
        search.grammar.continuousLower = -config_.maxAbsConstant;
        search.grammar.continuousUpper = config_.maxAbsConstant;
        if (config_.fitConstants) {
            search.fitting = ConstantFitConfig{};
            search.fitting->lower = -config_.maxAbsConstant;
            search.fitting->upper = config_.maxAbsConstant;
            search.fitting->maxLossAttempts = config_.fitLossAttempts;
            search.fitting->lossThreshold = config_.fitThreshold;
        }

        ExecutionContext execution({config_.workers});
        RandomContext random(config_.seed, execution);
        std::atomic<bool> finished{false};
        std::atomic<bool> timedOut{false};
        std::mutex timerMutex;
        std::condition_variable timerChanged;
        std::thread timer;
        if (config_.maxTimeSeconds) {
            const auto deadline = start + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(*config_.maxTimeSeconds));
            timer = std::thread([&] {
                std::unique_lock lock(timerMutex);
                if (timerChanged.wait_until(lock, deadline, [&] { return finished.load(); })) return;
                timedOut.store(true, std::memory_order_release);
                // The core checks cancellation at candidate/generation boundaries. Reassert
                // across executor batch resets until the private work has drained.
                while (!finished.load(std::memory_order_acquire)) {
                    lock.unlock();
                    execution.requestStop();
                    lock.lock();
                    timerChanged.wait_for(lock, std::chrono::milliseconds(1),
                                          [&] { return finished.load(); });
                }
            });
        }

        SymbolicRegressionResult result;
        try {
            result = symbolicRegression(problem, search, random);
        } catch (...) {
            finished.store(true, std::memory_order_release);
            timerChanged.notify_all();
            if (timer.joinable()) timer.join();
            throw;
        }
        finished.store(true, std::memory_order_release);
        timerChanged.notify_all();
        if (timer.joinable()) timer.join();

        PythonRegressionStats next;
        next.timedOut = timedOut.load(std::memory_order_acquire);
        next.stop = result.stoppingReason;
        next.lossAttempts = result.trainingWork.lossAttempts;
        next.rowAttempts = result.trainingWork.rowAttempts;
        next.fittingInvocations = result.trainingWork.fittingInvocations;
        next.fittingLossAttempts = result.trainingWork.fittingLossAttempts;
        next.fittingProposals = result.trainingWork.proposals;
        next.fittingAccepted = result.trainingWork.accepted;
        next.invalidLosses = result.trainingWork.invalidLosses;
        next.candidateEvaluations = result.candidateEvaluations;
        next.attemptedGenerations = result.attemptedGenerations;
        next.publishedGenerations = result.publishedGenerations;
        next.rawInitialExpressions = result.rawInitialExpressions;
        next.preparedInitialExpressions = result.preparedInitialExpressions;
        for (const auto& fitness : result.preparedInitialFitness)
            next.preparedInitialLosses.push_back(fitness.state() == FitnessState::Valid ? std::optional<double>(fitness.score()) : std::nullopt);
        std::ostringstream rngText;
        rngText << random.engine();
        next.rngState = rngText.str();
        next.searchSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        if (result.best) {
            next.fitted = true;
            next.trainingMse = result.best->fitness();
            next.nodes = result.best->expression().nodeCount();
            next.depth = result.best->expression().depth();
            next.bestGeneration = result.bestGeneration.value_or(0);
            expression_ = result.best->expression();
        } else {
            expression_.reset();
        }
        stats_ = std::move(next);
    }

    bool fitted() const noexcept { return expression_.has_value(); }
    std::size_t featureCount() const { requireFitted(); return expression_->variableCount(); }
    const PythonRegressionStats& stats() const noexcept { return stats_; }
    std::string expression() const {
        requireFitted();
        return expression_->toString();
    }
    std::vector<double> predict(const std::vector<std::vector<double>>& rows) const {
        requireFitted();
        std::vector<double> predictions;
        predictions.reserve(rows.size());
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const auto value = expression_->evaluate(rows[i]);
            if (!value.valid())
                throw std::domain_error("selected expression is mathematically invalid at prediction row " +
                                        std::to_string(i));
            predictions.push_back(value.value());
        }
        return predictions;
    }
    std::string sympyExpression(const std::vector<std::string>& featureNames) const {
        requireFitted();
        if (featureNames.size() != expression_->variableCount())
            throw std::invalid_argument("feature-name count differs from model arity");
        std::vector<std::string> stack;
        stack.reserve(expression_->nodeCount());
        for (const auto& node : expression_->nodes()) {
            const auto kind = node.kind();
            if (kind == ExpressionKind::Variable) {
                const auto& name = featureNames.at(node.variableIndex());
                if (name.empty() || !(std::isalpha(static_cast<unsigned char>(name[0])) || name[0] == '_') ||
                    !std::all_of(name.begin() + 1, name.end(), [](unsigned char c) {
                        return std::isalnum(c) || c == '_';
                    }))
                    throw std::invalid_argument("SymPy export requires identifier-like feature names");
                stack.push_back(name);
            } else if (kind == ExpressionKind::Constant) {
                stack.push_back(constantText(node.constantValue()));
            } else if (kind == ExpressionKind::Powi || kind == ExpressionKind::NegativePowi) {
                auto child = pop(stack);
                const int exponent = kind == ExpressionKind::Powi
                    ? static_cast<int>(node.exponent()) : -static_cast<int>(node.exponent());
                stack.push_back("(" + child + " ** " + std::to_string(exponent) + ")");
            } else {
                const auto arity = expressionArity(kind);
                if (stack.size() < arity) throw std::logic_error("malformed validated expression stack");
                std::vector<std::string> args(arity);
                for (std::size_t i = arity; i-- > 0;) args[i] = pop(stack);
                if (arity == 2 && (kind == ExpressionKind::Add || kind == ExpressionKind::Subtract ||
                                   kind == ExpressionKind::Multiply || kind == ExpressionKind::Divide ||
                                   kind == ExpressionKind::Pow)) {
                    const char* op = kind == ExpressionKind::Add ? "+" : kind == ExpressionKind::Subtract ? "-" :
                                     kind == ExpressionKind::Multiply ? "*" : kind == ExpressionKind::Divide ? "/" : "**";
                    stack.push_back("(" + args[0] + " " + op + " " + args[1] + ")");
                } else {
                    const auto name = sympyFunction(kind);
                    std::string call = name + "(";
                    for (std::size_t i = 0; i < args.size(); ++i) {
                        if (i) call += ", ";
                        call += args[i];
                    }
                    call += ")";
                    stack.push_back(std::move(call));
                }
            }
        }
        if (stack.size() != 1) throw std::logic_error("validated expression export did not produce one root");
        return stack.back();
    }

private:
    static std::string pop(std::vector<std::string>& stack) {
        if (stack.empty()) throw std::logic_error("malformed validated expression stack");
        auto value = std::move(stack.back());
        stack.pop_back();
        return value;
    }
    static std::string constantText(double value) {
        char buffer[64];
        const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value,
                                           std::chars_format::general, 17);
        if (result.ec != std::errc{}) throw std::runtime_error("constant formatting failed");
        std::string text(buffer, result.ptr);
        if (text == "-0") text = "-0.0";
        return text;
    }
    static std::string sympyFunction(ExpressionKind kind) {
        switch (kind) {
        case ExpressionKind::Log: return "log";
        case ExpressionKind::Sin: return "sin";
        case ExpressionKind::Cos: return "cos";
        case ExpressionKind::Exp: return "exp";
        case ExpressionKind::Sqrt: return "sqrt";
        default: throw std::invalid_argument("selected operator has no qualified SymPy export mapping");
        }
    }
    void requireFitted() const {
        if (!expression_) throw std::logic_error("regressor has no committed fitted model");
    }

    PythonRegressionConfig config_;
    std::optional<Expression> expression_;
    PythonRegressionStats stats_;
};
