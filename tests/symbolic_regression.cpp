#include <SymbolicRegression.hpp>
#include <array>
#include <iostream>
#include <set>
#include <type_traits>

static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class E, class F> void throws(F action) {
    bool caught = false; try { action(); } catch (const E&) { caught = true; }
    check(caught, "expected exception");
}
using K = ExpressionKind;
using C = ExpressionCandidate;
using P = Population<C, RegressionOrdering>;
static Expression add(const Expression& a, const Expression& b) { return Expression::binary(K::Add, a, b); }
static Expression target() {
    const auto x = Expression::variable(0);
    return add(add(Expression::binary(K::Multiply, x, x), x), Expression::constant(1, 1));
}
static std::vector<RegressionRow> training() {
    std::vector<RegressionRow> rows;
    for (int k = 0; k <= 16; ++k) { const double x = -2. + k/4.; rows.push_back({{x}, x*x+x+1}); }
    return rows;
}
static void lossesAndOwnership() {
    static_assert(!std::is_copy_assignable_v<RegressionProblem>);
    auto rows = training();
    auto& retained = rows[0].inputs[0];
    const auto problem = RegressionProblem::create(std::move(rows));
    retained = 500;
    check(problem->measure(target()).loss.score() == 0, "target has zero training MSE with detached rows");
    const auto small = RegressionProblem::create({{{-1},1}, {{0},1}, {{1},3}});
    check(small->measure(Expression::variable(0)).loss.score() == 3, "independent linear MSE fixture");
    throws<std::invalid_argument>([] { RegressionProblem::create({}); });
    throws<std::invalid_argument>([] { RegressionProblem::create({{{1}, 2}, {{}, 0}}); });
    throws<std::invalid_argument>([] { RegressionProblem::create({{{std::numeric_limits<double>::infinity()}, 1}}); });
    throws<std::invalid_argument>([] { RegressionProblem::create({{{1}, std::numeric_limits<double>::quiet_NaN()}}); });
    throws<std::invalid_argument>([] { RegressionProblem::create({{{1}, 2}}, {31,6,2}, static_cast<RegressionLoss>(1)); });
    throws<std::invalid_argument>([&] { small->measure(Expression::variable(0, 2)); });
    const auto x = Expression::variable(0);
    const double huge = std::numeric_limits<double>::max();
    auto prediction = Expression::binary(K::Multiply, x, x);
    check(RegressionProblem::create({{{huge}, 0}})->measure(prediction).loss.state() == FitnessState::Invalid, "prediction overflow invalid");
    check(RegressionProblem::create({{{huge}, -huge}})->measure(x).loss.state() == FitnessState::Invalid, "residual overflow invalid");
    check(RegressionProblem::create({{{1e200}, 0}})->measure(x).loss.state() == FitnessState::Invalid, "squared residual overflow invalid");
    check(RegressionProblem::create({{{1e154}, 0}, {{1e154}, 0}})->measure(x).loss.state() == FitnessState::Invalid, "sum overflow invalid even with finite mean mathematically");
    auto partial = RegressionProblem::create({{{0}, 0}, {{huge}, 0}})->measure(prediction);
    check(partial.loss.state() == FitnessState::Invalid && partial.samplesAttempted == 2 && !partial.maxAbsoluteError,
          "no favorable partial-data score");
    C parent(x, small); parent.calculateFitness();
    C child = parent;
    child.replaceExpression(Expression::constant(1, 1));
    check(child.fitnessState() == FitnessState::Unevaluated && parent.fitness() == 3, "controlled expression edit invalidates only child");
    child.calculateFitness();
    check(child.fitness() == 4./3., "new expression evaluated under same problem");
    const auto text = child.text(); const auto fitness = child.evaluation();
    throws<std::invalid_argument>([&] { child.replaceExpression(Expression::constant(3, 1)); });
    check(child.text() == text && child.evaluation() == fitness, "failed replacement preserves genome/cache");
    throws<std::invalid_argument>([&] { child.replaceExpression(Expression::variable(0, 2)); });
    RandomContext random(8);
    P p(std::vector<C>{parent, child}, random, ObjectiveDirection::Minimize);
    const auto old = p.stats(); const auto gen = p.generation(); const auto best = p.bestSeen()->text();
    auto other = RegressionProblem::create({{{0}, 0}, {{1}, 1}});
    p.setCreature(0, C(x, other));
    throws<std::invalid_argument>([&] { p.updateValues(); });
    check(p.stats() == old && p.generation() == gen && p.bestSeen()->text() == best, "immutable run problem identity enforced");
    throws<std::invalid_argument>([&] { p.setSize(2); });
    check(p.stats() == old, "unsupported unconfigured initialization cannot damage committed state");
}
static void structureAndLimits() {
    const auto x = Expression::variable(0), one = Expression::constant(1, 1);
    const auto tree = Expression::binary(K::Subtract, add(x, one), Expression::constant(2, 1));
    const ExpressionLimits limits{31, 6, 2};
    check(replaceExpressionSubtree(tree, tree.nodeCount()-1, x, limits)->toString() == "x0", "root replacement");
    auto internal = replaceExpressionSubtree(tree, 2, x, limits);
    check(internal->toString() == "(x0 - 2.0)" && internal->evaluate(std::array{5.}).value() == 3, "internal unequal replacement preserves subtraction order");
    check(replaceExpressionSubtree(x, 0, tree, limits)->nodeCount() == 5, "terminal replacement grows structure");
    check(replaceExpressionSubtree(tree, 4, x, {1,1,2}).has_value(), "node boundary after replacing root");
    check(!replaceExpressionSubtree(tree, 0, tree, {7,6,2}), "replacement node count overflow rejected");
    const auto deep = add(add(x, one), one); // depth 3
    const auto replacement = add(x, one); // depth 2, locally within a depth-3 limit
    check(!replaceExpressionSubtree(deep, 0, replacement, {31,3,2}), "destination depth rather than just subtree depth checked");
    check(replaceExpressionSubtree(deep, 0, replacement, {31,4,2})->depth() == 4, "destination depth exact boundary");
    throws<std::out_of_range>([&] { replaceExpressionSubtree(x, 1, x, limits); });
    const auto problem = RegressionProblem::create(training());
    ExpressionVariation variation(problem);
    RandomContext random(83);
    std::set<std::size_t> sizes;
    C father(x, problem), mother(tree, problem);
    father.calculateFitness(); mother.calculateFitness();
    const auto fatherText = father.text(), motherText = mother.text();
    bool crossoverStructure = false, mutationStructure = false;
    for (int i = 0; i < 24; ++i) {
        auto initial = variation.initialize(random.engine()); sizes.insert(initial.expression().nodeCount());
        C a = father, b = mother;
        variation.crossover(father, mother, a, b, 1, random.engine());
        crossoverStructure |= a.expression().nodeCount() != father.expression().nodeCount() || b.expression().nodeCount() != mother.expression().nodeCount();
        const auto beforeMutation = a.expression().nodeCount();
        a = variation.mutation(a, 1, random.engine());
        mutationStructure |= a.expression().nodeCount() != beforeMutation;
        check(a.expression().nodeCount() <= 31 && a.expression().depth() <= 6 &&
              b.expression().nodeCount() <= 31 && b.expression().depth() <= 6, "seeded variation invariants");
        check(a.fitnessState() == FitnessState::Unevaluated, "accepted mutation invalidates fitness");
        a.calculateFitness(); b.calculateFitness();
    }
    const auto s = variation.statistics();
    check(sizes.size() > 1 && sizes.count(1), "random initialization has varied sizes and terminals");
    check(s.mutationChanges > 0 && s.crossoverChanges > 0 && mutationStructure && crossoverStructure, "both operators actually alter expression structure");
    check(father.text() == fatherText && mother.text() == motherText && father.fitnessState() == FitnessState::Valid,
          "parents remain unchanged");
    // Different depths/sizes, both prevalidated: some swaps violate a destination bound.
    // A finite seeded sample finds and verifies a bounded fallback without manufacturing an operational error.
    auto tight = RegressionProblem::create(training(), {5,3,2});
    ExpressionGrammar grammar; grammar.proposalLimit = 1;
    ExpressionVariation bounded(tight, grammar);
    C a(deep,tight), b(add(x,one),tight);
    for (int i = 0; i < 40 && bounded.statistics().crossoverFallbacks == 0; ++i) {
        C outA = a, outB = b;
        const auto before = bounded.statistics();
        bounded.crossover(a,b,outA,outB,1,random.engine());
        if (bounded.statistics().crossoverFallbacks > before.crossoverFallbacks)
            check(outA.text() == a.text() && outB.text() == b.text(), "bounded rejection fallback preserves parents");
    }
    check(bounded.statistics().crossoverFallbacks > 0 && bounded.statistics().structuralRejections > 0, "structural rejection and fallback observable");
}
struct Fault { std::size_t calls = 0, throwOn = 0; bool variation = false; };
struct Injected : std::runtime_error { using std::runtime_error::runtime_error; };
class FaultCandidate : public C {
public:
    FaultCandidate(const Expression& e, std::shared_ptr<const RegressionProblem> p, std::shared_ptr<Fault> f)
        : C(e, std::move(p)), fault(std::move(f)) {}
    std::shared_ptr<Fault> fault;
protected:
    Fitness evaluate() const override {
        ++fault->calls;
        const auto result = C::evaluate(); // attempted sample work precedes injected operational failure
        if (fault->calls == fault->throwOn) throw Injected("chosen regression evaluation failure");
        return result;
    }
};
struct FaultVariation {
    std::shared_ptr<Fault> fault;
    bool invalidateAll = false;
    template<class T> void crossover(const T& father,const T& mother,T& a,T& b,float,std::mt19937& rng) {
        (void)rng(); a=father; b=mother;
    }
    template<class T> T mutation(const T& parent,float,std::mt19937& rng) {
        (void)rng();
        if (fault && fault->variation) throw Injected("chosen structural variation failure");
        T child = parent;
        if (invalidateAll) {
            const auto x=Expression::variable(0);
            child.replaceExpression(Expression::binary(K::Multiply,x,x));
        }
        return child;
    }
};
static bool hasCause(const std::exception& error, const char* message) {
    if (std::string(error.what()).find(message) != std::string::npos) return true;
    try { std::rethrow_if_nested(error); }
    catch(const std::exception& cause) { return hasCause(cause,message); }
    catch(...) {}
    return false;
}
static void failuresAndOrdering() {
    auto problem = RegressionProblem::create(training());
    auto control=std::make_shared<Fault>();
    using FP=Population<FaultCandidate,RegressionOrdering>;
    using FG=GeneticAlgorithm<FP,FaultCandidate,FaultVariation>;
    RandomContext random(42);
    const auto x=Expression::variable(0);
    FP p(std::vector<FaultCandidate>{FaultCandidate(x,problem,control), FaultCandidate(Expression::constant(1,1),problem,control)},
         random,ObjectiveDirection::Minimize);
    FG ga(1,1,random,FaultVariation{control});
    const auto before=p.creatures(); const auto best=p.bestSeen()->text(); const auto gen=p.generation();
    auto checkPreserved=[&] {
        const auto now=p.creatures();
        for(std::size_t i=0;i<now->size();++i) {
            float as,ae,bs,be; (*now)[i].rouletteRange(as,ae); (*before)[i].rouletteRange(bs,be);
            check((*now)[i].text()==(*before)[i].text() && (*now)[i].evaluation()==(*before)[i].evaluation() && as==bs && ae==be,
                  "failed attempt preserves committed expression/cache/ranking");
        }
        check(p.bestSeen()->text()==best && p.generation()==gen && ga.evolutions()==0, "failure preserves generation/archive");
    };
    control->variation=true;
    const auto oldEngine=random.engine(); const auto work=random.evaluationAttempts();
    try { ga.evolve(p); check(false,"variation must throw"); }
    catch(const std::exception& e) { check(hasCause(e,"chosen structural variation failure"),"variation cause retained"); }
    checkPreserved();
    check(random.engine()!=oldEngine && random.evaluationAttempts()==work && ga.attempts()==1,"failed variation consumes draws but no evaluator work");
    control->variation=false; control->throwOn=control->calls+2;
    try { ga.evolve(p); check(false,"evaluation must throw"); }
    catch(const std::exception& e) { check(hasCause(e,"chosen regression evaluation failure") && hasCause(e,"candidate 1"),"partial generation error context retained"); }
    checkPreserved();
    check(random.evaluationAttempts()==work+2 && ga.attempts()==2,"throwing attempted fitness calls remain counted");
    // Exact same MSE: x and x+0. Smaller wins in both ranking and the archive.
    auto equality=RegressionProblem::create({{{0},1}});
    P ties(std::vector<C>{C(add(x,Expression::constant(0,1)),equality),C(x,equality)},random,ObjectiveDirection::Minimize);
    check(ties.bestCreature().expression().nodeCount()==1,"MSE tie prefers smaller expression");
    check(ties.creatures()->back().fitnessPercent()>66, "selection weights use the same complexity ordering");
    throws<std::invalid_argument>([&] { P wrong(std::vector<C>{C(x,equality)},random,ObjectiveDirection::Maximize); });
    ties.publishGeneration({C(Expression::constant(0,1),equality)});
    check(ties.bestSeen()->text()=="0.0" && ties.bestGeneration()==1,"deterministic text tie used in archive too");
    // Fixed finite data whose x*x overflows; ordinary invalidity does not throw or regenerate.
    auto invalidProblem=RegressionProblem::create({{{1e200},0}});
    P valid(std::vector<C>{C(Expression::constant(0,1),invalidProblem),C(Expression::constant(1,1),invalidProblem)},random,ObjectiveDirection::Minimize);
    GeneticAlgorithm<P,C,FaultVariation> invalidGA(1,1,random,FaultVariation{{},true});
    const auto saved=valid.stats(); const auto savedBest=valid.bestSeen()->text(); const auto savedGeneration=valid.generation();
    const auto calls=random.evaluationAttempts();
    check(invalidGA.evolve(valid)==GenerationOutcome::NoViableCandidates,"all-invalid successor explicit outcome");
    check(valid.stats()==saved && valid.bestSeen()->text()==savedBest && valid.generation()==savedGeneration &&
          random.evaluationAttempts()==calls+2 && invalidGA.evolutions()==0,"all-invalid successor preserved with charged work");
    GeneticAlgorithm<P,C,FaultVariation> elites(1,1,random,FaultVariation{{},true},1);
    check(elites.evolve(valid)==GenerationOutcome::Committed && valid.bestCreature().text()==savedBest &&
          valid.bestSeen()->text()==savedBest && valid.bestGeneration()==0,
          "ranked elite survives with committed archive discovery retained");
    P allInvalid(std::vector<C>{C(Expression::binary(K::Multiply,x,x),invalidProblem)},random,ObjectiveDirection::Minimize);
    check(!allInvalid.bestSeen() && !allInvalid.generation(),"all-invalid initialization has no committed best");
}
static void budgetsAndRepeatability() {
    const auto problem=RegressionProblem::create({{{0},.125},{{1},.375}});
    SymbolicRegressionConfig config; config.population=6; config.elites=1; config.maxGenerationAttempts=3; config.maxEvaluations=11;
    RandomContext budget(11);
    const auto stopped=symbolicRegression(problem,config,budget);
    check(stopped.stoppingReason==SearchStop::EvaluationBudget && stopped.candidateEvaluations==6 &&
          stopped.publishedGenerations==0 && stopped.attemptedGenerations==0,"budget stops before incomplete generation");
    config.maxEvaluations=24;
    RandomContext a(19),b(19);
    const auto first=symbolicRegression(problem,config,a),second=symbolicRegression(problem,config,b);
    check(first.best->text()==second.best->text() && first.best->evaluation()==second.best->evaluation() &&
          first.bestGeneration==second.bestGeneration && first.initialBestMSE==second.initialBestMSE &&
          first.attemptedGenerations==second.attemptedGenerations && first.publishedGenerations==second.publishedGenerations &&
          first.candidateEvaluations==second.candidateEvaluations && first.stoppingReason==second.stoppingReason && first.variation==second.variation && a.engine()==b.engine(),
          "independent serial searches repeat including counters and RNG state");
    check(first.attemptedGenerations==3 && first.publishedGenerations==3 && first.candidateEvaluations==24, "short integer-terminal search actually completes its configured generations");
    check(first.candidateEvaluations==6*(1+first.attemptedGenerations) && first.candidateEvaluations<=24,"one evaluation per slot including elites");
    const auto exactProblem=RegressionProblem::create({{{},1}});
    SymbolicRegressionConfig exactConfig; exactConfig.population=4; exactConfig.elites=1; exactConfig.maxEvaluations=4;
    exactConfig.grammar.operators.clear(); exactConfig.grammar.constants={1};
    RandomContext exact(123);
    const auto initialFit=symbolicRegression(exactProblem,exactConfig,exact);
    check(initialFit.bestGeneration==0 && initialFit.attemptedGenerations==0 && initialFit.candidateEvaluations==4 &&
          initialFit.stoppingReason==SearchStop::ExactTrainingFit,"natural initialization fit reported as generation zero");
    ExpressionGrammar variableOnly; variableOnly.operators.clear(); variableOnly.constants.clear();
    ExpressionVariation variables(problem,variableOnly);
    check(variables.initialize(exact.engine()).text()=="x0", "variable-only configurable grammar");
    auto bad=config; bad.grammar.proposalLimit=0; RandomContext invalidConfig(0);
    const auto unchangedEngine=invalidConfig.engine();
    throws<std::invalid_argument>([&] { symbolicRegression(problem,bad,invalidConfig); });
    check(invalidConfig.engine()==unchangedEngine && invalidConfig.evaluationAttempts()==0,"bad configuration rejected before work");
    config.maxEvaluations=5; RandomContext insufficient(9);
    const auto noInit=symbolicRegression(problem,config,insufficient);
    check(!noInit.best && noInit.candidateEvaluations==0 && insufficient.evaluationAttempts()==0,"initial budget boundary has no partially evaluated best");
    config.maxEvaluations=24;
    auto invalidData=RegressionProblem::create({{{1},std::numeric_limits<double>::max()}});
    RandomContext invalid(22);
    const auto noValid=symbolicRegression(invalidData,config,invalid);
    check(noValid.stoppingReason==SearchStop::NoViableCandidates && !noValid.best && noValid.candidateEvaluations==6,"all-invalid search initialization terminates");
}
int main() {
    try { lossesAndOwnership(); structureAndLimits(); failuresAndOrdering(); budgetsAndRepeatability();
        std::cout<<"Symbolic regression integration/loss/variation/publication/budget contracts passed\n";
    } catch(const std::exception& e) { std::cerr<<"FAILED: "<<e.what()<<'\n'; return 1; }
}
