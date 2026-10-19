#include "Creature.hpp"
#include "GeneticAlgorithm.hpp"
#include "Population.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

static int checks = 0;
static void check(bool ok, const char* message) {
    ++checks;
    if (!ok) throw std::runtime_error(message);
}
template<class E, class F> void throws(F action) {
    bool caught = false;
    try { action(); } catch (const E&) { caught = true; }
    check(caught, "expected exception was not thrown");
}
using C = Creature<char>;
using P = Population<C>;
using GA = GeneticAlgorithm<P, C>;

// Deterministic, real-valued fitness fixture, independent of the character target.
struct Score : Creature_i<double> {
    explicit Score(std::mt19937& rng)
        : Creature_i<double>({std::uniform_real_distribution<double>(0, 1)(rng)}) {}
    explicit Score(double value) : Creature_i<double>({value}) {}
    std::string stats() override {
        return fitnessState() == FitnessState::Valid ? std::to_string(fitness()) : "no-score";
    }
protected:
    Fitness evaluate() const override { return Fitness::fromScore(gene(0)); }
    double mutatedGene(unsigned int, std::mt19937& rng) const override {
        return std::uniform_real_distribution<double>(0, 1)(rng);
    }
};
using SP = Population<Score>;
using SG = GeneticAlgorithm<SP, Score>;

static void ownershipAndFitness() {
    static_assert(std::has_virtual_destructor_v<Creature_i<char>>);
    static_assert(std::has_virtual_destructor_v<Population_i<C>>);
    static_assert(std::has_virtual_destructor_v<GeneticAlgorithm_i<P,C>>);
    static_assert(!std::is_copy_constructible_v<RandomContext>);
    const std::string target = TARGET;
    C c(std::vector<char>(target.begin(), target.end()));
    c.calculateFitness();
    check(c.fitness() == 1, "target fitness");
    C copy = c;
    copy.setGene(0, '?');
    check(c.gene(0) == target[0], "copy must own its genome");
    check(copy.fitnessState() == FitnessState::Unevaluated, "gene write invalidates fitness");
    throws<std::logic_error>([&] { copy.fitness(); });
    copy.calculateFitness();
    check(std::abs(copy.fitness() - 73.f / 74.f) < 1e-6, "fitness recalculation");
    C moved = std::move(copy);
    check(moved.chromosome().size() == target.size(), "move expression preserves genome");
    RandomContext random(91);
    moved.mutateGene(0, random.engine());
    check(moved.fitnessState() == FitnessState::Unevaluated, "mutation invalidates fitness");
    C wrong(std::vector<char>{'x'});
    wrong.calculateFitness();
    check(wrong.fitnessState() == FitnessState::Invalid, "wrong target shape is candidate invalidity");
    P one(1, random);
    check(one.bestCreature().chromosome() == one.creatures()->front().chromosome(), "singleton best");
    auto snapshot = one.creatures();
    snapshot->clear();
    check(one.creatures()->size() == 1, "creatures is an independent snapshot");
    throws<std::invalid_argument>([&] { one.setSize(0); });
    check(one.creatures()->size() == 1, "rejected resize preserves population");
    throws<std::out_of_range>([&] { one.setCreature(1, c); });
}

static void selectionContracts() {
    RandomContext random(123);
    SP p(2, random);
    p.setCreature(0, Score(.001));
    p.setCreature(1, Score(.003));
    p.updateValues();
    auto members = p.creatures();
    check(std::abs(members->front().fitnessPercent() - 100.f / 3) < 1e-4, "fractional fitness normalization");
    check(std::abs(members->front().fitnessPercent() - 100.f / 3) < 1e-4, "roulette percentage units");
    check(members->back().fitnessPercent() > 66, "roulette covers full interval");
    SG ga(1, 1, random);
    int low = 0;
    for (int i = 0; i < 4000; ++i) low += ga.roulette(p).fitness() < .002;
    check(low > 1150 && low < 1500, "seeded roulette samples both weighted ranges");
    // Equal zero scores use uniform weights, never an accidental final-member monopoly.
    p.setCreature(0, Score(0.0)); p.setCreature(1, Score(0.0)); p.updateValues();
    members = p.creatures();
    check(members->front().fitnessPercent() == 50, "zero score fallback");
    p.setCreature(0, Score(2.0)); p.setCreature(1, Score(2.0)); p.updateValues();
    check(p.creatures()->front().fitnessPercent() == 50, "equal positive scores");
    for (const auto invalid : {std::numeric_limits<float>::infinity(),
                               std::numeric_limits<float>::quiet_NaN()}) {
        p.setCreature(0, Score(invalid));
        check(p.updateValues() == GenerationOutcome::Committed, "invalid member allows viable generation");
        check(p.creatures()->front().fitnessState() == FitnessState::Invalid, "invalid member ranked first");
        check(p.creatures()->front().fitnessPercent() == 0, "invalid member has no selection weight");
    }
}

static void operatorContracts() {
    RandomContext random(5), other(5);
    GA ga(1, 1, random);
    C father(std::vector<char>(8, 'A')), mother(std::vector<char>(8, 'B'));
    C a = father, b = mother;
    ga.crossover(father, mother, a, b);
    check(a.chromosome().size() == 8 && b.chromosome().size() == 8, "parent sized crossover");
    for (unsigned int i = 0; i < 8; ++i)
        check((a.gene(i) == 'A' && b.gene(i) == 'B') ||
              (a.gene(i) == 'B' && b.gene(i) == 'A'), "complementary crossover");
    // Same template, different length: regresses old static 74-gene size.
    C shortC(std::vector<char>{'C'});
    ga.crossover(shortC, shortC, a, b);
    check(a.chromosome().size() == 1, "crossover must not cache chromosome size");
    throws<std::invalid_argument>([&] { ga.crossover(father, shortC, a, b); });
    C empty(std::vector<char>{});
    throws<std::invalid_argument>([&] { ga.mutation(empty); });
    throws<std::invalid_argument>([&] { ga.crossover(empty, empty, a, b); });
    P mismatch(2, other);
    throws<std::runtime_error>([&] { ga.evolve(mismatch); });
    throws<std::invalid_argument>([&] { ga.roulette(mismatch); });
    for (const float rate : {-0.1f, 1.1f, std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::infinity()}) {
        throws<std::invalid_argument>([&] { GA bad(rate, 0, random); });
        throws<std::invalid_argument>([&] { GA bad(0, rate, random); });
    }
    throws<std::invalid_argument>([&] { P zero(0, random); });
    GA disabled(0, 0, random);
    check(disabled.mutation(father).chromosome() == father.chromosome(), "disabled mutation");
    disabled.crossover(father, mother, a, b);
    check(a.chromosome() == father.chromosome() && b.chromosome() == mother.chromosome(), "disabled crossover");
}

static void repeatability() {
    RandomContext r1(2026), r2(2026), noise(7), different(2027);
    check(r1.seed() == 2026, "effective seed retained");
    SP p1(7, r1), p2(7, r2), interferer(7, noise), p3(7, different);
    SG g1(1, 1, r1), g2(1, 1, r2), gn(1, 1, noise);
    check(p1.stats() == p2.stats(), "repeatable initialization");
    check(p1.stats() != p3.stats(), "seed affects initialization");
    for (int generation = 0; generation < 25; ++generation) {
        g1.evolve(p1);
        gn.evolve(interferer); // Independent runs cannot perturb r1 or r2.
        g2.evolve(p2);
        const auto a = p1.creatures(), b = p2.creatures();
        check(a->size() == 7 && b->size() == 7, "odd population size preserved");
        for (std::size_t i = 0; i < a->size(); ++i)
            check(a->at(i).chromosome() == b->at(i).chromosome(), "repeatable full generation");
    }
    check(g1.evolutions() == 25, "generation count");
    SP single(1, r1);
    g1.evolve(single);
    check(single.creatures()->size() == 1, "singleton evolution");
    single.setSize(3);
    check(single.creatures()->size() == 3, "explicit RNG used by resize");
    // Concrete character genome uses the supplied engine for initialization and gene mutation.
    RandomContext ca(44), cb(44);
    C a(ca.engine()), b(cb.engine());
    check(a.chromosome() == b.chromosome(), "character initialization repeatability");
    for (unsigned int i = 0; i < 74; ++i) {
        a.mutateGene(i, ca.engine()); b.mutateGene(i, cb.engine());
    }
    check(a.chromosome() == b.chromosome(), "character mutation repeatability");
}
int main() {
    try {
        ownershipAndFitness(); selectionContracts(); operatorContracts(); repeatability();
        std::cout << checks << " contract checks passed\n";
    } catch (const std::exception& e) {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
}
