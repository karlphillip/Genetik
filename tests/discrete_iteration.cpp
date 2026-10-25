#include <DiscreteRunCheckpoint.hpp>
#include <DiscreteExpressionParsing.hpp>
#include <DiscreteExpressionFormatting.hpp>
#include <barrier>
#include <fstream>
#include <iostream>

namespace {
void check(bool b,const char* what){if(!b)throw std::runtime_error(what);}
template<class E,class F>void rejects(F&& f){try{f();}catch(const E&){return;}throw std::runtime_error("expected exception absent");}
auto parse(std::string_view s,std::size_t dimensions=1){return parseDiscreteExpression(s,dimensions);}
auto evaluate(const DiscreteExpression& e,std::int64_t n=0,DiscreteEvaluationLimits l={}){return e.evaluate(std::array{n},l);}
void equal(const DiscreteExpression& e,std::int64_t n,std::int64_t expected){auto r=evaluate(e,n,DiscreteEvaluationLimits{100000,100000,1024*1024});check(r.valid()&&r.value->integer()==expected,"independent exact integral-sequence reference");check(parse(e.toString(),e.dimensions()).sameRepresentation(e),"iteration canonical roundtrip preserves binding IDs");}
void math(){
 auto sum=parse("sum(k0, 1, x0, k0)"),squares=parse("sum(k0, 1, x0, k0*k0)"),factorial=parse("product(k0, 1, x0, k0)");
 for(std::int64_t n:{0,1,2,3,8,12,100,10000}){equal(sum,n,n*(n+1)/2);equal(squares,n,n*(n+1)*(2*n+1)/6);} // closed forms within range, independent of interpreter
 for(auto [n,target]:std::array<std::array<std::int64_t,2>,4>{{{0,1},{1,1},{5,120},{20,2432902008176640000LL}}})equal(factorial,n,target);
 check(evaluate(factorial,21).status==DiscreteEvaluationStatus::MathematicalInvalid,"21 factorial checked overflow");
 equal(parse("sum(k0, 1, 3, sum(k1, 1, k0, k1))"),0,10);
 equal(parse("sum(k0,1,3,sum(k1,1,k0,k0+k1))"),0,24);
 equal(parse("sum(k0,1,3,sum(k1,1,3,if(k0==k1,1,0)))"),0,3);
 equal(parse("sum(k0, 1, x0, if(x0 % k0 == 0, 1, 0))"),6,4);
 equal(parse("sum(k7, 0, x0, x0*k7)"),5,75);
 equal(parse("sum(k0,-3,-1,k0)"),0,-6);
 equal(parse("sum(k0,0,3,if(k0==0,0,10%k0))"),0,1);
 check(!evaluate(parse("product(k0,1,3,if(k0==1,0,1%(k0-2)))")).valid(),"zero product cannot mask later invalidity");
 equal(parse("sum(k0,3,2,1%0)"),0,0);equal(parse("product(k0,3,2,1%0)"),0,1);
 check(!evaluate(parse("sum(k0,3,1%0,k0)")).valid(),"invalid upper bound before empty convention");
 check(!evaluate(parse("product(k0,1%0,-1,k0)")).valid(),"invalid lower bound");
 equal(parse("sum(k0,9223372036854775807,9223372036854775807,k0)"),0,std::numeric_limits<std::int64_t>::max());
 equal(parse("sum(k0,-9223372036854775808,-9223372036854775808,k0)"),0,std::numeric_limits<std::int64_t>::min());
 equal(parse("sum(k0,-9223372036854775808,-9223372036854775807,0)"),0,0);
 check(!evaluate(parse("sum(k0,9223372036854775806,9223372036854775807,k0)")).valid(),"sum accumulation overflow near INT64_MAX");
 for(auto e:{parse("sum(k0,-9223372036854775808,9223372036854775807,0)"),sum}){auto result=evaluate(e,4294967295LL);check(result.status==DiscreteEvaluationStatus::ResourceExhausted&&result.iterations==0,"full-width length/near-limit series rejected before iteration");}
 equal(parse("if(true,7,sum(k0,-9223372036854775808,9223372036854775807,0))"),0,7);
 auto l=DiscreteEvaluationLimits{};l.maxAggregateIterations=0;equal(parse("sum(k0,1,0,k0)"),0,0);check(evaluate(parse("sum(k0,1,0,k0)"),0,l).valid(),"zero aggregate allowance permits empty sum");
 auto e=parse("sum(k0,1,3,k0)");l={6,6,1024*1024,3,3,1};auto exact=evaluate(e,0,l);check(exact.valid()&&exact.iterations==3&&exact.nodeVisits==6&&exact.operations==6&&exact.iterationOperations==1,"exact iteration work boundary");
 for(int field=0;field<5;++field){auto shortLimit=l;if(field==0)--shortLimit.maxNodeVisits;if(field==1)--shortLimit.maxOperations;if(field==2)--shortLimit.maxIterationsPerOperation;if(field==3)--shortLimit.maxAggregateIterations;if(field==4)--shortLimit.maxIterationDepth;auto result=evaluate(e,0,shortLimit);check(result.status==DiscreteEvaluationStatus::ResourceExhausted&&!result.value,"resource stops cannot publish partial sum");}
 auto nested=parse("sum(k0,1,3,sum(k1,1,k0,k1))");l={};l.maxAggregateIterations=8;auto partial=evaluate(nested,0,l);check(partial.status==DiscreteEvaluationStatus::ResourceExhausted&&partial.iterations==8&&!partial.value,"mid-nested exhaustion preserves attempted work");l={};l.maxIterationDepth=1;check(evaluate(nested,0,l).status==DiscreteEvaluationStatus::ResourceExhausted,"runtime nesting allowance");
 int polls=0;auto cancelled=sum.evaluateWithCancellation(std::array<std::int64_t,1>{1000},{},[&]{return ++polls==30;});check(cancelled.status==DiscreteEvaluationStatus::Cancelled&&cancelled.iterations>0&&cancelled.iterations<1000&&!cancelled.value,"bounded cooperative cancellation during loop");
}
void scopes(){
 for(auto text:{"k0","sum(k0,k0,3,k0)","sum(k0,1,k0,k0)","sum(k0,1,3,k1)","sum(k0,1,3,k0)+k0","sum(k0,1,3,sum(k0,1,2,k0))","sum(k0,1,3,true)","sum(k0,true,3,k0)","sum(k0,1,2,k0)+product(k0,1,2,k0)"})rejects<DiscreteParseError>([&]{(void)parse(text);});
 equal(parse("sum(k40,sum(k2,1,2,k2),4,k40)"),0,7);
 auto expression=parse("sum(k0,1,x0,sum(k1,1,k0,k0+k1))");auto round=parse(expression.toString());check(round.sameRepresentation(expression),"nested outer binding identity");
 auto nodes=std::vector<DiscreteNode>{DiscreteNode::iterator(42)};std::array<std::size_t,1> scope{42};DiscreteExpression body(nodes,1,{},scope);rejects<std::invalid_argument>([&]{(void)evaluate(body);});auto built=DiscreteExpression::iteration(DiscreteKind::Sum,42,DiscreteExpression::integer(1,1),DiscreteExpression::variable(0,1),body);equal(built,5,15);nodes.clear();check(built.toString()=="sum(k42, 1, x0, k42)","builder owns scope and nodes");
 const auto leaf=discreteSubtree(built,built.children(built.nodeCount()-1)[2]);check(leaf.outerBindings().size()==1,"fragment retains required outer binding");check(!replaceDiscreteSubtree(parse("x0"),0,leaf,{}),"open iterator cannot escape binder");
 auto destination=parse("sum(k0,1,2,k0)+1");auto incoming=parse("sum(k0,1,2,k0)");const auto site=destination.children(destination.nodeCount()-1)[1];auto replaced=replaceDiscreteSubtree(destination,site,incoming,{});check(replaced.has_value()&&replaced->binders().size()==2,"closed binder subtree alpha renamed");check(replaced->binders()[0]!=replaced->binders()[1]&&evaluate(*replaced).value->integer()==6,"renaming avoids capture");
 auto inside=parse("sum(k0,1,2,k0)");auto inserted=replaceDiscreteSubtree(inside,inside.children(inside.nodeCount()-1)[2],incoming,{});check(inserted&&evaluate(*inserted).value->integer()==6,"closed binder moves into nested context without capture");
 auto inner=discreteSubtree(expression,expression.children(expression.nodeCount()-1)[2]);check(!inner.outerBindings().empty()&&!replaceDiscreteSubtree(parse("x0"),0,inner,{}),"nested free dependency cannot move outside");
 auto limits=expression.limits();limits.maxIterationDepth=1;rejects<DiscreteParseError>([&]{(void)parseDiscreteExpression(expression.toString(),1,limits);});
 check(formatDiscreteExpression(built).find("Σ[")!=std::string::npos&&formatDiscreteExpression(parse("product(k0,1,x0,k0)"),DiscreteFormat::LaTeX).find("\\prod_")!=std::string::npos,"iteration presentation");
 std::array<std::int64_t,1> inputs{5};(void)built.evaluate(inputs);check(inputs[0]==5,"local iterator does not overwrite external input");
}
DiscreteSearchConfig config(){DiscreteSearchConfig c;c.population=12;c.elites=1;c.maxGenerations=100;c.maxLossAttempts=1212;c.grammar.operators={DiscreteKind::Add,DiscreteKind::Multiply,DiscreteKind::Sum,DiscreteKind::Product};c.grammar.iterationBoundsAreTerminals=true;return c;}
auto problem(){return DiscreteRegressionProblem::create(1,{{{0},0},{{0},1},{{2},7}}, {},DiscreteEvaluationLimits{100000,100000,1024*1024,32,4096,4});}
void variation(){auto p=problem();auto c=config();DiscreteVariation v(p,c.grammar);std::mt19937 rng(7);bool sawLoop=false,changed=false;
 for(int i=0;i<200;++i){auto a=v.initialize(rng),b=v.initialize(rng);sawLoop=sawLoop||!a.expression().binders().empty();auto parent=a.text();auto child=v.mutation(a,1,rng);v.validate(child.expression());changed=changed||child.text()!=parent;auto x=a,y=b;v.crossover(a,b,x,y,1,rng);v.validate(x.expression());v.validate(y.expression());check(a.text()==parent,"variation does not mutate parent");check(parse(x.text()).sameRepresentation(x.expression()),"varied binder roundtrip");}
 check(sawLoop&&changed,"iteration productions and actual varied offspring");auto d1=c.grammar;d1.operators={DiscreteKind::Add};DiscreteVariation old(p,d1);rejects<std::invalid_argument>([&]{old.validate(parse("sum(k0,1,x0,k0)"));});
 auto objective=std::make_shared<DiscreteTrainingObjective>(p,100);DiscreteExpressionCandidate candidate(parse("sum(k0,1,x0,k0)"),p,objective);candidate.calculateFitness();auto edited=v.mutation(DiscreteExpressionCandidate(candidate.expression(),p),1,rng);if(!edited.expression().sameRepresentation(candidate.expression()))check(edited.fitnessState()==FitnessState::Unevaluated,"fitness invalidated");
}
std::string populationKey(const DiscreteEvolutionRun& run,const RandomContext& random){CheckpointWriter w;auto state=RunCheckpointAccess::population(run.population());writeCommittedPopulation(w,state);auto r=RunCheckpointAccess::random(random);w.text(r.engine);w.integer(r.candidateAttempts);const auto a=RunCheckpointAccess::algorithm(run.algorithm());w.integer(a.attempts);w.integer(a.published);genetik_discrete_detail::writeWork(w,run.work());auto s=run.algorithm().variation().statistics();for(auto n:{s.initializations,s.mutations,s.crossovers,s.rejections,s.fallbacks})w.integer(n);return w.data();}
void checkpoint(const std::filesystem::path& folder,unsigned workers){auto p=problem();auto c=config();std::string continuous;{
 RandomContext random(77,ExecutionConfig{workers});DiscreteEvolutionRun whole(p,c,random);for(int i=0;i<100;++i)whole.step();continuous=populationKey(whole,random);
 }const auto path=folder/("iteration-"+std::to_string(workers)+".gtkc");{
 RandomContext random(77,ExecutionConfig{workers});DiscreteEvolutionRun part(p,c,random);for(int i=0;i<40;++i)part.step();part.save(path);
 }auto document=loadCheckpoint(path);RandomContext resumed(77,ExecutionConfig{workers});auto run=DiscreteEvolutionRun::restore(document,p,c,resumed);check(run->population().generation()==40,"restored committed generation");for(int i=40;i<100;++i)run->step();check(populationKey(*run,resumed)==continuous,"100 versus 40/destroy/load/100 entire state equality");
 auto wrong=c;wrong.grammar.maxIterationNesting=1;RandomContext fresh(77,ExecutionConfig{workers});const auto prior=fresh.engine();rejects<CheckpointError>([&]{(void)DiscreteEvolutionRun::restore(document,p,wrong,fresh);});check(fresh.engine()==prior&&fresh.evaluationAttempts()==0,"incompatible grammar rejected before RNG/work");auto policy=p->evaluationLimits();--policy.maxAggregateIterations;auto other=DiscreteRegressionProblem::create(1,p->rows(),p->limits(),policy);rejects<CheckpointError>([&]{(void)DiscreteEvolutionRun::restore(document,other,c,fresh);});
 auto corrupted=encodeCheckpoint(document);corrupted[corrupted.size()-1]^=1;rejects<CheckpointError>([&]{(void)decodeCheckpoint(corrupted);});
 auto native=parse("sum(k0,1,3,k0)");DiscreteExpressionCandidate candidate(native,p);candidate.calculateFitness();CheckpointWriter w;CandidateCheckpointCodec<DiscreteExpressionCandidate>::encode(w,candidate);CheckpointReader reader(w.data());auto restored=CandidateCheckpointCodec<DiscreteExpressionCandidate>::decode(reader,{p,{}});reader.finish();check(restored.expression().sameRepresentation(native),"explicit iteration node codec");
}
void legacyCodec(){
 const std::string hex="00000000000000010000000000000003000000000000000200000000000000000000000000000000000000000000000131000000000000000300000000000000010000000000000000000000000000000000000000000000000000000000000000";std::string bytes;
 const auto digit=[](char c){return c<='9'?c-'0':c-'a'+10;};for(std::size_t i=0;i<hex.size();i+=2)bytes.push_back(static_cast<char>(digit(hex[i])*16+digit(hex[i+1])));
 auto p=DiscreteRegressionProblem::create(1,{{{0},1},{{2},3}});CheckpointReader reader(bytes);auto restored=CandidateCheckpointCodec<DiscreteExpressionCandidate>::decode(reader,{p,{}});reader.finish();check(restored.text()=="(x0 + 1)"&&restored.fitness()==0,"original D1 codec fixture accepted");CheckpointWriter writer;CandidateCheckpointCodec<DiscreteExpressionCandidate>::encode(writer,restored);check(writer.data()==bytes,"D1 node payload bytes preserved");
}
void failedPrivateCheckpoint(const std::filesystem::path& folder){
 auto limits=DiscreteEvaluationLimits{};limits.maxIterationsPerOperation=0;auto p=DiscreteRegressionProblem::create(1,{{{1000000},0},{{1000000},1}}, {},limits);auto c=config();c.grammar.initialMaxDepth=1;c.crossoverRate=0;c.mutationRate=1;c.maxLossAttempts=10000;RandomContext random(42);DiscreteEvolutionRun run(p,c,random);bool failed=false;
 for(int attempt=0;attempt<8&&!failed;++attempt){const auto generation=run.population().generation();const auto before=run.population().bestSeen()->text();try{run.step();}catch(const std::exception&){auto failure=std::current_exception();const auto contains=[](auto&& self,std::exception_ptr e)->bool{try{std::rethrow_exception(e);}catch(const DiscreteResourceExhausted&){return true;}catch(const std::exception& e2){auto n=dynamic_cast<const std::nested_exception*>(&e2);return n&&self(self,n->nested_ptr());}catch(...){return false;}};check(contains(contains,failure),"resource failure remains distinct");check(run.population().generation()==generation&&run.population().bestSeen()->text()==before,"resource failed generation not published");failed=true;}}
 check(failed,"controlled resource interruption occurred");auto snapshot=run.checkpoint();auto state=populationKey(run,random);run.save(folder/"failed-private.gtkc");RandomContext fresh(42);auto resumed=DiscreteEvolutionRun::restore(loadCheckpoint(folder/"failed-private.gtkc"),p,c,fresh);check(resumed->checkpoint().payload==snapshot.payload&&populationKey(*resumed,fresh)==state,"failed-private checkpoint preserves population/RNG/consumed work");
}
void overlap(){ExecutionContext executor({4});std::barrier rendezvous(4);auto e=parse("sum(k0,0,x0,if(k0==0,0,10%k0))");std::atomic<int> active{0},peak{0};executor.run(4,[&](std::size_t i){int now=++active,old=peak.load();while(old<now&&!peak.compare_exchange_weak(old,now)){}rendezvous.arrive_and_wait();for(int j=0;j<50;++j){auto v=evaluate(e,static_cast<std::int64_t>(i));check(v.valid(),"concurrent shared immutable iteration");auto l=DiscreteEvaluationLimits{};l.maxAggregateIterations=0;check(evaluate(e,static_cast<std::int64_t>(i),l).status==DiscreteEvaluationStatus::ResourceExhausted,"concurrent private resource outcomes");}--active;});check(peak==4,"forced iteration overlap");}
}
int main(int argc,char** argv){try{if(argc<2)throw std::invalid_argument("workspace argument required");std::filesystem::create_directories(argv[1]);if(argc>2){overlap();checkpoint(argv[1],4);}else{math();scopes();variation();legacyCodec();failedPrivateCheckpoint(argv[1]);checkpoint(argv[1],1);checkpoint(argv[1],2);checkpoint(argv[1],4);overlap();}std::cout<<"Bounded iteration contracts passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
