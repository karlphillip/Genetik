#include <DiscreteExpressionParsing.hpp>
#include <DiscreteExpressionFormatting.hpp>
#include <DiscreteSymbolicRegression.hpp>
#include <DiscreteExpressionCheckpoint.hpp>
#include <array>
#include <barrier>
#include <fstream>
#include <iostream>
#include <thread>

namespace {
void check(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}
template<class Error> bool nestedIs(std::exception_ptr failure){try{std::rethrow_exception(failure);}catch(const Error&){return true;}catch(const std::exception& e){auto nested=dynamic_cast<const std::nested_exception*>(&e);return nested&&nestedIs<Error>(nested->nested_ptr());}catch(...){return false;}}
template<class Error,class F> void rejectsNested(F&& f){try{f();}catch(...){if(nestedIs<Error>(std::current_exception()))return;throw;}throw std::runtime_error("expected nested rejection missing");}
template<class Error,class F> void rejects(F&& f){try{f();}catch(const Error&){return;}throw std::runtime_error("expected rejection missing");}
auto parse(std::string_view text,std::size_t n=1){return parseDiscreteExpression(text,n);}
auto value(const DiscreteExpression& e,std::int64_t n=0){return e.evaluate(std::array{n});}
void references(const char* path){std::ifstream file(path);check(bool(file),"open independent references");std::string line;std::size_t count=0;
 while(std::getline(file,line)){if(line.empty()||line[0]=='#')continue;auto tab=line.find('\t');auto e=std::string_view(line).substr(0,tab);auto expected=line.substr(tab+1);auto expression= parse(e,0);auto result=expression.evaluate({});
  if(expected=="invalid")check(result.status==DiscreteEvaluationStatus::MathematicalInvalid,"independent overflow/domain fixture");
  else {check(result.valid(),"independent valid fixture");if(expected=="true"||expected=="false")check(result.value->type()==DiscreteType::Bool&&result.value->boolean()==(expected=="true"),"independent comparison");else check(result.value->type()==DiscreteType::Int64&&std::to_string(result.value->integer())==expected,"independent integer fixture");}
  check(parse(expression.toString(),0).sameRepresentation(expression),"reference structural round trip");++count;
 }check(count==4000,"actual reference manifest");}
void arithmetic(){
 rejects<std::invalid_argument>([]{(void)DiscreteExpression::integer(std::numeric_limits<std::uint64_t>::max());});
 rejects<std::invalid_argument>([]{(void)DiscreteNode::integer(std::numeric_limits<std::uint64_t>::max());});
 for(auto [a,b,expected]:std::array<std::array<std::int64_t,3>,4>{{{7,3,1},{-7,3,2},{6,3,0},{0,3,0}}}){auto e=parse(std::to_string(a)+" % "+std::to_string(b),0);check(e.evaluate({}).value->integer()==expected,"Euclidean fixture");}
 for(auto text:{"neg(-9223372036854775808)","9223372036854775807 + 1","-9223372036854775808 - 1","-9223372036854775808 * -1","7 % 0","7 % -3"})check(parse(text,0).evaluate({}).status==DiscreteEvaluationStatus::MathematicalInvalid,"strict overflow/domain");
 check(parse("-9223372036854775808 % 9223372036854775807",0).evaluate({}).value->integer()==9223372036854775806LL,"extreme Euclidean remainder");
 auto lazy=parse("if(x0 == 0, 0, 10 % x0)");auto z=value(lazy,0);check(z.valid()&&z.value->integer()==0&&z.nodeVisits==5&&z.operations==2,"lazy zero with exact work");
 check(value(lazy,2).valid()&&value(lazy,2).value->integer()==0,"selected modulo");check(!value(lazy,-1).valid(),"selected nonpositive divisor invalid");
 check(value(parse("if(true, 3, 9223372036854775807 + 1)"),0).value->integer()==3,"unselected overflow is not evaluated");
 check(value(parse("if(false, 1 % 0, if(true, 7, 2 % -1))"),0).value->integer()==7,"nested lazy branch");
 check(!parse("if((9223372036854775807 + 1) == 0, 1, 2)",0).evaluate({}).valid(),"invalid condition propagates");
 auto boolean=parse("if(x0 < 0, true, x0 == 0)");check(value(boolean,-1).value->boolean()&&!value(boolean,1).value->boolean(),"Bool conditional result");
 DiscreteEvaluationLimits limits;limits.maxNodeVisits=5;limits.maxOperations=2;check(lazy.evaluate(std::array<std::int64_t,1>{0},limits).valid(),"at exact visit/operation allowance");limits.maxNodeVisits=4;check(lazy.evaluate(std::array<std::int64_t,1>{0},limits).status==DiscreteEvaluationStatus::ResourceExhausted,"below visit allowance");limits.maxNodeVisits=6;limits.maxOperations=1;check(lazy.evaluate(std::array<std::int64_t,1>{0},limits).status==DiscreteEvaluationStatus::ResourceExhausted,"below operation allowance");
 limits={};limits.maxWorkspaceBytes=0;check(lazy.evaluate(std::array<std::int64_t,1>{0},limits).status==DiscreteEvaluationStatus::ResourceExhausted,"frame byte limit");
 auto shallow=parse("if(true, 1, neg(neg(neg(neg(neg(neg(1)))))))",0);std::size_t bytes=0;for(;bytes<4096;++bytes)if(shallow.evaluate({},DiscreteEvaluationLimits{4096,4096,bytes}).valid())break;
 check(bytes<4096,"lazy frame storage sufficient");check(parse("if(true, neg(neg(neg(neg(neg(neg(1)))))), 1)",0).evaluate({},DiscreteEvaluationLimits{4096,4096,bytes}).status==DiscreteEvaluationStatus::ResourceExhausted,"only selected branch consumes frames");
 std::atomic<bool> stopped{true};check(lazy.evaluate(std::array<std::int64_t,1>{0},{},&stopped).status==DiscreteEvaluationStatus::Cancelled,"cooperative evaluation cancellation");
 rejects<std::invalid_argument>([&]{(void)lazy.evaluate({});});
}
void parsing(){
 for(auto bad:{"", "x1", "1.0", "1e2", "9223372036854775808", "-9223372036854775809", "if(true,1)", "if(1,2,3)","if(true,1,false)","true+1","(1 + 2", "x0 garbage", "foo(x0)","x0 / 2","1 < 2 < 3", "if(true,,2)"})rejects<DiscreteParseError>([&]{(void)parse(bad);});
 auto e=parse("if((x0 % 2) == 0, (x0 * x0), ((3 * x0) + 1))");auto direct=DiscreteExpression::binary(DiscreteKind::Multiply,DiscreteExpression::variable(0,1),DiscreteExpression::integer(2,1));check(direct.sameRepresentation(parse("x0*2")),"direct construction and parser agreement");
 auto canonical=e.toString();check(parse(canonical).sameRepresentation(e),"full conditional reconstruction");
 auto pretty=formatDiscreteExpression(e,DiscreteFormat::Unicode,[](std::size_t){return "n";});auto latex=formatDiscreteExpression(e,DiscreteFormat::LaTeX);check(pretty.find("mod")!=std::string::npos&&pretty.find("then")!=std::string::npos&&latex.find("\\begin{cases}")!=std::string::npos,"pretty and LaTeX preserve branches");
 check(formatDiscreteExpression(e,DiscreteFormat::Canonical)==canonical,"canonical formatting unchanged");rejects<std::length_error>([&]{(void)e.toString(1);});rejects<std::length_error>([&]{(void)formatDiscreteExpression(e,DiscreteFormat::Unicode,{},8);});
 DiscreteParsingLimits p;p.maxSourceBytes=1;check(parseDiscreteExpression("1",0,{},p).evaluate({}).valid(),"source bytes at bound");rejects<DiscreteParseError>([&]{(void)parseDiscreteExpression("12",0,{},p);});p={};p.maxTokenBytes=1;rejects<DiscreteParseError>([&]{(void)parseDiscreteExpression("12",0,{},p);});
 p={};p.maxNesting=4;check(parseDiscreteExpression("((1))",0,{},p).evaluate({}).valid(),"bounded nesting accepted");rejects<DiscreteParseError>([&]{(void)parseDiscreteExpression("((((1))))",0,{},p);});p.maxNesting=129;rejects<std::invalid_argument>([&]{(void)parseDiscreteExpression("1",0,{},p);});
 p={};p.maxWorkspaceBytes=1;rejects<DiscreteParseError>([&]{(void)parseDiscreteExpression("1",0,{},p);});
 auto l=e.limits();l.maxNodes=e.nodeCount();l.maxDepth=e.depth();l.maxConstants=e.constantCount();check(parseDiscreteExpression(canonical,1,l).sameRepresentation(e),"expression boundaries at limits");--l.maxNodes;rejects<DiscreteParseError>([&]{(void)parseDiscreteExpression(canonical,1,l);});l=e.limits();l.maxDepth=1;rejects<DiscreteParseError>([&]{(void)parseDiscreteExpression(canonical,1,l);});l=e.limits();l.maxConstants=0;rejects<DiscreteParseError>([&]{(void)parseDiscreteExpression("1",0,l);});l.maxConstantMagnitude=0;rejects<DiscreteParseError>([&]{(void)parseDiscreteExpression("1",0,l);});
 std::vector<DiscreteNode> nodes{DiscreteNode::variable(0),DiscreteNode::integer(1),DiscreteNode::operation(DiscreteKind::Add)};DiscreteExpression owned(nodes,1);nodes[1]=DiscreteNode::integer(99);check(value(owned,1).value->integer()==2,"owned node storage");auto copy=owned;nodes.clear();check(copy.sameRepresentation(owned),"copy independence");auto assigned=parse("1");assigned=owned;check(assigned.sameRepresentation(owned),"value copy assignment");auto moved=std::move(assigned);check(moved.sameRepresentation(owned),"move preserves expression");rejects<std::logic_error>([&]{(void)formatDiscreteExpression(assigned);});
 rejects<std::invalid_argument>([]{DiscreteExpression invalid(std::array{DiscreteNode::operation(DiscreteKind::Add)},0);});
 auto destination=parse("x0+1");auto prior=destination.toString();try{destination=parse("if(");}catch(const DiscreteParseError&){}check(destination.toString()==prior,"failed parse preserves destination");
}
void regression(){
 auto p=DiscreteRegressionProblem::create(1,{{{0},0},{{1},2},{{2},4}});auto e=parse("x0+x0");check(p->measure(e).loss==Fitness::valid(0),"exact equality loss zero");check(p->measure(parse("x0")).loss==Fitness::valid(2),"mismatch objective");
 auto extreme=DiscreteRegressionProblem::create(0,{{{},std::numeric_limits<std::int64_t>::min()}});check(extreme->measure(parse("9223372036854775807",0)).loss==Fitness::valid(1),"no overflowing error subtraction");
 auto q=DiscreteRegressionProblem::create(1,{{{2},0},{{0},0},{{-1},0}});auto invalid=q->measure(parse("10 % x0"),true);check(invalid.loss.state()==FitnessState::Invalid&&invalid.invalidRows==2&&invalid.rowsAttempted==3,"full invalid-row reporting");
 DiscreteEvaluationLimits shortWork;shortWork.maxNodeVisits=1;auto bounded=DiscreteRegressionProblem::create(1,{{{1},2}}, {},shortWork);rejects<DiscreteResourceExhausted>([&]{(void)bounded->measure(parse("x0+1"));});
 auto objective=std::make_shared<DiscreteTrainingObjective>(p,2);DiscreteExpressionCandidate c(e,p,objective);c.calculateFitness();check(c.fitness()==0,"candidate scoring");auto clone=c;clone.replaceExpression(parse("x0"));check(clone.fitnessState()==FitnessState::Unevaluated&&c.fitness()==0,"clone and edit invalidation");clone.calculateFitness();check(objective->work().lossAttempts==2,"whole loss attempt units");rejects<DiscreteLossBudgetExceeded>([&]{c.calculateFitness();});check(objective->work().lossAttempts==2,"budget admits no extra attempt");
 auto other=DiscreteRegressionProblem::create(1,{{{0},9}});DiscreteExpressionCandidate fresh(e,other);check(c.problemIdentity()!=fresh.problemIdentity()&&fresh.fitnessState()==FitnessState::Unevaluated,"problem identity independent of schema");
 auto movedCandidate=std::move(fresh);(void)movedCandidate;rejects<std::logic_error>([&]{fresh.replaceExpression(e);});rejects<std::logic_error>([&]{fresh.calculateFitness();});
 rejects<std::invalid_argument>([]{(void)DiscreteRegressionProblem::create(1,{});});rejects<std::invalid_argument>([]{(void)DiscreteRegressionProblem::create(1,{{{},0}});});
 // Resource failure discards a complete private successor while preserving archive/generation.
 auto limited=DiscreteRegressionProblem::create(1,{{{1},0}}, {},shortWork);auto work=std::make_shared<DiscreteTrainingObjective>(limited,100);RandomContext random(1);
 using P=Population<DiscreteExpressionCandidate,DiscreteRegressionOrdering>;
 std::vector<DiscreteExpressionCandidate> initial{{parse("0"),limited,work},{parse("1"),limited,work}};P population(initial,random,ObjectiveDirection::Minimize);const auto before=population.bestSeen()->text();auto generation=population.generation();
 auto next=initial;next[1].replaceExpression(parse("x0+1"));rejectsNested<DiscreteResourceExhausted>([&]{population.publishGeneration(next);});check(population.generation()==generation&&population.bestSeen()->text()==before&&work->work().lossAttempts==4&&work->work().rowAttempts==4,"resource failure retains consumed work and committed state");
 // Candidate codec stores integer fields and explicit fitness, never raw memory.
 DiscreteExpressionCandidate saved(parse("-9223372036854775808"),p);saved.calculateFitness();CheckpointWriter writer;CandidateCheckpointCodec<DiscreteExpressionCandidate>::encode(writer,saved);CheckpointReader reader(writer.data());auto restored=CandidateCheckpointCodec<DiscreteExpressionCandidate>::decode(reader,{p,{}});reader.finish();check(restored.text()==saved.text()&&restored.evaluation()==saved.evaluation(),"candidate codec exact integer round trip");
 std::vector<DiscreteNode> deepNodes{DiscreteNode::integer(1)};for(int i=0;i<200;++i)deepNodes.push_back(DiscreteNode::operation(DiscreteKind::Negate));DiscreteExpression deep(deepNodes,1);rejects<DiscreteParseError>([&]{(void)parse(deep.toString());});DiscreteExpressionCandidate deepCandidate(deep,p);deepCandidate.calculateFitness();CheckpointWriter deepWriter;CandidateCheckpointCodec<DiscreteExpressionCandidate>::encode(deepWriter,deepCandidate);CheckpointReader deepReader(deepWriter.data());auto deepRestored=CandidateCheckpointCodec<DiscreteExpressionCandidate>::decode(deepReader,{p,{}});deepReader.finish();check(deepRestored.expression().sameRepresentation(deep)&&deepRestored.evaluation()==deepCandidate.evaluation(),"deep node-based reconstruction beyond parser ceiling");
}
void publicationFailures(){
 auto p=DiscreteRegressionProblem::create(1,{{{1},9}});RandomContext random(7);auto objective=std::make_shared<DiscreteTrainingObjective>(p,5,&random.execution());
 std::vector<DiscreteExpressionCandidate> candidates{{parse("1"),p,objective},{parse("2"),p,objective}};
 Population<DiscreteExpressionCandidate,DiscreteRegressionOrdering> pop(candidates,random,ObjectiveDirection::Minimize);
 check(pop.publishGeneration(candidates)==GenerationOutcome::Committed,"complete admitted generation");const auto generation=pop.generation();const auto best=pop.bestSeen()->text();
 rejectsNested<DiscreteLossBudgetExceeded>([&]{(void)pop.publishGeneration(candidates);});check(pop.generation()==generation&&pop.bestSeen()->text()==best&&objective->work().lossAttempts==5,"global budget preserves committed generation and consumed work");
 auto flag=std::make_shared<bool>(false);
 struct Fault : DiscreteExpressionCandidate {std::shared_ptr<bool> fail;Fault(const DiscreteExpression& e,std::shared_ptr<const DiscreteRegressionProblem> p,std::shared_ptr<bool> f):DiscreteExpressionCandidate(e,std::move(p)),fail(std::move(f)){} Fitness evaluate() const override {if(*fail)throw std::bad_alloc();return DiscreteExpressionCandidate::evaluate();}};
 std::vector<Fault> faults{{parse("1"),p,flag},{parse("2"),p,flag}};RandomContext r(9);Population<Fault,DiscreteRegressionOrdering> committed(faults,r,ObjectiveDirection::Minimize);auto previous=committed.generation();auto archive=committed.bestSeen()->text();*flag=true;
 rejectsNested<std::bad_alloc>([&]{(void)committed.publishGeneration(faults);});check(committed.generation()==previous&&committed.bestSeen()->text()==archive,"original operational cause and unchanged publication");
 // Stop requests belong to the active batch, not an idle prior batch.
 struct Cancel : DiscreteExpressionCandidate {ExecutionContext* execution;std::shared_ptr<bool> cancel;Cancel(const DiscreteExpression& e,std::shared_ptr<const DiscreteRegressionProblem> p,ExecutionContext* x,std::shared_ptr<bool> f):DiscreteExpressionCandidate(e,std::move(p)),execution(x),cancel(std::move(f)){} Fitness evaluate() const override {if(*cancel)execution->requestStop();return DiscreteExpressionCandidate::evaluate();}};
 RandomContext cancelled(8);auto cancel=std::make_shared<bool>(false);std::vector<Cancel> tasks{{parse("1"),p,&cancelled.execution(),cancel},{parse("2"),p,&cancelled.execution(),cancel}};Population<Cancel,DiscreteRegressionOrdering> safe(tasks,cancelled,ObjectiveDirection::Minimize);auto old=safe.generation();*cancel=true;
 rejectsNested<EvaluationCancelled>([&]{(void)safe.publishGeneration(tasks);});check(safe.generation()==old,"cancelled active generation is not published");
}
void variation(){
 auto p=DiscreteRegressionProblem::create(2,{{{1,2},5}});DiscreteGrammar grammar;grammar.allowedVariables=std::vector<std::size_t>{1};DiscreteVariation v(p,grammar);std::mt19937 rng(123);bool changed=false;
 for(unsigned i=0;i<150;++i){auto a=v.initialize(rng),b=v.initialize(rng);auto original=a.text();for(const auto& n:a.expression().nodes())if(n.kind()==DiscreteKind::Variable)check(n.variable()==1,"original input index preserved");auto child=v.mutation(a,1,rng);v.validate(child.expression());if(child.text()!=original){changed=true;check(child.fitnessState()==FitnessState::Unevaluated,"mutated fitness invalidation");}auto x=a,y=b;v.crossover(a,b,x,y,1,rng);v.validate(x.expression());v.validate(y.expression());check(a.text()==original,"immutable parent");check(parse(a.text(),2).sameRepresentation(a.expression()),"generated round trip");}
 check(changed,"actual varied offspring");
 rejects<std::invalid_argument>([&]{v.validate(parse("x0",2));});
 auto small=p->limits();small.maxNodes=1;small.maxDepth=1;auto restrictive=DiscreteRegressionProblem::create(2,{{{1,2},0}},small);DiscreteVariation one(restrictive,grammar);for(int i=0;i<20;++i)check(one.initialize(rng).expression().nodeCount()==1,"node/depth boundary variation");
 DiscreteGrammar comparison;comparison.booleanConstants.clear();comparison.operators={DiscreteKind::Equal,DiscreteKind::IfThenElse};DiscreteVariation construct(p,comparison);check(construct.generate(DiscreteType::Bool,rng).type()==DiscreteType::Bool,"operation-only Boolean production");
 DiscreteGrammar impossible;impossible.allowedVariables=std::vector<std::size_t>{};impossible.constants.clear();rejects<std::invalid_argument>([&]{DiscreteVariation no(p,impossible);});
 grammar.positiveDivisors={0};rejects<std::invalid_argument>([&]{DiscreteVariation no(p,grammar);});
 auto a=parse("if(x0 == 0, 1, 2)",2),b=parse("x1+1",2);check(!replaceDiscreteSubtree(a,2,b,p->limits()),"incompatible typed subtree rejected");auto tiny=p->limits();tiny.maxStorageBytes=1;check(!replaceDiscreteSubtree(b,0,parse("1",2),tiny),"replacement storage refused before allocation");
 std::mt19937 ra(99),rb(99);DiscreteVariation da(p),db(p);for(int i=0;i<30;++i)check(da.initialize(ra).text()==db.initialize(rb).text()&&ra==rb,"deterministic generation and RNG");
}
struct Batch {std::string text;Fitness fitness;std::mt19937 rng;DiscreteTrainingWork work;std::uint64_t requests,generations;};
Batch run(unsigned workers){auto p=DiscreteRegressionProblem::create(1,{{{0},97},{{1},83},{{2},71}});DiscreteSearchConfig c;c.population=12;c.elites=1;c.maxGenerations=8;c.maxLossAttempts=108;c.grammar.operators={DiscreteKind::Add,DiscreteKind::Subtract,DiscreteKind::Multiply};RandomContext r(17,ExecutionConfig{workers});auto result=discreteSymbolicRegression(p,c,r);check(result.best.has_value(),"viable discrete search");return {result.best->text(),result.best->evaluation(),r.engine(),result.work,result.candidateRequests,result.publishedGenerations};}
void overlap(){
 ExecutionContext execution({4});auto e=parse("if(x0 == 0, 0, 10 % x0)");std::barrier gate(4);std::atomic<int> active{0},peak{0};
 execution.run(4,[&](std::size_t i){const auto now=++active;int previous=peak.load();while(previous<now&&!peak.compare_exchange_weak(previous,now)){}gate.arrive_and_wait();
  for(int k=0;k<100;++k){auto result=value(e,static_cast<std::int64_t>(i)-1);check(result.valid()==(i!=0),"overlapping lazy/domain evaluations");auto parsed=parse(e.toString());check(parsed.sameRepresentation(e),"independent parser workspace");}--active;});check(peak==4,"forced overlap");
 // Force the candidate-preparation path to overlap, including a failing private generation.
 auto problem=DiscreteRegressionProblem::create(1,{{{1},0}});RandomContext random(44,ExecutionConfig{4});auto objective=std::make_shared<DiscreteTrainingObjective>(problem,100,&random.execution());
 auto rendezvous=std::make_shared<std::barrier<>>(4);auto fail=std::make_shared<std::atomic<bool>>(false);
 struct Prepared : DiscreteExpressionCandidate {
   std::shared_ptr<std::barrier<>> rendezvous;std::shared_ptr<std::atomic<bool>> fail;
   Prepared(const DiscreteExpression& e,std::shared_ptr<const DiscreteRegressionProblem> p,std::shared_ptr<DiscreteTrainingObjective> o,std::shared_ptr<std::barrier<>> b,std::shared_ptr<std::atomic<bool>> f):DiscreteExpressionCandidate(e,std::move(p),std::move(o)),rendezvous(std::move(b)),fail(std::move(f)){}
   bool parallelEvaluationSafe() const noexcept override{return true;}
   Fitness evaluate() const override {rendezvous->arrive_and_wait();if(fail->load()&&expression().nodes()[0].integer()==2)throw std::bad_alloc();return DiscreteExpressionCandidate::evaluate();}
 };
 std::vector<Prepared> prepared;for(int i=0;i<4;++i)prepared.emplace_back(parse(std::to_string(i)),problem,objective,rendezvous,fail);
 Population<Prepared,DiscreteRegressionOrdering> population(prepared,random,ObjectiveDirection::Minimize);check(objective->work().lossAttempts==4&&objective->work().rowAttempts==4,"overlapping preparation merges private work");auto generation=population.generation();auto archive=population.bestSeen()->text();fail->store(true);
 rejectsNested<std::bad_alloc>([&]{(void)population.publishGeneration(prepared);});check(population.generation()==generation&&population.bestSeen()->text()==archive&&random.evaluationAttempts()==8,"overlapping operational failure drains and preserves archive");
 auto one=run(1);for(auto workers:{2,4}){auto other=run(workers);check(other.text==one.text&&other.fitness==one.fitness&&other.rng==one.rng&&other.work==one.work&&other.requests==one.requests&&other.generations==one.generations,"nonbinding serial/parallel equality");}
}
}
int main(int argc,char** argv){try{if(argc==2&&std::string_view(argv[1])=="--overlap")overlap();else {check(argc==2,"reference fixture path required");references(argv[1]);arithmetic();parsing();regression();publicationFailures();variation();overlap();}std::cout<<"Discrete contracts passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
