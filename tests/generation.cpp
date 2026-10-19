#include "Creature.hpp"
#include "GeneticAlgorithm.hpp"
#include "Population.hpp"
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>

static int checks = 0;
static void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
template<class E, class F> void throws(F f) {
    bool caught = false;
    try { f(); } catch (const E&) { caught = true; }
    check(caught, "expected exception");
}
struct InjectedError : std::runtime_error { using std::runtime_error::runtime_error; };
// External diagnostics/fault injection only. The objective remains gene(0), except sentinel -999.
struct Control {
    std::size_t calls = 0, throwOn = 0, unevaluatedOn = 0;
    bool nonstandard = false;
    double mutationValue = 2;
    int copyFaultCountdown = -1;
    std::size_t copyFaultAfterCall = 0;
};
class Candidate : public Creature_i<double> {
public:
    explicit Candidate(std::mt19937& rng) : Creature_i<double>({std::uniform_real_distribution<double>(0, 1)(rng)}) {}
    Candidate(double value, std::shared_ptr<Control> control = {}) : Creature_i<double>({value}), control_(control) {}
    Candidate(const Candidate& other) : Creature_i<double>(other), control_(other.control_) {
        if (control_ && control_->copyFaultCountdown >= 0 && control_->copyFaultCountdown-- == 0)
            throw InjectedError("copy failure");
    }
    Candidate(Candidate&&) noexcept = default;
    Candidate& operator=(const Candidate&) = default;
    Candidate& operator=(Candidate&&) noexcept = default;
    std::string stats() override { return std::to_string(gene(0)); }
protected:
    Fitness evaluate() const override {
        if (control_) {
            ++control_->calls;
            if (control_->calls == control_->throwOn) {
                if (control_->nonstandard) throw 71;
                throw InjectedError("chosen evaluator failure");
            }
            if (control_->calls == control_->unevaluatedOn) return Fitness{};
            if (control_->calls == control_->copyFaultAfterCall) control_->copyFaultCountdown = 0;
        }
        return gene(0) == -999 ? Fitness::invalid() : Fitness::fromScore(gene(0));
    }
    double mutatedGene(unsigned int, std::mt19937& rng) const override {
        const double draw = std::uniform_real_distribution<double>(0, 1)(rng);
        return control_ ? control_->mutationValue : draw;
    }
private:
    std::shared_ptr<Control> control_;
};
using P = Population<Candidate>;
using GA = GeneticAlgorithm<P, Candidate>;

static bool same(const Candidate& a, const Candidate& b) {
    float as, ae, bs, be;
    a.rouletteRange(as, ae); b.rouletteRange(bs, be);
    return a.chromosome() == b.chromosome() && a.evaluation() == b.evaluation() &&
        a.fitnessPercent() == b.fitnessPercent() && as == bs && ae == be;
}
struct Snapshot {
    std::shared_ptr<std::vector<Candidate>> members;
    std::optional<Candidate> best;
    std::optional<std::uint64_t> generation;
};
static Snapshot snapshot(const P& p) { return {p.creatures(), p.bestSeen(), p.generation()}; }
static void unchanged(const P& p, const Snapshot& before) {
    const auto now = p.creatures();
    check(now->size() == before.members->size(), "preserve size");
    for (std::size_t i = 0; i < now->size(); ++i)
        check(same(now->at(i), before.members->at(i)), "preserve genome, cache and selection metadata");
    check(p.generation() == before.generation, "preserve committed generation");
    const auto best = p.bestSeen();
    check(best.has_value() == before.best.has_value(), "preserve best presence");
    if (best) check(same(*best, *before.best), "preserve best committed candidate");
}
static bool containsError(const std::exception& error, const std::string& text) {
    if (std::string(error.what()).find(text) != std::string::npos) return true;
    try { std::rethrow_if_nested(error); }
    catch (const std::exception& nested) { return containsError(nested, text); }
    catch (...) {}
    return false;
}

static void statesAndMutation() {
    Candidate c(0);
    check(c.fitnessState() == FitnessState::Unevaluated, "initial unevaluated differs from zero");
    throws<std::logic_error>([&] { c.fitness(); });
    c.calculateFitness();
    check(c.fitnessState() == FitnessState::Valid && c.fitness() == 0, "valid zero");
    c.setGene(0, -3);
    check(c.fitnessState() == FitnessState::Unevaluated, "setGene invalidates");
    c.calculateFitness();
    check(c.fitness() == -3, "valid negative");
    auto copy = c;
    RandomContext random(31);
    copy.mutateGene(0, random.engine());
    check(copy.fitnessState() == FitnessState::Unevaluated && c.fitness() == -3, "mutation independent cache");
    copy = c;
    check(copy.evaluation() == c.evaluation(), "assignment copies matching genome/cache");
    Candidate moved(std::move(copy));
    check(moved.fitness() == -3 && copy.fitnessState() == FitnessState::Unevaluated, "move clears source cache");
    copy = std::move(moved);
    check(copy.fitness() == -3 && moved.fitnessState() == FitnessState::Unevaluated, "move assignment clears source cache");
    auto genes = c.chromosome();
    auto& retained = genes[0];
    c.replaceChromosome(std::move(genes));
    check(c.fitnessState() == FitnessState::Unevaluated, "whole genome edit invalidates");
    c.calculateFitness();
    retained = 8;
    check(c.gene(0) == -3 && c.fitness() == -3, "retained input reference cannot mutate private genome");
    std::vector<char> input(74, '?');
    char& inputRef = input[0];
    Creature<char> charC(std::move(input));
    charC.calculateFitness();
    inputRef = 'A';
    check(charC.gene(0) == '?', "constructor does not adopt externally aliased buffer");
    auto detached = c.chromosome(); detached[0] = 99;
    check(c.gene(0) == -3, "no mutable chromosome view");
    throws<std::out_of_range>([&] { c.setGene(5, 2); });
    throws<std::out_of_range>([&] { c.mutateGene(5, random.engine()); });
    check(c.fitness() == -3, "rejected edits leave current cache intact");
    c.setGene(0, -999); c.calculateFitness();
    check(c.fitnessState() == FitnessState::Invalid, "explicit mathematical invalidity");
    throws<std::logic_error>([&] { c.fitness(); });
    for (double x : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                     -std::numeric_limits<double>::infinity()}) {
        c.setGene(0, x); c.calculateFitness();
        check(c.fitnessState() == FitnessState::Invalid, "numerical adapter rejects nonfinite");
        throws<std::invalid_argument>([&] { Fitness::valid(x); });
    }
    c.setGene(0, 0);
    check(c.fitnessState() == FitnessState::Unevaluated, "edit invalid candidate makes it unevaluated");
}

static void mixedAndDegenerate() {
    RandomContext random(52);
    P p(std::vector<Candidate>{Candidate(-999), Candidate(-4), Candidate(0)}, random);
    check(p.lastOutcome() == GenerationOutcome::Committed && p.generation() == 0, "initial generation commits");
    check(p.bestCreature().fitness() == 0 && p.mean() == -2, "invalid excluded from best and mean");
    auto members = p.creatures();
    check(members->front().fitnessPercent() == 0, "invalid has zero roulette weight");
    GA ga(0, 0, random);
    for (int i = 0; i < 100; ++i) check(ga.roulette(p).fitnessState() == FitnessState::Valid, "exclude invalid parents");
    P minP(std::vector<Candidate>{Candidate(-999), Candidate(-4), Candidate(0)}, random, ObjectiveDirection::Minimize);
    check(minP.bestCreature().fitness() == -4, "direction independent of validity");
    P extremes(std::vector<Candidate>{Candidate(-std::numeric_limits<double>::max()),
        Candidate(std::numeric_limits<double>::max())}, random);
    check(extremes.bestCreature().fitness() == std::numeric_limits<double>::max(), "finite score extremes rank without subtraction overflow");
    P equal(std::vector<Candidate>{Candidate(-2), Candidate(-2)}, random);
    check(equal.creatures()->front().fitnessPercent() == 50, "equal negative weights");
    P single(std::vector<Candidate>{Candidate(-999), Candidate(0), Candidate(-999)}, random);
    GA sg(0, 0, random);
    for (int i = 0; i < 20; ++i) check(sg.roulette(single).fitness() == 0, "one viable parent");
    check(sg.evolve(single) == GenerationOutcome::Committed && single.creatures()->size() == 3, "one viable parent fills generation");
    const auto singletonMembers = single.creatures();
    for (const auto& c : *singletonMembers) check(c.fitness() == 0, "singleton parent copies");
    // Public population inputs must detach candidate objects as well as their gene buffers.
    std::vector<Candidate> supplied{Candidate(1), Candidate(2)};
    auto& retainedCandidate = supplied[0];
    P detached(std::move(supplied), random);
    const auto detachedBefore = snapshot(detached);
    retainedCandidate.setGene(0, 100);
    unchanged(detached, detachedBefore);
    std::vector<Candidate> successor{Candidate(3), Candidate(4)};
    auto& retainedSuccessor = successor[0];
    detached.publishGeneration(std::move(successor));
    const auto publishedBefore = snapshot(detached);
    retainedSuccessor.invalidateFitness();
    retainedSuccessor.setGene(0, 200);
    unchanged(detached, publishedBefore);
    // Snapshot modifications cannot alter committed state, even through base mutators.
    auto snap = snapshot(p);
    members->back().setGene(0, 100);
    unchanged(p, snap);
    p.setCreature(0, Candidate(100));
    throws<std::logic_error>([&] { p.bestCreature(); });
    throws<std::logic_error>([&] { p.mean(); });
    throws<std::logic_error>([&] { ga.roulette(p); });
    throws<std::runtime_error>([&] { ga.evolve(p); });
    unchanged(p, snap);
    check(p.updateValues() == GenerationOutcome::Committed && p.bestCreature().fitness() == 100, "explicit refresh publishes edits");
    const auto calls = random.evaluationAttempts();
    P invalid(std::vector<Candidate>{Candidate(-999), Candidate(-999)}, random);
    check(random.evaluationAttempts() == calls + 2, "all-invalid initialization attempts counted");
    check(!invalid.generation() && !invalid.bestSeen() && invalid.creatures()->empty(), "all-invalid initial state has no committed best");
    throws<std::logic_error>([&] { invalid.bestCreature(); });
    GA ig(1, 1, random);
    const auto engine = random.engine();
    const auto work = random.evaluationAttempts();
    check(ig.evolve(invalid) == GenerationOutcome::NoViableCandidates, "absent initial state terminates");
    check(random.engine() == engine && random.evaluationAttempts() == work, "no implicit retry or draws");
    check(invalid.setSize(1) == GenerationOutcome::Committed && invalid.generation() == 0, "explicit reinitialization allowed");
}

static void evaluationFailure() {
    // Mirrored successful run establishes the exact consumed-RNG state after offspring construction.
    RandomContext r1(808), r2(808);
    auto control = std::make_shared<Control>(), referenceControl = std::make_shared<Control>();
    P p(std::vector<Candidate>{Candidate(1, control), Candidate(0, control), Candidate(-999, control)}, r1);
    P reference(std::vector<Candidate>{Candidate(1, referenceControl), Candidate(0, referenceControl), Candidate(-999, referenceControl)}, r2);
    GA ga(1, 1, r1), rg(1, 1, r2);
    check(ga.evolve(p) == GenerationOutcome::Committed && rg.evolve(reference) == GenerationOutcome::Committed, "setup committed generation");
    auto before = snapshot(p);
    const auto attempts = r1.evaluationAttempts();
    const auto engine = r1.engine();
    control->mutationValue = referenceControl->mutationValue = 100;
    control->throwOn = control->calls + 2; // first offspring improves objective, second throws
    bool failed = false;
    try { ga.evolve(p); }
    catch (const std::exception& error) {
        failed = true;
        check(containsError(error, "generation attempt"), "generation context retained");
        check(containsError(error, "candidate 1"), "offspring index retained");
        check(containsError(error, "chosen evaluator failure"), "underlying error retained");
        try {
            try { std::rethrow_if_nested(error); }
            catch (const std::exception& evaluation) { std::rethrow_if_nested(evaluation); }
        } catch (const InjectedError&) { check(true, "underlying exception type preserved"); }
    }
    check(failed, "chosen evaluation throws");
    unchanged(p, before);
    check(ga.evolutions() == 1 && ga.lastOutcome() == GenerationOutcome::OperationalFailure, "failed evolution does not advance counter");
    check(p.lastOutcome() == GenerationOutcome::OperationalFailure, "population reports operational failure");
    check(r1.evaluationAttempts() == attempts + 2, "attempts include throwing call, no refund");
    check(r1.engine() != engine, "draws consumed on aborted attempt");
    rg.evolve(reference);
    check(r1.engine() == r2.engine(), "exact aborted-attempt RNG state matches same offspring work");
    check(reference.bestSeen()->fitness() == 100 && p.bestSeen()->fitness() == 2, "abandoned improvement not published");
    // No attempt happens until the caller explicitly calls again.
    control->throwOn = 0; control->mutationValue = -999;
    const auto invalidBefore = snapshot(p);
    const auto invalidWork = r1.evaluationAttempts();
    check(ga.evolve(p) == GenerationOutcome::NoViableCandidates, "all-invalid successor returns explicit outcome");
    unchanged(p, invalidBefore);
    check(ga.evolutions() == 1 && r1.evaluationAttempts() == invalidWork + 3, "invalid successor work remains counted");
    control->mutationValue = 0;
    check(ga.evolve(p) == GenerationOutcome::Committed && p.bestSeen()->fitness() == 2, "worse commit retains historical best");
    check(ga.evolutions() == 2 && p.generation() == 2, "only successful publications advance counters");
}

static void initializationAndOtherFailures() {
    RandomContext random(19);
    auto control = std::make_shared<Control>();
    control->throwOn = 2;
    throws<std::runtime_error>([&] {
        P fails(std::vector<Candidate>{Candidate(1, control), Candidate(2, control)}, random);
    });
    check(random.evaluationAttempts() == 2, "constructor failure accounting survives in run context");
    control->throwOn = 0;
    P p(std::vector<Candidate>{Candidate(1, control), Candidate(2, control)}, random);
    auto before = snapshot(p);
    const auto work = random.evaluationAttempts();
    control->throwOn = control->calls + 1;
    control->nonstandard = true;
    bool retained = false;
    try { p.updateValues(); }
    catch (const std::exception& e) {
        try { std::rethrow_if_nested(e); }
        catch (int value) { retained = value == 71; }
    }
    check(retained, "nonstandard evaluator exception retained");
    unchanged(p, before);
    check(random.evaluationAttempts() == work + 1, "update failure counted");
    control->throwOn = 0; control->unevaluatedOn = control->calls + 1;
    throws<std::runtime_error>([&] { p.updateValues(); });
    unchanged(p, before);
    control->unevaluatedOn = 0;
    p.setCreature(0, Candidate(9, control));
    control->throwOn = control->calls + 2;
    throws<std::runtime_error>([&] { p.updateValues(); });
    unchanged(p, before);
    check(p.bestCreature().fitness() == 2, "failed staged edit discarded, committed state ready");
    control->throwOn = 0;
    // Throw during copying/preparation, before publication; parents and archive still intact.
    GA ga(1, 1, random);
    const auto callsBeforeCopyFailure = random.evaluationAttempts();
    control->copyFaultCountdown = 0;
    throws<std::runtime_error>([&] { ga.evolve(p); });
    control->copyFaultCountdown = -1;
    unchanged(p, before);
    check(random.evaluationAttempts() == callsBeforeCopyFailure && ga.evolutions() == 0, "pre-evaluation failure consumes no evaluations");
    // Fail when copying the selected best into the ready state, after every evaluation succeeds.
    control->copyFaultAfterCall = control->calls + 2;
    const auto archiveWork = random.evaluationAttempts();
    throws<std::runtime_error>([&] { ga.evolve(p); });
    control->copyFaultCountdown = -1;
    control->copyFaultAfterCall = 0;
    unchanged(p, before);
    check(random.evaluationAttempts() == archiveWork + 2 && ga.evolutions() == 0,
          "archive-preparation failure discards evaluated offspring, counts all work");
    // Whole-population replacement uses the same viability/publication boundary.
    const auto resizeBefore = snapshot(p);
    check(p.publishGeneration({Candidate(-999), Candidate(-999)}) == GenerationOutcome::NoViableCandidates, "manual invalid publication rejected");
    unchanged(p, resizeBefore);
}
int main() {
    try {
        statesAndMutation(); mixedAndDegenerate(); evaluationFailure(); initializationAndOtherFailures();
        std::cout << checks << " fitness/publication checks passed\n";
    } catch (const std::exception& e) {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
}
