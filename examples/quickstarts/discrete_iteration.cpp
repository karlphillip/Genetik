#include <DiscreteExpressionParsing.hpp>
#include <DiscreteExpressionFormatting.hpp>
#include <DiscreteRunCheckpoint.hpp>
#include <charconv>
#include <iostream>
int main(int argc,char** argv){try{
 unsigned threads=1;if(argc!=1){if(argc!=3||std::string_view(argv[1])!="--threads")throw std::invalid_argument("usage: genetik_discrete_iteration_quickstart [--threads N]");auto text=std::string_view(argv[2]);auto converted=std::from_chars(text.data(),text.data()+text.size(),threads);if(converted.ec!=std::errc{}||converted.ptr!=text.data()+text.size()||!threads||threads>64)throw std::invalid_argument("threads must be 1..64");}
 for(const auto& [text,expected]:std::array<std::pair<std::string_view,std::int64_t>,3>{{{"sum(k0,1,x0,k0)",15},{"product(k0,1,x0,k0)",120},{"sum(k0,1,3,sum(k1,1,k0,k1))",10}}}){
  auto e=parseDiscreteExpression(text,1);auto result=e.evaluate(std::array<std::int64_t,1>{5});if(!result.valid()||result.value->integer()!=expected||!parseDiscreteExpression(e.toString(),1).sameRepresentation(e))return 1;
  std::cout<<e.toString()<<" = "<<result.value->integer()<<" (iterations "<<result.iterations<<")\n"<<formatDiscreteExpression(e,DiscreteFormat::LaTeX)<<'\n';
 }
 auto problem=DiscreteRegressionProblem::create(1,{{{0},0},{{1},1},{{2},3},{{3},6}});DiscreteSearchConfig c;c.population=16;c.maxLossAttempts=336;c.maxGenerations=20;c.grammar.operators={DiscreteKind::Add,DiscreteKind::Multiply,DiscreteKind::Sum};c.grammar.iterationBoundsAreTerminals=true;
 RandomContext random(42,ExecutionConfig{threads});DiscreteEvolutionRun run(problem,c,random);while(!run.stoppingReason())if(run.step()!=GenerationOutcome::Committed)break;
 auto best=run.population().bestSeen();if(!best)return 1;
 std::cout<<"Bounded search: "<<best->text()<<" mismatches="<<best->fitness()<<" generation="<<*run.population().generation()<<" attempts="<<run.work().lossAttempts<<" iterations="<<run.work().iterations<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
