#pragma once
#include "DimensionalRegression.hpp"
#include "MathematicalConstraints.hpp"
#include <variant>

// Owned problem factory: the identity is derived from the exact data actually
// measured, rather than an arbitrary user-supplied cache key.
class ScalarMathematicsProblem {
  public:
    static ScalarMathematicsProblem scientific(const std::vector<RegressionRow> &rows,
                                               ExpressionLimits limits = {}) {
        return {rows, limits, {}, {}, {}, false};
    }
    static ScalarMathematicsProblem dimensional(const std::vector<RegressionRow> &rows,
                                                const std::vector<PhysicalDimension> &variables,
                                                PhysicalDimension output, ExpressionLimits limits = {},
                                                DimensionalGrammarLimits grammar = {}) {
        return {rows, limits, variables, output, grammar, true};
    }
    static ScalarMathematicsProblem constrained(const std::vector<RegressionRow> &rows,
                                                std::shared_ptr<const MathematicalConstraints> constraints,
                                                ExpressionLimits limits = {}) {
        if (!constraints)
            throw std::invalid_argument("constraint context required");
        auto result = scientific(rows, limits);
        result.problem_ = std::make_shared<const ConstrainedRegressionProblem>(rows, constraints, limits);
        CheckpointWriter w;
        w.text(result.identity_);
        w.text(constraints->identity());
        result.identity_ = w.data();
        return result;
    }
    const auto &problem() const noexcept { return problem_; }
    const std::string &identity() const noexcept { return identity_; }
    bool hasDimensions() const noexcept { return dimensional_; }
    const auto &selection() const noexcept { return selection_; }
    DimensionalGrammarLimits grammarLimits() const noexcept { return grammar_; }

  private:
    ScalarMathematicsProblem(const std::vector<RegressionRow> &rows, ExpressionLimits limits,
                             const std::vector<PhysicalDimension> &variables, PhysicalDimension output,
                             DimensionalGrammarLimits grammar, bool dimensional)
        : dimensional_(dimensional), grammar_(grammar) {
        if (dimensional) {
            auto p = std::make_shared<const DimensionalRegressionProblem>(rows, variables, output, limits);
            problem_ = p;
            selection_ = std::make_shared<DimensionalConstantSelection>(variables, p->inferenceLimits());
        } else
            problem_ = RegressionProblem::create(rows, limits);
        CheckpointWriter w;
        w.text("scalar-mathematical-problem;native-binary64;strict/1");
        w.integer(dimensional);
        w.integer(rows.size());
        for (const auto &row : rows) {
            w.integer(row.inputs.size());
            for (double input : row.inputs)
                w.real(input);
            w.real(row.target);
        }
        w.integer(limits.maxNodes);
        w.integer(limits.maxDepth);
        w.real(limits.maxAbsConstant);
        w.integer(limits.maxArithmeticOperations);
        if (dimensional) {
            w.text(DimensionalVariation::productionPolicy);
            const auto inference =
                std::dynamic_pointer_cast<const DimensionalRegressionProblem>(problem_)->inferenceLimits();
            for (auto n : {inference.maxNodes, inference.maxWorkspaceBytes, inference.maxInferenceSteps})
                w.integer(n);
            w.integer(variables.size());
            for (auto unit : variables)
                DimensionalExpression::writeDimension(w, unit);
            DimensionalExpression::writeDimension(w, output);
            for (auto n : {grammar.maxUnitStates, grammar.maxPlanningSteps, grammar.maxGenerationSteps,
                           grammar.maxWorkspaceBytes})
                w.integer(n);
        }
        identity_ = w.data();
    }
    std::shared_ptr<const RegressionProblem> problem_;
    std::shared_ptr<const ConstantParameterSelection> selection_;
    std::string identity_;
    bool dimensional_;
    DimensionalGrammarLimits grammar_;
};
class ScalarMathematicsVariation {
  public:
    ScalarMathematicsVariation(const ScalarMathematicsProblem &p, ExpressionGrammar grammar,
                               std::shared_ptr<RegressionPreparation> preparation)
        : value_(make(p, std::move(grammar), std::move(preparation))) {}
    ExpressionCandidate initialize(std::mt19937 &rng) {
        return std::visit([&](auto &v) { return v.initialize(rng); }, value_);
    }
    ExpressionCandidate mutation(const ExpressionCandidate &p, float rate, std::mt19937 &rng) {
        return std::visit([&](auto &v) { return v.mutation(p, rate, rng); }, value_);
    }
    void crossover(const ExpressionCandidate &p, const ExpressionCandidate &q, ExpressionCandidate &a,
                   ExpressionCandidate &b, float rate, std::mt19937 &rng) {
        std::visit([&](auto &v) { v.crossover(p, q, a, b, rate, rng); }, value_);
    }
    void markElite(ExpressionCandidate &c) const {
        std::visit([&](const auto &v) { v.markElite(c); }, value_);
    }
    void markOffspring(ExpressionCandidate &c, const ExpressionCandidate &p) const {
        std::visit([&](const auto &v) { v.markOffspring(c, p); }, value_);
    }
    VariationStatistics statistics() const {
        return std::visit([](const auto &v) { return v.statistics(); }, value_);
    }
    void restoreStatistics(const VariationStatistics &s) {
        std::visit([&](auto &v) { v.restoreStatistics(s); }, value_);
    }

  private:
    using Value = std::variant<ExpressionVariation, DimensionalVariation>;
    static Value make(const ScalarMathematicsProblem &p, ExpressionGrammar g,
                      std::shared_ptr<RegressionPreparation> preparation) {
        if (p.hasDimensions())
            return Value(std::in_place_type<DimensionalVariation>,
                         std::dynamic_pointer_cast<const DimensionalRegressionProblem>(p.problem()),
                         std::move(g), std::move(preparation), p.grammarLimits());
        return Value(std::in_place_type<ExpressionVariation>, p.problem(), std::move(g),
                     std::move(preparation));
    }
    Value value_;
};
class ScalarMathematicsRun {
  public:
    using PopulationType = Population<ExpressionCandidate, RegressionOrdering>;
    using Algorithm = GeneticAlgorithm<PopulationType, ExpressionCandidate, ScalarMathematicsVariation>;
    ScalarMathematicsRun(const ScalarMathematicsProblem &p, const SymbolicRegressionConfig &config,
                         RandomContext &random)
        : ScalarMathematicsRun(p, config, random, RestoreTag{}) {
        if (config.maxTrainingLossAttempts < config.population || config.maxEvaluations < config.population)
            throw TrainingLossBudgetExceeded();
        std::vector<ExpressionCandidate> initial;
        initial.reserve(config.population);
        for (std::size_t i = 0; i < config.population; ++i)
            initial.push_back(algorithm_->variation().initialize(random.engine()));
        population_ = std::make_unique<PopulationType>(initial, random, ObjectiveDirection::Minimize);
        preparation_->initializing = false;
    }
    const PopulationType &population() const { return *population_; }
    const TrainingWork &work() const noexcept { return objective_->work(); }
    const Algorithm &algorithm() const noexcept { return *algorithm_; }
    PreparationStatistics preparationWork() const noexcept { return preparation_->statistics(); }
    std::optional<SearchStop> stoppingReason() const {
        auto best = population().bestSeen();
        if (!best)
            return SearchStop::NoViableCandidates;
        if (best->fitness() == 0)
            return SearchStop::ExactTrainingFit;
        if (best->fitness() <= config_.trainingLossThreshold)
            return SearchStop::TrainingLossThreshold;
        if (algorithm_->attempts() && algorithm_->lastOutcome() == GenerationOutcome::NoViableCandidates)
            return SearchStop::NoViableCandidates;
        if (random_.execution().stopRequested())
            return SearchStop::Cancelled;
        if (algorithm_->attempts() >= config_.maxGenerationAttempts)
            return SearchStop::GenerationLimit;
        if (random_.evaluationAttempts() >= config_.maxEvaluations ||
            config_.population > config_.maxEvaluations - random_.evaluationAttempts() ||
            objective_->remaining() < config_.population)
            return SearchStop::EvaluationBudget;
        return {};
    }
    GenerationOutcome step() {
        if (stoppingReason())
            throw std::logic_error("scalar mathematical run stopped");
        return algorithm_->evolve(*population_);
    }
    CheckpointDocument checkpoint() const {
        auto state = RunCheckpointAccess::population(population());
        auto random = RunCheckpointAccess::random(random_);
        auto algorithm = RunCheckpointAccess::algorithm(*algorithm_);
        CheckpointWriter w;
        w.text(CandidateCheckpointCodec<ExpressionCandidate>::identifier);
        w.integer(random.seed);
        w.integer(random.candidateAttempts);
        w.text(random.engine);
        w.integer(random.threads);
        w.integer(algorithm.attempts);
        w.integer(algorithm.published);
        w.integer(static_cast<unsigned>(algorithm.lastOutcome));
        writeTrainingWork(w, work());
        writePreparationStatistics(w, preparation_->statistics());
        w.integer(preparation_->rawInitialBest.has_value());
        if (preparation_->rawInitialBest)
            w.real(*preparation_->rawInitialBest);
        writeVariationStatistics(w, algorithm_->variation().statistics());
        writeCommittedPopulation(w, state);
        CheckpointDocument d;
        d.codec = "genetik.scalar-mathematics-evolution/1";
        d.identity = identity_;
        d.payload = w.data();
        d.generation = state.generation;
        return d;
    }
    void save(const std::filesystem::path &path) const { saveCheckpointAtomic(path, checkpoint()); }
    static std::unique_ptr<ScalarMathematicsRun> restore(const CheckpointDocument &d,
                                                         const ScalarMathematicsProblem &p,
                                                         const SymbolicRegressionConfig &c,
                                                         RandomContext &random) {
        auto result =
            std::unique_ptr<ScalarMathematicsRun>(new ScalarMathematicsRun(p, c, random, RestoreTag{}));
        if (d.codec != "genetik.scalar-mathematics-evolution/1" || d.runtime != checkpointRuntime() ||
            d.identity != result->identity_)
            throw CheckpointError("scalar mathematics run identity/runtime differs");
        CheckpointReader r(d.payload);
        if (r.text() != CandidateCheckpointCodec<ExpressionCandidate>::identifier)
            throw CheckpointError("scalar mathematical candidate codec");
        RandomCheckpoint rng;
        rng.seed = static_cast<std::uint32_t>(r.count(UINT32_MAX));
        rng.candidateAttempts = r.integer();
        rng.engine = r.text(16000);
        rng.threads = static_cast<unsigned>(r.count(256));
        AlgorithmCheckpoint a;
        a.attempts = r.integer();
        a.published = static_cast<unsigned>(r.count(UINT32_MAX));
        a.lastOutcome = static_cast<GenerationOutcome>(r.count(2));
        auto work = readTrainingWork(r);
        auto preparation = readPreparationStatistics(r);
        std::optional<double> initial;
        if (r.count(1))
            initial = r.real();
        auto variation = readVariationStatistics(r);
        auto state = readCommittedPopulation<ExpressionCandidate>(
            r, ScalarCheckpointContext{p.problem(), result->preparation_});
        r.finish();
        if (!d.generation || *d.generation != state.generation || state.generation != a.published ||
            a.attempts > c.maxGenerationAttempts || a.published > a.attempts ||
            state.members.size() != c.population || state.direction != ObjectiveDirection::Minimize ||
            rng.candidateAttempts > c.maxEvaluations || variation.initializations != c.population ||
            rng.candidateAttempts < c.population)
            throw CheckpointError("inconsistent scalar mathematical counters");
        if (a.attempts == UINT64_MAX || rng.candidateAttempts / (a.attempts + 1) > c.population ||
            (rng.candidateAttempts / (a.attempts + 1) == c.population &&
             rng.candidateAttempts % (a.attempts + 1)))
            throw CheckpointError("scalar candidate request accounting");
        if (p.hasDimensions()) {
            auto dimensional = std::dynamic_pointer_cast<const DimensionalRegressionProblem>(p.problem());
            for (const auto &member : state.members)
                if (!dimensional->accepts(member.expression()))
                    throw CheckpointError("stored dimensional candidate units");
            if (!dimensional->accepts(state.best.expression()))
                throw CheckpointError("stored dimensional best units");
        }
        auto population =
            RunCheckpointAccess::restorePopulation<ExpressionCandidate, RegressionOrdering>(state, random);
        result->objective_->restoreWork(work);
        result->preparation_->restoreStatistics(preparation, initial);
        result->preparation_->initializing = false;
        RunCheckpointAccess::restoreAlgorithm(*result->algorithm_, a);
        result->algorithm_->variation().restoreStatistics(variation);
        RunCheckpointAccess::restoreRandom(random, rng);
        result->population_ = std::move(population);
        return result;
    }

  private:
    struct RestoreTag {};
    ScalarMathematicsRun(const ScalarMathematicsProblem &problem, const SymbolicRegressionConfig &c,
                         RandomContext &random, RestoreTag)
        : problem_(problem), config_(c), random_(random) {
        if (random.evaluationAttempts() != 0 || c.population < 2 || c.population > 100000 ||
            c.elites >= c.population || !c.fitEveryGenerations || !std::isfinite(c.trainingLossThreshold) ||
            c.trainingLossThreshold < 0 || c.diagnostics || !std::isfinite(c.crossoverRate) ||
            !std::isfinite(c.mutationRate) || c.crossoverRate < 0 || c.crossoverRate > 1 ||
            c.mutationRate < 0 || c.mutationRate > 1)
            throw std::invalid_argument("fresh bounded scalar mathematics run/configuration required");
        objective_ = std::make_shared<TrainingObjective>(problem.problem(), c.maxTrainingLossAttempts);
        preparation_ = std::make_shared<ConstantRefinement>(objective_, c.fitting, c.fitEveryGenerations,
                                                            problem.selection());
        ScalarMathematicsVariation variation(problem, c.grammar, preparation_);
        CheckpointWriter w;
        w.text(problem.identity());
        w.integer(random.execution().requested());
        w.integer(c.population);
        w.integer(c.maxGenerationAttempts);
        w.integer(c.maxEvaluations);
        w.integer(c.maxTrainingLossAttempts);
        w.integer(c.elites);
        w.integer(std::bit_cast<std::uint32_t>(c.crossoverRate));
        w.integer(std::bit_cast<std::uint32_t>(c.mutationRate));
        w.real(c.trainingLossThreshold);
        w.integer(c.fitEveryGenerations);
        const auto &g = c.grammar;
        w.integer(g.operators.size());
        for (auto k : g.operators)
            w.integer(static_cast<unsigned>(k));
        for (const auto *v : {&g.constants, &g.realExponents}) {
            w.integer(v->size());
            for (double x : *v)
                w.real(x);
        }
        for (const auto *v : {&g.exponents, &g.negativeExponents}) {
            w.integer(v->size());
            for (auto x : *v)
                w.integer(x);
        }
        w.integer(g.powerExponentsAreConstants);
        for (auto n : {g.initialMaxDepth, g.mutationMaxDepth, g.proposalLimit})
            w.integer(n);
        for (double n :
             {g.growBinaryProbability, g.continuousProbability, g.continuousLower, g.continuousUpper})
            w.real(n);
        w.integer(c.fitting.has_value());
        if (c.fitting) {
            const auto &f = *c.fitting;
            w.integer(f.maxLossAttempts);
            w.integer(f.maxSweeps);
            w.integer(f.maxProposals);
            w.integer(f.lossThreshold.has_value());
            if (f.lossThreshold)
                w.real(*f.lossThreshold);
            w.real(f.initialStep);
            w.real(f.minimumStep);

            w.real(f.lower);
            w.real(f.upper);
        }
        identity_ = w.data();
        algorithm_ = std::make_unique<Algorithm>(c.crossoverRate, c.mutationRate, random,
                                                 std::move(variation), c.elites);
    }
    ScalarMathematicsProblem problem_;
    SymbolicRegressionConfig config_;
    RandomContext &random_;
    std::shared_ptr<TrainingObjective> objective_;
    std::shared_ptr<ConstantRefinement> preparation_;
    std::unique_ptr<PopulationType> population_;
    std::unique_ptr<Algorithm> algorithm_;
    std::string identity_;
};
