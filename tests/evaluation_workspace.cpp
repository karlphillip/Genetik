#include <SymbolicRegression.hpp>
#include <array>
#include <bit>
#include <cstdlib>
#include <iostream>
#include <new>
// Test-only ordinary operator-new instrumentation. Failure disables itself before
// throwing, permitting allocation of nested diagnostic strings and exception state.
namespace allocation_test {
bool counting=false; std::size_t calls=0,bytes=0; bool failNext=false;
}
void* operator new(std::size_t n) {
    if(allocation_test::failNext){allocation_test::failNext=false;throw std::bad_alloc();}
    if(allocation_test::counting){++allocation_test::calls;allocation_test::bytes+=n;}
    if(void* p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
// Keep replacement deallocation opaque to GCC's optimized mismatched-new/delete
// heuristic; this test intentionally implements the matching new/delete via malloc/free.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void operator delete(void* p) noexcept {std::free(p);}
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
// libstdc++ stable_sort uses nothrow temporary buffers. Route the paired
// nothrow forms through this interceptor too, including under ASan interposition.
void* operator new(std::size_t n,const std::nothrow_t&) noexcept {
    try{return ::operator new(n);}catch(...){return nullptr;}
}
void* operator new[](std::size_t n,const std::nothrow_t&) noexcept {
    try{return ::operator new[](n);}catch(...){return nullptr;}
}
void operator delete(void* p,const std::nothrow_t&) noexcept {::operator delete(p);}
void operator delete[](void* p,const std::nothrow_t&) noexcept {::operator delete[](p);}

static void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
static bool badAllocation(const std::exception& e) {
    if(dynamic_cast<const std::bad_alloc*>(&e))return true;
    try{std::rethrow_if_nested(e);}catch(const std::exception& n){return badAllocation(n);}catch(...){}
    return false;
}
class AllocationObjective final : public TrainingObjective {
public:
    using TrainingObjective::TrainingObjective;
    std::uint64_t failAt=0;
protected:
    RegressionMetrics compute(const Expression& e) override {
        if(work().lossAttempts==failAt)allocation_test::failNext=true;
        return TrainingObjective::compute(e);
    }
};
static void workspace() {
    const auto x=Expression::variable(0);
    const auto log=Expression::unary(ExpressionKind::Log,x);
    const auto larger=Expression::binary(ExpressionKind::Subtract,log,x);
    genetik_detail::EvaluationWorkspace scratch,independent;
    check(scratch.evaluate(x,std::array{2.}).value()==2,"initial workspace result");
    allocation_test::failNext=true;
    bool threw=false;try{(void)scratch.evaluate(larger,std::array{1.});}catch(const std::bad_alloc&){threw=true;}
    check(threw,"growth failure propagated");
    check(scratch.evaluate(x,std::array{-0.0}).value()==0 && std::signbit(scratch.evaluate(x,std::array{-0.0}).value()),"failed growth leaves usable storage and signed zero");
    check(scratch.evaluate(larger,std::array{1.}).value()==-1,"successful growth");
    allocation_test::calls=allocation_test::bytes=0;allocation_test::counting=true;
    for(int i=0;i<100;++i) {
        const auto bad=scratch.evaluate(larger,std::array{-1.});
        const auto good=scratch.evaluate(larger,std::array{1.});
        if(bad.valid() || good.value()!=-1)std::abort();
    }
    allocation_test::counting=false;
    check(allocation_test::calls==0,"repeated valid/invalid reuse has no growth");
    try{(void)scratch.evaluate(larger,std::array{std::numeric_limits<double>::infinity()});throw std::logic_error("missing input error");}
    catch(const std::invalid_argument&){}
    check(scratch.evaluate(larger,std::array{1.}).value()==-1,"input failure cannot contaminate subsequent row");
    check(independent.evaluate(x,std::array{7.}).value()==7 && scratch.evaluate(x,std::array{3.}).value()==3,"independent invocation storage");
    const auto problem=RegressionProblem::create({{{1},0},{{2},0},{{3},0}});
    allocation_test::calls=allocation_test::bytes=0;allocation_test::counting=true;
    const auto loss=problem->measure(x);
    allocation_test::counting=false;
    check(loss.samplesAttempted==3 && loss.loss.score()==14./3. && allocation_test::calls==1 && allocation_test::bytes==sizeof(double),"one initialized-by-push stack allocation per loss");
    const auto late=RegressionProblem::create({{{1},0},{{0},0},{{2},0}});
    check(late->measure(log).samplesAttempted==2,"training still stops at first invalid row");
}
static void failure() {
    const auto problem=RegressionProblem::create({{{1},2},{{2},4}},{31,6,8});
    auto objective=std::make_shared<AllocationObjective>(problem);
    ConstantFitConfig fit;fit.maxLossAttempts=4;
    auto prep=std::make_shared<ConstantRefinement>(objective,fit);
    const auto start=Expression::binary(ExpressionKind::Multiply,Expression::constant(1,1),Expression::variable(0));
    ExpressionCandidate parent(start,problem,prep);
    RandomContext random(7);
    using P=Population<ExpressionCandidate,RegressionOrdering>;
    P population(std::vector{parent,parent},random,ObjectiveDirection::Minimize);
    const auto members=population.creatures();const auto best=population.bestSeen();
    const auto previous=objective->work();const auto oldEngine=random.engine();
    objective->failAt=previous.lossAttempts+3; // two completed probes, then first-row allocation failure
    ExpressionGrammar grammar;
    GeneticAlgorithm<P,ExpressionCandidate,ExpressionVariation> ga(0.f,0.f,random,ExpressionVariation(problem,grammar,prep));
    bool cause=false;try{ga.evolve(population);}catch(const std::exception& e){cause=badAllocation(e);}
    check(cause && population.generation()==0 && ga.evolutions()==0,"allocation cause nested, no publication");
    check(population.bestSeen()->text()==best->text() && population.bestSeen()->evaluation()==best->evaluation(),"archive preserved");
    const auto after=population.creatures();
    for(std::size_t i=0;i<members->size();++i)check(members->at(i).text()==after->at(i).text() && members->at(i).evaluation()==after->at(i).evaluation(),"committed members preserved");
    check(objective->work().lossAttempts==previous.lossAttempts+3 && objective->work().rowAttempts==previous.rowAttempts+5,"failed loss and first row remain charged");
    check(random.engine()!=oldEngine,"consumed randomness not rolled back");
    objective->failAt=0;
    check(objective->measure(best->expression()).loss==best->evaluation(),"success after allocation failure");
}
int main(){try{workspace();failure();std::cout<<"workspace ownership, growth, reset and allocation-failure accounting passed\n";}catch(const std::exception& e){allocation_test::counting=false;allocation_test::failNext=false;std::cerr<<e.what()<<'\n';return 1;}}
