#ifdef GENETIK_V05_PRODUCER
#include "v05/ScalarExpressionCheckpoint.hpp"
#else
#include <ScalarExpressionCheckpoint.hpp>
#endif
#include <ExpressionParsing.hpp>
#include <iostream>

static_assert(std::is_same_v<decltype(&loadCheckpoint),CheckpointDocument(*)(const std::filesystem::path&,CheckpointLimits)>);
using P=Population<ExpressionCandidate,RegressionOrdering>;
using A=GeneticAlgorithm<P,ExpressionCandidate,ExpressionVariation>;
auto problem(){return RegressionProblem::create({{{0},1},{{0},2},{{2},7}},{7,4,8});}
auto variation(std::shared_ptr<const RegressionProblem> p){ExpressionGrammar g;g.initialMaxDepth=2;g.mutationMaxDepth=2;return ExpressionVariation(p,g);}
std::string payload(const P& p,const A& a,const RandomContext& rng){CheckpointWriter w;auto r=RunCheckpointAccess::random(rng);w.integer(r.seed);w.integer(r.threads);w.integer(r.candidateAttempts);w.text(r.engine);auto s=RunCheckpointAccess::algorithm(a);w.integer(s.attempts);w.integer(s.published);w.integer(static_cast<unsigned>(s.lastOutcome));writeVariationStatistics(w,a.variation().statistics());writeCommittedPopulation(w,RunCheckpointAccess::population(p));return w.data();}

int main(int argc,char** argv){try{
    if(argc!=2)return 2;
    const std::filesystem::path root=argv[1];std::filesystem::create_directories(root);auto p=problem();
#ifdef GENETIK_V05_PRODUCER
    RandomContext random(41,ExecutionConfig{2});auto v=variation(p);std::vector<ExpressionCandidate> initial;for(int i=0;i<8;++i)initial.push_back(v.initialize(random.engine()));P population(initial,random,ObjectiveDirection::Minimize);A algorithm(.9,.35,random,std::move(v),1);
    for(int n=0;n<8;++n){if(n==3){CheckpointDocument d;d.codec=std::string(CandidateCheckpointCodec<ExpressionCandidate>::identifier);d.identity="v05-scalar-exact-mse/fixture-rows-and-settings-1";d.payload=payload(population,algorithm,random);d.generation=3;saveCheckpointAtomic(root/"v05.gtkc",d);auto foreign=d;foreign.codec="custom.unknown/1";saveCheckpointAtomic(root/"v05-wrong-codec.gtkc",foreign);foreign=d;foreign.runtime+=";wrong-runtime";{const auto raw=encodeCheckpoint(foreign);std::ofstream bad(root/"v05-wrong-runtime.gtkc",std::ios::binary);bad.write(raw.data(),raw.size());if(!bad)throw std::runtime_error("v0.5 negative fixture write failed");}}if(algorithm.evolve(population)!=GenerationOutcome::Committed)throw std::runtime_error("published v0.5 producer generation failed");}
    std::ofstream out(root/"expected-v05.bin",std::ios::binary);auto bytes=payload(population,algorithm,random);out.write(bytes.data(),bytes.size());if(!out)throw std::runtime_error("v0.5 expected trajectory write failed");std::cout<<genetikVersion<<" producer saved v0.5 checkpoint\n";
#else
    bool rejected=false;try{(void)loadCheckpoint(root/"v05.gtkc");}catch(const CheckpointError&){rejected=true;}if(!rejected)throw std::runtime_error("generic reader must remain exact-version");for(const auto* name:{"v05-wrong-codec.gtkc","v05-wrong-runtime.gtkc"}){bool failed=false;try{(void)loadScalarCheckpoint(root/name);}catch(const CheckpointError&){failed=true;}if(!failed)throw std::runtime_error("v0.5 import accepted wrong codec/runtime");}
    auto d=loadScalarCheckpoint(root/"v05.gtkc");if(d.producerVersion!="0.5.0"||d.identity!="v05-scalar-exact-mse/fixture-rows-and-settings-1"||d.runtime!=checkpointRuntime())throw std::runtime_error("v0.5 checkpoint provenance/context import failed");
    CheckpointReader r(d.payload);RandomCheckpoint random;random.seed=static_cast<std::uint32_t>(r.integer());random.threads=static_cast<unsigned>(r.integer());random.candidateAttempts=r.integer();random.engine=r.text();AlgorithmCheckpoint algorithm;algorithm.attempts=r.integer();algorithm.published=static_cast<unsigned>(r.integer());algorithm.lastOutcome=static_cast<GenerationOutcome>(r.integer());auto stats=readVariationStatistics(r);auto state=readCommittedPopulation<ExpressionCandidate>(r,ScalarCheckpointContext{p,{}});r.finish();
    RandomContext rng(41,ExecutionConfig{2});auto population=RunCheckpointAccess::restorePopulation<ExpressionCandidate,RegressionOrdering>(state,rng);A a(.9,.35,rng,variation(p),1);RunCheckpointAccess::restoreRandom(rng,random);RunCheckpointAccess::restoreAlgorithm(a,algorithm);a.variation().restoreStatistics(stats);for(int n=3;n<8;++n)if(a.evolve(*population)!=GenerationOutcome::Committed)throw std::runtime_error("v0.5 imported continuation failed");
    std::ifstream expected(root/"expected-v05.bin",std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(expected)),{});if(payload(*population,a,rng)!=bytes)throw std::runtime_error("v0.5 imported trajectory differs");
    std::cout<<"published0.5 producer ->0.6 consumer: population, fitness, archive, RNG, counters and continued trajectory exact\n";
#endif
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
