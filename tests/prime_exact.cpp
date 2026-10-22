#include "../examples/PrimeExactOptions.hpp"
#include <barrier>
#include <sstream>
#include <bit>
using namespace prime_example;
static void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
static void close(double a,double b,double eps=1e-12){check(std::abs(a-b)<=eps,"independent objective expectation");}
template<class F>static void rejects(F f){bool caught=false;try{f();}catch(const std::invalid_argument&){caught=true;}check(caught,"expected invalid configuration");}
static auto constant(double c){return Expression::constant(c,1,{31,6,8});}
static auto binary(ExpressionKind k,const Expression& a,const Expression& b){return Expression::binary(k,a,b,{31,6,8});}
static void objectiveTests() {
    using K=ExpressionKind;auto x=Expression::variable(0,1,{31,6,8});
    ExactPrefixProblem p({1,2,3});auto identity=binary(K::Multiply,x,constant(.1));
    check(p.measure(identity).loss.score()==0,"all rounded matches imply zero");
    const auto twice=binary(K::Multiply,x,constant(.2));close(p.measure(twice).loss.score(),3+(0.5+2./3+3./4)/1000);
    ExactPrefixProblem one({1});close(one.measure(constant(.0002)).loss.score(),1.0005);
    check(one.measure(constant(.0002)).loss.score()<one.measure(constant(.0003)).loss.score(),"secondary orders equal mismatches");
    std::vector<std::uint32_t> labels(1000,1);ExactPrefixProblem many(labels);
    const auto most=binary(K::Add,constant(.0001),binary(K::Multiply,constant(8),binary(K::Subtract,x,constant(.001))));
    const auto fewer=many.measure(most).loss.score(),more=many.measure(constant(.0002)).loss.score();
    check(fewer>=999 && fewer<1000 && more>=1000 && fewer<more,"one fewer mismatch dominates secondary extremes");
    // Exactly representable decoded half-integer:10000/32=312.5.
    ExactPrefixProblem lower({313}),upper({312});check(lower.measure(constant(1./32)).loss.score()==0,"positive halfway rounds away from zero");
    close(upper.measure(constant(1./32)).loss.score(),1+1./3000);
    check(rounded(-312.5)==-313 && rounded(-0.0)==0,"existing signed rounding rule");
    const auto invalid=binary(K::Divide,constant(1),constant(0));check(p.measure(invalid).loss.state()==FitnessState::Invalid,"invalid arithmetic not a penalty");
    ExpressionLimits huge{31,6,std::numeric_limits<double>::max()};
    check(p.measure(Expression::constant(std::numeric_limits<double>::max(),1,huge)).loss.state()==FitnessState::Invalid,"decoded overflow invalid");
    check(p.measure(Expression::constant(0x1p63/10000.0,1,huge)).loss.state()==FitnessState::Invalid,"unrepresentable rounded integer invalid");
    const auto late=Expression::unary(K::Log,binary(K::Subtract,constant(.0015),x),{31,6,8});
    std::uint64_t rows=0;auto failed=p.measure(late,&rows);check(failed.loss.state()==FitnessState::Invalid && failed.samplesAttempted==2 && rows==2,"no valid prefix score after failed row");
    auto supplied=std::vector<std::uint32_t>{1,2,3};ExactPrefixProblem owned(supplied);supplied[0]=999;
    check(owned.referenceValues()[0]==1 && owned.measure(identity).loss.score()==0,"objective owns targets");
    rejects([]{ExactPrefixProblem e({});});rejects([]{ExactPrefixProblem e(std::vector<std::uint32_t>(1001,1));});
    rejects([]{ExactPrefixProblem e({0});});
}
static void scheduleTests() {
    using namespace std::chrono;const ExactClock::time_point start{};
    ExactReportSchedule s(start,seconds(30));check(s.due(start),"initial report");
    check(!s.due(start+seconds(29)),"no early report");check(s.due(start+seconds(30)),"deadline inclusive");
    check(s.due(start+seconds(95)) && s.nextDeadline()==start+seconds(120),"long generation catches up without drift");
    check(!s.due(start+seconds(96)),"no catch-up burst");check(s.due(start+seconds(97),true),"forced final before deadline");
    ExactReportSchedule slow(start,seconds(30));check(slow.due(start+seconds(91)) && slow.nextDeadline()==start+seconds(120),"slow initialization still absolute schedule");
    rejects([&]{ExactReportSchedule bad(start,seconds(0));});
}
static SymbolicRegressionConfig smallConfig() {
    auto c=config(10000);c.population=8;c.elites=1;c.maxGenerationAttempts=3;c.grammar.operators={ExpressionKind::Add,ExpressionKind::Multiply};c.fitting->maxLossAttempts=4;return c;
}
static void reportTests() {
    auto p=std::make_shared<const ExactPrefixProblem>(std::vector<std::uint32_t>{1});
    auto objective=std::make_shared<TrainingObjective>(p,100);
    auto prepare=std::make_shared<ConstantRefinement>(objective);
    std::vector<ExpressionCandidate> candidates;
    auto invalid=binary(ExpressionKind::Divide,constant(1),constant(0));
    for(const auto& e:std::vector<Expression>{constant(.0002),constant(.0001),constant(.0001),binary(ExpressionKind::Add,constant(.0001),constant(0)),invalid})candidates.emplace_back(e,p,prepare);
    RandomContext random(42,ExecutionConfig{2});Population<ExpressionCandidate,RegressionOrdering> population(candidates,random,ObjectiveDirection::Minimize);
    const auto before=random.engine();const auto beforeWork=objective->work();const auto calls=random.evaluationAttempts();const auto members=population.creatures();
    std::ostringstream text;text<<std::setprecision(17);ExactReportingWork work;
    exactSnapshot(text,population.selectionView(),p->referenceValues(),{},99,1,"initial","running",work);
    const auto ranking=exactRanking(population.selectionView(),99);
    check(ranking.valid==4 && ranking.invalid==1 && ranking.unevaluated==0 && ranking.indices.size()==4,"top clamps to available valid individuals");
    const auto& view=population.selectionView();check(view[ranking.indices[0]].expression().nodeCount()==1 && view[ranking.indices[0]].fitness()==0,"score then complexity ordering");
    check(ranking.indices[0]<ranking.indices[1] && view[ranking.indices[0]].text()==view[ranking.indices[1]].text(),"stable committed-slot ties");
    check(text.str().find("duplicate_formula=1")!=std::string::npos && text.str().find("displayed=4")!=std::string::npos,"individuals not deduplicated");
    check(work.predictionRows==4 && work.snapshots==1,"reporting work separate and bounded");
    check(random.engine()==before && random.evaluationAttempts()==calls && objective->work()==beforeWork,"reporting consumes no RNG or training work");
    for(std::size_t i=0;i<view.size();++i)check(view[i].text()==(*members)[i].text() && view[i].evaluation()==(*members)[i].evaluation(),"reporting never edits caches or genomes");
    std::vector<ExpressionCandidate> unevaluated;unevaluated.emplace_back(constant(1),p);check(exactRanking(unevaluated,5).unevaluated==1,"unevaluated count separate");
    std::ostringstream broken;broken.setstate(std::ios::badbit);bool failed=false;
    try{exactSnapshot(broken,view,p->referenceValues(),{},1,0,"final","running",work);}catch(const std::runtime_error&){failed=true;}
    check(failed && population.generation()==0,"report I/O failure follows publication; cannot undo committed generation");
}
static void runTests() {
    auto p=std::make_shared<const ExactPrefixProblem>(std::vector<std::uint32_t>{1,4,13});auto c=smallConfig();
    RandomContext a(2),b(2);std::ostringstream outA,outB;
    const auto clock=[] {static const auto t=ExactClock::time_point{};return t;};
    const auto x=runExactSearch(p,c,a,outA,5,std::chrono::seconds(30),true,clock);
    const auto y=runExactSearch(p,c,b,outB,5,std::chrono::seconds(30),false,clock);
    check(x.training==y.training && a.engine()==b.engine() && x.candidateRequests==y.candidateRequests && x.published==y.published && x.stop==y.stop,"reporting on/off search equivalence");
    check(x.best->text()==y.best->text() && x.best->evaluation()==y.best->evaluation(),"same best with reports");
    check(x.reporting.snapshots==2 && outA.str().find("snapshot=initial")!=std::string::npos && outA.str().find("snapshot=final")!=std::string::npos,"initial/final forced");
    // Deterministic clock advances across a deadline only at coordinator boundaries.
    unsigned ticks=0;const auto ticking=[&]{return ExactClock::time_point{}+std::chrono::seconds(31*ticks++);};
    RandomContext periodic(2);std::ostringstream timed;
    const auto z=runExactSearch(p,c,periodic,timed,5,std::chrono::seconds(30),true,ticking);
    check(z.reporting.snapshots>=3 && timed.str().find("snapshot=periodic")!=std::string::npos,"periodic at successful commit");
    c.maxTrainingLossAttempts=1;c.maxEvaluations=1;RandomContext tiny(2);std::ostringstream empty;
    auto stopped=runExactSearch(p,c,tiny,empty,5,std::chrono::seconds(30));
    check(stopped.stop==SearchStop::EvaluationBudget && !stopped.best && stopped.training.lossAttempts==0 && empty.str().find("snapshot=final")!=std::string::npos,"too-small allowance final without fake initial commit");
    c=smallConfig();c.fitting.reset();c.maxTrainingLossAttempts=9;RandomContext limited(2,ExecutionConfig{2});std::ostringstream last;
    auto exhausted=runExactSearch(p,c,limited,last,5,std::chrono::seconds(30));
    check(exhausted.stop==SearchStop::EvaluationBudget && exhausted.training.lossAttempts==8 && exhausted.published==0,"boundary allowance retained");
    // Exhaustion inside a private fitting generation retains the committed initialization.
    c=smallConfig();c.maxGenerationAttempts=0;RandomContext initialRandom(2);std::ostringstream initialText;
    const auto initial=runExactSearch(p,c,initialRandom,initialText,5,std::chrono::seconds(30),false);
    c.maxGenerationAttempts=3;c.maxTrainingLossAttempts=initial.training.lossAttempts+c.population;
    RandomContext interruptedRandom(2);std::ostringstream interruptedText;
    const auto interrupted=runExactSearch(p,c,interruptedRandom,interruptedText,5,std::chrono::seconds(30));
    check(interrupted.stop==SearchStop::EvaluationBudget && interrupted.attempts==1 && interrupted.published==0 && interrupted.unpublishedLosses>0,"private fitting attempt exhausted without publication");
    check(interrupted.training.lossAttempts==c.maxTrainingLossAttempts && interrupted.best->text()==initial.best->text() && interrupted.best->evaluation()==initial.best->evaluation(),"attempted fitting work counted; committed best retained");
    for(std::size_t i=0;i<initial.population.size();++i)check(initial.population[i].text()==interrupted.population[i].text() && initial.population[i].evaluation()==interrupted.population[i].evaluation(),"committed members retained after fitting interruption");
    std::atomic<bool> cancel{true};auto cancelled=std::make_shared<const ExactPrefixProblem>(std::vector<std::uint32_t>{1},ExpressionLimits{31,6,8},&cancel);
    RandomContext r(2);std::ostringstream final;auto stop=runExactSearch(cancelled,c,r,final,5,std::chrono::seconds(30));check(stop.stop==SearchStop::Cancelled && !stop.best && stop.training.lossAttempts==0,"early cancellation distinguished");
    auto exact=std::make_shared<const ExactPrefixProblem>(std::vector<std::uint32_t>{10000});
    // Direct winning snapshot checks do not require stochastic initialization success.
    ExpressionCandidate win(constant(1),exact);win.calculateFitness();ExactReportingWork w;std::ostringstream winning;
    exactSnapshot(winning,std::vector<ExpressionCandidate>{win},exact->referenceValues(),{},5,0,"winning","exact_training_fit",w);
    check(winning.str().find("exact_matches=1/1 mismatches=0")!=std::string::npos,"winning snapshot zero mismatch");
}
class GatedProblem final:public ExactPrefixProblem {
    mutable std::atomic<unsigned> starts_{0},active_{0},peak_{0};
    mutable std::barrier<> gate_{2};
public:
    GatedProblem():ExactPrefixProblem({1,4,13}){}
    bool parallelEvaluationSafe() const noexcept override{return true;}
    unsigned peak() const noexcept {return peak_.load();}
    RegressionMetrics measure(const Expression& e,std::uint64_t* rows=nullptr) const override {
        const auto active=active_.fetch_add(1)+1;
        auto peak=peak_.load();while(peak<active && !peak_.compare_exchange_weak(peak,active)){}
        struct Drain {std::atomic<unsigned>& active;~Drain(){active.fetch_sub(1);}} drain{active_};
        if(starts_.fetch_add(1)<2)gate_.arrive_and_wait();
        return ExactPrefixProblem::measure(e,rows);
    }
};
static void overlapTests() {
    auto c=smallConfig();std::optional<ExactSearchResult> reference;std::optional<std::mt19937> engine;
    for(unsigned threads:{1,2,4}) {
        std::shared_ptr<const ExactPrefixProblem> p=threads==1?std::make_shared<const ExactPrefixProblem>(std::vector<std::uint32_t>{1,4,13}):std::make_shared<const GatedProblem>();
        RandomContext random(2,ExecutionConfig{threads});std::ostringstream out;
        unsigned tick=0;const auto time=[&]{return ExactClock::time_point{}+std::chrono::seconds(tick++);};
        auto result=runExactSearch(p,c,random,out,5,std::chrono::seconds(1),true,time);
        check(result.best && result.published==3 && result.reporting.snapshots>=3,"worker search and boundary reporting completed");
        if(threads>1)check(static_cast<const GatedProblem&>(*p).peak()>=2 && static_cast<const GatedProblem&>(*p).peak()<=threads,"forced overlap within worker ceiling");
        if(reference) {
            check(result.training==reference->training && result.published==reference->published && result.best->text()==reference->best->text() && result.best->evaluation()==reference->best->evaluation() && random.engine()==*engine,"nonbinding thread-count equivalence");
            for(std::size_t i=0;i<result.population.size();++i)check(result.population[i].text()==reference->population[i].text() && result.population[i].evaluation()==reference->population[i].evaluation(),"stable committed population ordering");
        } else {reference=result;engine=random.engine();}
    }
}
int main(int argc,char**) {
    try {
        if(argc>1)overlapTests();
        else {objectiveTests();scheduleTests();reportTests();runTests();}
        std::cout<<"prime exact contracts passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
