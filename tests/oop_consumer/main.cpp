#include <RunCheckpoint.hpp>
// Application-owned permutation scheduling. No symbolic-regression headers.
#include <EvaluatedCandidate.hpp>
#include <Population.hpp>
#include <GeneticAlgorithm.hpp>
#include <algorithm>
#include <atomic>
#include <array>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace {
void require(bool ok) { if (!ok) throw std::runtime_error("permutation contract failed"); }
using Jobs = std::array<unsigned, 4>;
// Runtime-polymorphic application objective; immutable problem definition per run.
struct Objective {
    virtual ~Objective() = default;
    virtual bool parallelSafe() const noexcept {return false;}
    virtual Fitness score(const Jobs&) const = 0;
};
struct CompletionCost final : Objective {
    bool parallelSafe() const noexcept override {return true;}
    mutable std::atomic<unsigned> calls{0};
    unsigned throwAt = 0; // deterministic operational-failure injection only
    bool invalid = false;
    Fitness score(const Jobs& jobs) const override {
        if (++calls == throwAt) throw std::runtime_error("application evaluator fault");
        if (invalid) return Fitness::invalid();
        double time = 0, cost = 0;
        for (unsigned job : jobs) { time += job + 1; cost += time * (4-job); }
        return Fitness::valid(cost);
    }
};
class Schedule final : public EvaluatedCandidate {
public:
    Schedule(const Jobs& jobs, std::shared_ptr<const Objective> objective)
        : jobs_(jobs), objective_(std::move(objective)) {
        validate(jobs_);
        if (!objective_) throw std::invalid_argument("missing objective");
    }
    bool parallelEvaluationSafe() const noexcept override {return objective_->parallelSafe();}
    const Jobs& jobs() const noexcept { return jobs_; }
    void replace(const Jobs& jobs) {
        validate(jobs); // failed edits preserve both genome and cache
        Jobs detached = jobs;
        jobs_ = detached;
        invalidateFitness();
    }
    std::shared_ptr<const void> problemIdentity() const noexcept override { return objective_; }
    std::string stats() override { return "schedule"; }
protected:
    Fitness evaluate() const override { return objective_->score(jobs_); }
private:
    static void validate(Jobs jobs) {
        std::sort(jobs.begin(), jobs.end());
        if (jobs != Jobs{0,1,2,3}) throw std::invalid_argument("not a permutation");
    }
    Jobs jobs_;
    std::shared_ptr<const Objective> objective_;
};
struct PermutationVariation {
    static Jobs ordered(const Jobs& prefix, const Jobs& donor, std::size_t cut) {
        Jobs result = prefix;
        std::size_t pos = cut;
        for (unsigned job : donor)
            if (std::find(result.begin(),result.begin()+cut,job)==result.begin()+cut)
                result[pos++] = job;
        return result;
    }
    void crossover(const Schedule& f, const Schedule& m, Schedule& a, Schedule& b,
                   float rate, std::mt19937& rng) {
        Schedule left=f, right=m; // inputs and outputs may alias
        if (std::uniform_real_distribution<float>(0,1)(rng)<rate) {
            const auto cut=std::uniform_int_distribution<unsigned>(1,3)(rng);
            left.replace(ordered(f.jobs(),m.jobs(),cut));
            right.replace(ordered(m.jobs(),f.jobs(),cut));
        }
        a=left; b=right;
    }
    Schedule mutation(const Schedule& c, float rate, std::mt19937& rng) {
        Schedule result=c;
        if (std::uniform_real_distribution<float>(0,1)(rng)<rate) {
            auto jobs=c.jobs();
            const auto i=std::uniform_int_distribution<unsigned>(0,3)(rng);
            const auto offset=std::uniform_int_distribution<unsigned>(1,3)(rng);
            std::swap(jobs[i],jobs[(i+offset)%4]);
            result.replace(jobs);
        }
        return result;
    }
};
using Pool = Population<Schedule>;
using BaseAlgorithm = GeneticAlgorithm<Pool,Schedule,PermutationVariation>;
// Existing algorithm-level virtual entry points are exercised by evolve(), not
// only called directly by this application.
class Algorithm final : public BaseAlgorithm {
public:
    using BaseAlgorithm::BaseAlgorithm;
    unsigned selections=0, crosses=0, mutations=0;
    bool failVariation=false, invalidSelection=false;
    Schedule roulette(const Pool& p) override {
        ++selections; auto c=BaseAlgorithm::roulette(p);
        if (invalidSelection) c.invalidateFitness();
        return c;
    }
    void crossover(const Schedule& f,const Schedule& m,Schedule& a,Schedule& b) override {
        ++crosses; BaseAlgorithm::crossover(f,m,a,b);
    }
    Schedule mutation(const Schedule& c) override {
        ++mutations;
        auto result=BaseAlgorithm::mutation(c);
        if (failVariation) throw std::runtime_error("application variation fault");
        return result;
    }
};
std::string state(const Pool& pool) {
    std::ostringstream out;
    out << *pool.generation() << ':' << *pool.bestGeneration() << ':';
    const auto members=pool.creatures();
    for (const auto& c : *members) {
        for (auto job:c.jobs()) out << job;
        out << ':' << static_cast<int>(c.fitnessState()) << ':' << c.fitness() << ';';
    }
    const auto best=*pool.bestSeen();
    for (auto job:best.jobs()) out << job;
    out << ':' << best.fitness();
    return out.str();
}
void originalCause(const std::exception& e,const std::string& message) {
    try { std::rethrow_if_nested(e); }
    catch (const std::exception& nested) { originalCause(nested,message); return; }
    require(e.what()==message);
}
}
int main(int argc,char** argv) {
    try {
    ExecutionConfig execution;
    if(argc!=1){if(argc!=3 || std::string_view(argv[1])!="--threads")return 1;execution=ExecutionConfig::parse(argv[2]);}
    std::cout<<"threads_requested="<<execution.threads<<" threads_effective="<<std::min(execution.threads,8u)<<'\n';
    auto objective=std::make_shared<CompletionCost>();
    Jobs input{0,1,2,3};
    Schedule known(input,objective); known.calculateFitness(); require(known.fitness()==35);
    input={3,2,1,0}; require(known.jobs()==Jobs{0,1,2,3});
    Schedule reverse(input,objective); reverse.calculateFitness(); require(reverse.fitness()==85);
    auto copy=known; copy.replace(input);
    require(copy.fitnessState()==FitnessState::Unevaluated && known.fitness()==35);
    try { known.replace({0,0,1,2}); require(false); } catch (const std::invalid_argument&) {}
    require(known.fitness()==35 && known.jobs()==Jobs{0,1,2,3});
    require(PermutationVariation::ordered(Jobs{0,1,2,3},Jobs{3,2,1,0},2)==Jobs{0,1,3,2});
    std::mt19937 operatorRng(5);
    PermutationVariation variation;
    auto changed=variation.mutation(known,1,operatorRng);
    require(changed.jobs()!=known.jobs() && changed.fitnessState()==FitnessState::Unevaluated);
    variation.crossover(known,reverse,changed,reverse,1,operatorRng);
    auto sorted=changed.jobs(); std::sort(sorted.begin(),sorted.end());
    require(sorted==Jobs{0,1,2,3} && known.fitness()==35);
    RandomContext random(42,execution);
    std::vector<Schedule> initial;
    for (unsigned i=0;i<8;++i) {
        Jobs jobs{0,1,2,3}; std::shuffle(jobs.begin(),jobs.end(),random.engine());
        initial.emplace_back(jobs,objective);
    }
    Pool pool(initial,random,ObjectiveDirection::Minimize);
    Algorithm algorithm(1,1,random,PermutationVariation{},1);
    GeneticAlgorithm_i<Pool,Schedule>& interface=algorithm;
    const auto before=state(pool);
    const auto committed=RunCheckpointAccess::population(pool);RandomContext imported(42,execution);
    RunCheckpointAccess::restoreRandom(imported,RunCheckpointAccess::random(random));
    auto restored=RunCheckpointAccess::restorePopulation<Schedule>(committed,imported);
    require(state(*restored)==before && imported.engine()==random.engine() && imported.evaluationAttempts()==random.evaluationAttempts());
    auto snapshot=pool.creatures();
    snapshot->front().replace({3,2,1,0});
    require(state(pool)==before);
    if(execution.threads>1){
        for(unsigned i=0;i<5;++i)require(interface.evolve(pool)==GenerationOutcome::Committed);
        require(random.evaluationAttempts()==48 && pool.generation()==5 && algorithm.selections>0 && algorithm.crosses>0 && algorithm.mutations>0);
        require(random.execution().peakTasks()<=execution.threads);
        std::cout<<"parallel permutation contracts passed\n";return 0;
    }
    algorithm.invalidSelection=true;
    try { interface.evolve(pool); require(false); }
    catch (const std::runtime_error& e) { originalCause(e,"selection must return valid evaluated parents"); }
    require(state(pool)==before && random.evaluationAttempts()==8);
    algorithm.invalidSelection=false;
    auto rng=random.engine();
    algorithm.failVariation=true;
    try { interface.evolve(pool); require(false); }
    catch (const std::runtime_error& e) { originalCause(e,"application variation fault"); }
    require(state(pool)==before && random.engine()!=rng && random.evaluationAttempts()==8);
    algorithm.failVariation=false;
    objective->throwAt=objective->calls.load()+3;
    try { interface.evolve(pool); require(false); }
    catch (const std::runtime_error& e) { originalCause(e,"application evaluator fault"); }
    require(state(pool)==before && random.evaluationAttempts()==11);
    objective->throwAt=0;
    objective->invalid=true;
    require(interface.evolve(pool)==GenerationOutcome::NoViableCandidates);
    require(state(pool)==before && random.evaluationAttempts()==19);
    objective->invalid=false;
    for (unsigned i=0;i<5;++i) require(interface.evolve(pool)==GenerationOutcome::Committed);
    require(pool.generation()==5 && algorithm.evolutions()==5);
    require(algorithm.selections>0 && algorithm.crosses>0 && algorithm.mutations>0);
    require(random.evaluationAttempts()==59);
    auto bad=std::make_shared<CompletionCost>(); bad->invalid=true;
    RandomContext other(0);
    Pool noBest(std::vector<Schedule>{Schedule(input,bad)},other,ObjectiveDirection::Minimize);
    require(!noBest.generation() && !noBest.bestSeen());
    std::cout << "permutation OOP contracts passed; generations=" << *pool.generation()
              << " evaluations=" << random.evaluationAttempts() << '\n';
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
