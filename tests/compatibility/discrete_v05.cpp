#ifdef GENETIK_V05_PRODUCER
#include "v05/DiscreteRunCheckpoint.hpp"
#else
#include <DiscreteRunCheckpoint.hpp>
#endif
#include <iostream>

auto problem(){return DiscreteRegressionProblem::create(1,{{{0},1},{{1},2},{{2},3},{{3},4}});}
DiscreteSearchConfig config(){DiscreteSearchConfig c;c.population=8;c.elites=1;c.maxGenerations=8;c.maxLossAttempts=1000;c.grammar.operators={DiscreteKind::Add,DiscreteKind::Subtract,DiscreteKind::Multiply};c.grammar.initialMaxDepth=2;c.grammar.mutationMaxDepth=2;return c;}

int main(int argc,char** argv){try{
    if(argc!=2)return 2;
    const std::filesystem::path root=argv[1];std::filesystem::create_directories(root);auto p=problem();auto c=config();
#ifdef GENETIK_V05_PRODUCER
    RandomContext random(427,ExecutionConfig{2});DiscreteEvolutionRun run(p,c,random);for(unsigned i=0;i<3;++i)run.step();run.save(root/"v05-discrete.gtkc");for(unsigned i=3;i<c.maxGenerations;++i)run.step();std::ofstream out(root/"expected-v05-discrete.bin",std::ios::binary);const auto bytes=run.checkpoint().payload;out.write(bytes.data(),bytes.size());if(!out)throw std::runtime_error("v0.5 discrete expected trajectory write failed");std::cout<<genetikVersion<<" producer saved v0.5 discrete checkpoint\n";
#else
    bool rejected=false;try{(void)loadCheckpoint(root/"v05-discrete.gtkc");}catch(const CheckpointError&){rejected=true;}if(!rejected)throw std::runtime_error("generic reader must remain exact-version");auto d=loadDiscreteRunCheckpoint(root/"v05-discrete.gtkc");if(d.producerVersion!="0.5.0"||d.codec!="genetik.discrete-evolution/2")throw std::runtime_error("v0.5 discrete checkpoint provenance/codec import failed");RandomContext random(427,ExecutionConfig{2});auto run=DiscreteEvolutionRun::restore(d,p,c,random);if(run->population().generation()!=3)throw std::runtime_error("v0.5 committed generation was not restored");for(unsigned i=3;i<c.maxGenerations;++i)run->step();std::ifstream expected(root/"expected-v05-discrete.bin",std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(expected)),{});if(run->checkpoint().payload!=bytes)throw std::runtime_error("v0.5 discrete full state continuation differs");std::cout<<"published0.5 discrete producer ->0.6 consumer: population, fitness, archive, RNG, work and continuation exact\n";
#endif
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
