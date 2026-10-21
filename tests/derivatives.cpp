#include <ExpressionDerivative.hpp>
#include <ExpressionParsing.hpp>
#include <ScalarExpressionTemplate.hpp>
#include <Execution.hpp>
#include "fixtures/derivatives/references.hpp"
#include <array>
#include <barrier>
#include <cstdlib>
#include <iostream>
#include <new>
#include <random>

// Test-only allocation failure / forced overlap INSIDE invocation-owned allocation.
namespace fault { thread_local bool fail=false; thread_local std::barrier<>* gate=nullptr; }
void* operator new(std::size_t n) {
    if(fault::gate){auto* gate=fault::gate;fault::gate=nullptr;gate->arrive_and_wait();}
    if(fault::fail){fault::fail=false;throw std::bad_alloc();}
    if(void* p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
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
void* operator new(std::size_t n,const std::nothrow_t&) noexcept {try{return ::operator new(n);}catch(...){return nullptr;}}
void* operator new[](std::size_t n,const std::nothrow_t&) noexcept {try{return ::operator new[](n);}catch(...){return nullptr;}}
void operator delete(void* p,const std::nothrow_t&) noexcept {::operator delete(p);}
void operator delete[](void* p,const std::nothrow_t&) noexcept {::operator delete[](p);}

using K=ExpressionKind;using S=DerivativeStatus;using F=DerivativeFailure;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static std::uint64_t bits(double x){return std::bit_cast<std::uint64_t>(x);}
template<class E,class Callable> static void throws(Callable f){bool caught=false;try{f();}catch(const E&){caught=true;}check(caught,"expected exception");}
static void equal(const DerivativeResult& a,const DerivativeResult& b) {
    check(a.status()==b.status() && a.work()==b.work() && a.primalFailedNode()==b.primalFailedNode() &&
          a.derivativeFailure()==b.derivativeFailure() && a.derivativeFailedNode()==b.derivativeFailedNode() &&
          a.exhaustedResource()==b.exhaustedResource() && a.resourceNode()==b.resourceNode(),"result and work equivalence");
    if(a.valueAvailable())check(bits(a.value())==bits(b.value()),"value bits");
    if(a.derivativeAvailable())check(bits(a.derivative())==bits(b.derivative()),"derivative bits");
}
static DerivativeResult ordinary(const Expression& e,std::span<const double> inputs,std::size_t target) {
    const auto plain=e.evaluate(inputs);const auto d=evaluatePartial(e,inputs,target);
    check(plain.valid()==d.valueAvailable(),"primal classification equivalence");
    check(plain.arithmeticOperations()==d.work().primalOperations,"primal actual work equivalence");
    check(plain.failedNode()==d.primalFailedNode(),"first primal failure node equivalence");
    if(plain.valid())check(bits(plain.value())==bits(d.value()),"ordinary primal bits");
    else check(d.status()==S::PrimalInvalidity,"primal invalidity wins");
    return d;
}
static DerivativeResult parsed(std::string_view text,std::span<const double> inputs,std::size_t target=0) {
    return ordinary(parseExpression(text,inputs.size()),inputs,target);
}
static void analytic() {
    const std::array p{2.,0.};const auto e=parseExpression("x0*x0+x0*x1+sin(x1)",2);
    check(ordinary(e,p,0).derivative()==4 && ordinary(e,p,1).derivative()==3,"multivariate partials");
    check(parsed("x0-x0",p).derivative()==0,"repeated occurrences");
    check(parsed("x1-x0",p).derivative()==-1,"right-only subtraction");
    check(parsed("x0-x1",p).derivative()==1,"left-only subtraction");
    check(parsed("x0/x0",std::array{2.}).derivative()==0,"both-active quotient");
    check(parsed("x0/x1",std::array{6.,2.}).derivative()==.5,"left quotient");
    check(parsed("x0/x1",std::array{6.,2.},1).derivative()==-1.5,"right quotient");
    check(parsed("(x0*x0)/(x0+1)",std::array{1.}).derivative()==.75,"product and quotient");
    check(parsed("exp((-1.0)*x0)",std::array{0.}).derivative()==-1,"parsed decay");
    check(bits(parsed("x0",std::array{4.,5.},1).derivative())==bits(0.),"unused target +0");
    const ScalarExpressionTemplate scaffold(parseExpression("((-1.0)*x1)*x0",2),1,{9,4,8},{13,6,8});
    const auto model=scaffold.materialize(parseExpression("1+x1*x1",2));
    for(double v:{3.,0.}) {
        const std::array at{v,2.};
        const auto dv=ordinary(model,at,0),dt=ordinary(model,at,1);
        check(dv.value()==-5*v && dv.derivative()==-5 && dt.derivative()==-4*v,"template original schema partials");
    }
    check(parsed("x2*x2",std::array{9.,8.,3.},2).derivative()==6,"higher original target index");
    const auto x=Expression::variable(0);
    for(auto kind:{K::Add,K::Subtract,K::Multiply,K::Divide}) {
        const auto direct=Expression::binary(kind,x,Expression::constant(2,1));
        equal(ordinary(direct,std::array{.5},0),parsed(direct.toString(),std::array{.5}));
    }
    for(const auto& r:derivativeReferences) {
        const auto direct=Expression::unary(r.kind,x);
        const auto a=ordinary(direct,std::array{r.input},0);
        check(a.derivativeAvailable(),"independent reference finite derivative");
        const double ulp=std::abs(std::nextafter(r.expected,INFINITY)-r.expected);
        check(std::abs(a.derivative()-r.expected)<=4*ulp,"independent analytic high precision reference (4 ulps)");
        equal(a,parsed(direct.toString(),std::array{r.input}));
    }
    for(unsigned n=0;n<=8;++n)for(double value:{-2.,-.5,-0.,0.,.5,2.}) {
        const auto power=Expression::power(x,n);
        const auto d=ordinary(power,std::array{value},0);
        double expected=n?static_cast<double>(n):0.;
        for(unsigned k=1;k<n;++k)expected*=value;
        check(d.derivative()==expected,"all powers analytic dyadic derivatives");
        check(d.work().primalOperations==(n>1?n-1:0) && d.work().derivativeArithmetic==(n>1?3*(n-1):0),"power recurrence exact work");
        equal(d,parsed(power.toString(),std::array{value}));
    }
    check(std::signbit(parsed("powi(x0,2)",std::array{-0.}).derivative()),"even power negative-zero derivative");
    check(!std::signbit(parsed("powi(x0,3)",std::array{-0.}).derivative()),"odd power positive-zero derivative");
}
static void boundaries() {
    const double tiny=std::numeric_limits<double>::denorm_min(), huge=std::numeric_limits<double>::max();
    for(double zero:{0.,-0.}) {
        const auto a=parsed("sqrt(x0*x0)",std::array{zero});
        check(a.status()==S::Unavailable && a.value()==0 && a.derivativeFailure()==F::SingularRule,"active sqrt zero singular");
        check(parsed("sqrt(x0)",std::array{zero}).derivativeFailure()==F::SingularRule,"both sqrt signed zeros");
        check(parsed("sqrt(powi(x0,4))",std::array{zero}).status()==S::Unavailable,"conservative active path");
        check(parsed("sqrt(x1)+x0",std::array{2.,zero}).derivative()==1,"inactive sqrt not singular");
        check(parsed("powi(sqrt(x0),0)",std::array{zero}).status()==S::Unavailable,"power zero preserves unavailable active child");
        check(parsed("0*sqrt(x0)",std::array{zero}).status()==S::Unavailable,"zero factor retains unavailable");
        check(parsed("1/x0",std::array{zero}).status()==S::PrimalInvalidity,"zero denominator");
        check(bits(parsed("sin(x0)",std::array{zero}).value())==bits(zero),"sin signed zero primal");
        check(parsed("cos(x0)",std::array{zero}).derivative()==0,"cos signed zero derivative");
    }
    check(std::signbit(parsed("(-0.0)*x0",std::array{1.}).derivative()),"negative zero derivative");
    check(parsed("x0*4.9406564584124654e-324",std::array{1.}).derivative()==tiny,"subnormal derivative");
    check(parsed("sqrt(x0)",std::array{tiny}).derivativeAvailable(),"positive subnormal sqrt");
    check(parsed("log(x0)",std::array{tiny}).status()==S::Unavailable,"valid log but derivative overflows");
    for(double a:{-1.,0.,-0.})check(parsed("log(x0)",std::array{a}).status()==S::PrimalInvalidity,"log domain");
    for(double a:{1e308,1e-308}) {
        const auto d=parsed("x0/x1",std::array{a,a},1);
        check(d.derivativeAvailable() && d.derivative()==-1/a,"quotient avoids squared denominator");
    }
    check(parsed("1.7976931348623157e308*(2*x0)",std::array{0.}).status()==S::Unavailable,"required derivative product overflow");
    check(parsed("sqrt(x0)-sqrt(x0)",std::array{0.}).status()==S::Unavailable,"cancellation retains unavailable active child");
    check(parsed("sin(x0)",std::array{0.}).derivative()==1,"sin zero analytic");
    check(parsed("exp(x0)",std::array{0.}).derivative()==1,"exp zero analytic");
    check(parsed("exp(x0)",std::array{1000.}).status()==S::PrimalInvalidity,"exp primal overflow");
    const auto under=parsed("exp(x0)",std::array{-1000.});check(under.value()==0 && under.derivative()==0,"valid exp underflow");
    check(std::signbit(parsed("exp((-1)*x0)",std::array{1000.}).derivative()),"negative-zero underflow derivative");
    const auto overflow=parsed("1.7976931348623157e308*x0+1.7976931348623157e308*x0",std::array{0.});
    check(overflow.status()==S::Unavailable && overflow.value()==0 && overflow.derivativeFailure()==F::NonfiniteArithmetic,"valid primal nonfinite derivative");
    check(parsed("0*(1.7976931348623157e308*x0+1.7976931348623157e308*x0)",std::array{0.}).status()==S::Unavailable,"derivative overflow not masked");
    const auto later=parsed("sqrt(x0)+log(-1)",std::array{0.});
    check(later.status()==S::PrimalInvalidity && later.derivativeFailure()==F::SingularRule && later.primalFailedNode()>later.derivativeFailedNode(),"derivative failure does not hide later primal error");
    for(auto text:{"0*log(x0)","powi(log(x0),0)","powi(x0*x0,0)"})
        check(parsed(text,std::array{-huge}).status()==S::PrimalInvalidity,"invalid child not masked by zero parent");
    // Every inactive scientific subtree still executes its ordinary domain checks.
    check(parsed("sqrt(x1)+x0",std::array{2.,-1.}).status()==S::PrimalInvalidity,"inactive invalid primal retained");
}
static void resources() {
    for(auto kind:{K::Sin,K::Cos,K::Exp,K::Log,K::Sqrt}) {
        const auto e=Expression::unary(kind,Expression::variable(0,2));
        const std::array inputs{.5,2.};const auto active=ordinary(e,inputs,0);
        const auto& w=active.work();
        check(w.primalOperations==1 && w.derivativeMathCalls==((kind==K::Sin || kind==K::Cos)?1u:0u),"native-call attribution");
        check(w.derivativeArithmetic==((kind==K::Cos || kind==K::Sqrt)?2u:1u),"unary derivative arithmetic attribution");
        const auto inactive=ordinary(e,inputs,1);
        check(inactive.work().derivativeArithmetic==0 && inactive.work().derivativeMathCalls==0 && bits(inactive.derivative())==bits(0.),"inactive path avoids derivative work");
        for(std::size_t allowance=0;allowance<=w.operations()+1;++allowance) {
            auto limits=DerivativeLimits{};limits.maxOperations=allowance;
            const auto r=evaluatePartial(e,inputs,0,limits);
            check(r.work().operations()<=allowance,"native work admission never exceeds allowance");
            if(allowance<w.operations())check(r.status()==S::ResourceLimit,"native work boundary");
            else equal(r,active);
        }
    }
    const auto e=parseExpression("sin(x0)+powi(x1,8)",2);const std::array bindings{.5,.5};
    const auto base=ordinary(e,bindings,1);const auto w=base.work();
    DerivativeLimits limits{w.operations(),w.nodesVisited,w.bindingsValidated,w.workspaceBytes};
    equal(base,evaluatePartial(e,bindings,1,limits));
    auto above=limits;++above.maxOperations;++above.maxNodeVisits;++above.maxBindings;++above.maxWorkspaceBytes;
    equal(base,evaluatePartial(e,bindings,1,above));
    for(int k=0;k<4;++k) {
        auto low=limits;
        if(k==0)--low.maxOperations;
        if(k==1)--low.maxNodeVisits;
        if(k==2)--low.maxBindings;
        if(k==3)--low.maxWorkspaceBytes;
        const auto r=evaluatePartial(e,bindings,1,low);
        check(r.status()==S::ResourceLimit && !r.valueAvailable() && !r.derivativeAvailable(),"below required resource");
        check(r.exhaustedResource()==(k==0?DerivativeResource::Operations:k==1?DerivativeResource::NodeVisits:k==2?DerivativeResource::Bindings:DerivativeResource::Workspace),"resource diagnostic");
        throws<std::logic_error>([&]{(void)r.value();});throws<std::logic_error>([&]{(void)r.derivative();});
        check(r.work().operations()<=low.maxOperations,"resource operation ceiling");
    }
    for(DerivativeLimits zero: {DerivativeLimits{0,100,100,10000},DerivativeLimits{100,0,100,10000},DerivativeLimits{100,100,0,10000},DerivativeLimits{100,100,100,0}})
        check(evaluatePartial(e,bindings,1,zero).status()==S::ResourceLimit,"zero resource allowances");
    const auto fail=parseExpression("sqrt(x0)+x0",1);auto low=DerivativeLimits{};low.maxOperations=1;
    const auto r=evaluatePartial(fail,std::array{0.},0,low);
    check(r.status()==S::ResourceLimit && r.derivativeFailure()==F::SingularRule,"resource wins over unavailable");
    throws<std::invalid_argument>([&]{(void)evaluatePartial(e,bindings,2);});
    throws<std::invalid_argument>([&]{(void)evaluatePartial(e,std::array{1.},0);});
    throws<std::invalid_argument>([&]{(void)evaluatePartial(e,std::array{1.,std::numeric_limits<double>::infinity()},0);});
    throws<std::invalid_argument>([]{(void)evaluatePartial(Expression::constant(1),{},0);});
    Expression moved=e;Expression owner=std::move(moved);
    throws<std::logic_error>([&]{(void)evaluatePartial(moved,bindings,0);});
    const auto unavailable=parsed("sqrt(x0)",std::array{0.});
    throws<std::logic_error>([&]{(void)unavailable.derivative();});
    // Unlimited machine counts must not wrap; a small real expression allocates only its plan.
    const auto maximum=std::numeric_limits<std::size_t>::max();
    equal(base,evaluatePartial(e,bindings,1,{maximum,maximum,maximum,maximum}));
    const auto before=e.toString();auto destination=base;fault::fail=true;
    throws<std::bad_alloc>([&]{destination=evaluatePartial(e,bindings,0);});
    equal(destination,base);
    check(e.toString()==before,"operational failure preserves expression");
    const auto saved=evaluatePartial(owner,bindings,0);
    (void)evaluatePartial(e,std::array{2.,3.},1);
    equal(saved,evaluatePartial(e,bindings,0)); // owned results survive other scratch lifetimes
}
static void generated() {
    std::mt19937 rng(711);
    std::vector<Expression> pool{Expression::variable(0,2),Expression::variable(1,2),Expression::constant(-0.,2),Expression::constant(.5,2)};
    for(int i=0;i<200;++i) {
        const auto a=pool[rng()%pool.size()],b=pool[rng()%pool.size()];
        const auto kind=static_cast<K>(static_cast<unsigned>(K::Add)+rng()%10);
        auto e=expressionArity(kind)==2?Expression::binary(kind,a,b):kind==K::Powi?Expression::power(a,rng()%9):Expression::unary(kind,a);
        for(std::size_t t=0;t<2;++t) {
            const std::array input{i%3==0?0.:.125,-.25};
            equal(ordinary(e,input,t),ordinary(parseExpression(e.toString(),2),input,t));
        }
        if(e.nodeCount()<25 && e.depth()<6){pool.push_back(e);if(pool.size()>24)pool.erase(pool.begin()+4);}
    }
    // Deep iterative propagation (no recursive derivative call stack).
    std::vector<ExpressionNode> nodes{ExpressionNode::variable(0)};
    for(int i=0;i<20000;++i)nodes.push_back(ExpressionNode::power(1));
    const Expression deep(nodes,1,{20001,20001});
    check(ordinary(deep,std::array{2.},0).derivative()==1,"deep bounded iterative traversal");
}
static void overlap() {
    const auto expression=parseExpression("sqrt(x0*x0)+sin(x1)",2);
    std::vector<DerivativeResult> reference;
    for(std::size_t i=0;i<13;++i)reference.push_back(evaluatePartial(expression,std::array{i%3? .5:0.,.125},i%2));
    for(unsigned n:{1u,2u,4u}) {
        ExecutionContext execution({n});std::barrier gate(static_cast<std::ptrdiff_t>(n));
        std::vector<DerivativeResult> results(13,reference.front());
        execution.run(results.size(),[&](std::size_t i){
            // All N workers must enter separate live evaluator invocations before any returns.
            if(i<n)fault::gate=&gate;
            for(int j=0;j<16;++j)results[i]=evaluatePartial(expression,std::array{i%3? .5:0.,.125},i%2);
        });
        check(execution.peakTasks()==n,"real overlap and worker ceiling");
        for(std::size_t i=0;i<results.size();++i)equal(results[i],reference[i]);
    }
}
int main(int argc,char**) {
    try {if(argc>1)overlap();else{analytic();boundaries();resources();generated();overlap();}
        std::cout<<"derivative contracts passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
