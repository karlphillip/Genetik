#include <ExpressionParsing.hpp>
#include <TypedExpressionParsing.hpp>
#include <TypedConstantFitting.hpp>
#include <array>
#include <barrier>
#include <bit>
#include <cstdlib>
#include <future>
#include <iostream>
#include <new>
#include <locale>
#include <random>

// Test-only allocation fault injection and gate: no library allocator API needed.
// Each thread exclusively owns its controls; the optional gate proves overlap
// INSIDE parsing, rather than merely before starting two short calls.
namespace fault {thread_local long after=-1;thread_local std::barrier<>* gate=nullptr;}
void* operator new(std::size_t n) {
    if(fault::gate){auto* g=fault::gate;fault::gate=nullptr;g->arrive_and_wait();}
    if(fault::after==0){fault::after=-1;throw std::bad_alloc();}
    if(fault::after>0)--fault::after;
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

static void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
static std::uint64_t bits(double d){return std::bit_cast<std::uint64_t>(d);}
using K=ExpressionKind;using T=TypedExpressionKind;using C=ExpressionParseCategory;
static void same(const TypedExpression& a,const TypedExpression& b) {
    check(a.schema()==b.schema()&&a.shape()==b.shape()&&a.toString()==b.toString(),"typed schema/shape/text");
    const auto x=a.nodes(),y=b.nodes();check(x.size()==y.size(),"typed node count");
    for(std::size_t i=0;i<x.size();++i) {
        check(x[i].kind()==y[i].kind(),"typed kind");
        if(x[i].kind()==T::Variable)check(x[i].variableIndex()==y[i].variableIndex(),"typed variable");
        if(x[i].kind()==T::Powi)check(x[i].exponent()==y[i].exponent(),"typed exponent");
        if(x[i].kind()==T::Constant) {
            const auto& u=x[i].constantValue();const auto& v=y[i].constantValue();check(u.shape()==v.shape(),"constant rank");
            for(std::size_t j=0;j<u.coefficients().size();++j)check(bits(u.coefficients()[j])==bits(v.coefficients()[j]),"coefficient bits");
        }
    }
    check(a.arithmeticCost()==b.arithmeticCost()&&a.temporaryCoefficients()==b.temporaryCoefficients(),"typed plans");
}
static void round(const Expression& a,std::span<const double> values={}) {
    const auto b=parseExpression(a.toString(),a.variableCount(),{},a.limits());
    check(a.sameRepresentation(b)&&a.toString()==b.toString(),"scalar structural round trip");
    const auto x=a.evaluate(values),y=b.evaluate(values);
    check(x.status()==y.status()&&x.failedNode()==y.failedNode()&&x.arithmeticOperations()==y.arithmeticOperations(),"scalar result classification/work");
    if(x.valid())check(bits(x.value())==bits(y.value()),"scalar exact result");
}
static void round(const TypedExpression& a,std::span<const TypedValue> values={}) {
    const auto b=parseTypedExpression(a.toString(),a.schema(),a.shape(),{},a.limits());same(a,b);
    const auto x=a.evaluate(values),y=b.evaluate(values);
    check(x.status()==y.status()&&x.failedNode()==y.failedNode()&&x.arithmeticOperations()==y.arithmeticOperations(),"typed result classification/work");
    if(x.valid()){const auto u=x.value().coefficients(),v=y.value().coefficients();for(std::size_t i=0;i<u.size();++i)check(bits(u[i])==bits(v[i]),"typed exact result");}
}
template<class F> static void rejects(F f,C category) {
    bool threw=false;try{f();}catch(const ExpressionParseError& e){threw=true;check(e.category()==category,"parse error category");check(std::string(e.what()).size()<180,"bounded diagnostic");}
    check(threw,"expected parse error");
}
static void scalar() {
    for(double d:{0.,-0.,-2.,.1,std::numeric_limits<double>::denorm_min(),std::numeric_limits<double>::min(),std::numeric_limits<double>::max(),-std::numeric_limits<double>::max()})round(Expression::constant(d));
    const auto x=Expression::variable(2,3);const std::array bindings{1.,2.,.5};round(x,bindings);
    for(auto k:{K::Add,K::Subtract,K::Multiply,K::Divide})round(Expression::binary(k,x,Expression::constant(.1,3)),bindings);
    for(auto k:{K::Log,K::Sin,K::Cos,K::Exp,K::Sqrt})round(Expression::unary(k,x),bindings);
    for(unsigned e:{0u,1u,2u,3u,8u}){round(Expression::power(x,e),bindings);round(Expression::power(Expression::constant(-0.),e));}
    std::mt19937_64 literalRng(17);
    for(int i=0;i<500;++i){const double d=std::bit_cast<double>(literalRng());if(std::isfinite(d))round(Expression::constant(d));}
    struct Comma:std::numpunct<char>{char do_decimal_point() const override{return ',';}};
    const auto previous=std::locale();std::locale::global(std::locale(previous,new Comma));
    check(bits(parseExpression("0.10000000000000001",0).evaluate({}).value())==bits(.1),"locale independent decimal");
    std::locale::global(previous);
    round(Expression::unary(K::Log,Expression::constant(-1)));
    round(Expression::binary(K::Divide,Expression::constant(1),Expression::constant(-0.)));
    round(Expression::power(Expression::binary(K::Multiply,Expression::constant(1e308),Expression::constant(1e308)),0));
    check(parseExpression("1+2*3-8/4",0).evaluate({}).value()==5,"precedence");
    check(parseExpression("8/4/2",0).evaluate({}).value()==1,"left associativity");
    check(parseExpression("8/(4/2)",0).evaluate({}).value()==4,"grouping");
    check(parseExpression("1e16+(-1e16+1)",0).evaluate({}).value()==0,"rounding grouping");
    check(parseExpression("(1e16+-1e16)+1",0).evaluate({}).value()==1,"explicit other grouping");
    check(parseExpression(" \r\n +.5E+1 \t",0).evaluate({}).value()==5,"decimal and whitespace");
    check(parseExpression("1+1",0).nodes().size()==3,"independent constants not folded");
}
static void typed() {
    for(auto value:{TypedValue::scalar(-0.),TypedValue::vector(std::array{-0.}),TypedValue::matrix(1,1,std::array{-0.}),TypedValue::matrix(2,3,std::array{1.,2.,3.,4.,5.,6.})})round(TypedExpression::constant(value));
    const std::array schema{TypedShape::matrix(2,3),TypedShape::vector(3),TypedShape::vector(2),TypedShape::scalar()};
    const std::array values{TypedValue::matrix(2,3,std::array{1.,2.,3.,-1.,0.,4.}),TypedValue::vector(std::array{2.,-1.,3.}),TypedValue::vector(std::array{5.,-2.}),TypedValue::scalar(.5)};
    const auto A=TypedExpression::variable(0,schema),x=TypedExpression::variable(1,schema),b=TypedExpression::variable(2,schema),s=TypedExpression::variable(3,schema);
    const auto product=TypedExpression::binary(T::MatVec,A,x);round(TypedExpression::binary(T::Add,product,b),values);
    for(auto k:{T::Add,T::Subtract})round(TypedExpression::binary(k,b,b),values);
    round(TypedExpression::binary(T::Multiply,s,product),values);round(TypedExpression::binary(T::Multiply,A,s),values);
    round(TypedExpression::binary(T::Divide,s,s),values);round(TypedExpression::binary(T::Dot,x,x),values);
    round(TypedExpression::binary(T::MatMul,A,TypedExpression::unary(T::Transpose,A)),values);
    for(auto k:{T::Log,T::Sin,T::Cos,T::Exp,T::Sqrt})round(TypedExpression::unary(k,s),values);
    round(TypedExpression::power(s,8),values);
    same(parseTypedExpression("matvec(x0,x1)+x2",schema,TypedShape::vector(2)),TypedExpression::binary(T::Add,product,b));
    const auto e=parseTypedExpression("matvec(x0,x1)",schema,TypedShape::vector(2));
    check(e.evaluate(values,{0,100}).status()==TypedEvaluationStatus::ResourceLimit,"post-import evaluation resource stop");
    rejects([&]{(void)parseTypedExpression("sin(x0)",schema,TypedShape::scalar());},C::Shape);
    rejects([&]{(void)parseTypedExpression("x1+x2",schema,TypedShape::vector(2));},C::Shape);
    rejects([&]{(void)parseTypedExpression("scalar(1)",{},TypedShape::vector(1));},C::Shape);
    rejects([&]{(void)parseTypedExpression("vector[1](1)",{},TypedShape::matrix(1,1));},C::Shape);
    for(auto text:{"matrix[0,2](1,2)","vector[0]()"})rejects([&]{(void)parseTypedExpression(text,{},TypedShape::scalar());},C::Shape);
    for(auto text:{"matrix[18446744073709551615,2](1)","vector[999999](1)"})rejects([&]{(void)parseTypedExpression(text,{},TypedShape::scalar());},C::Resource);
    for(auto text:{"vector[2](1)","matrix[1,1](1,2)","vector[2](1,,2)"}) {
        bool threw=false;try{(void)parseTypedExpression(text,{},TypedShape::scalar());}catch(const ExpressionParseError&){threw=true;}check(threw,"payload count malformed");
    }
}
static void malformedAndLimits() {
    const std::array<std::string_view,28> bad{"","()","1 2","x0=1","1;","1,2","2x0","-x0","--1","1e","1e-9999","1e9999","0x1p0","nan","inf","x01","x1","foo(1)","sin(1,2)","sin()","powi(1,x0)","powi(1,9)","powi(1,-1)","powi(1,1.0)","(1+2","sqrt(1))","[1,2]","1/*2"};
    for(auto text:bad){bool threw=false;try{(void)parseExpression(text,1);}catch(const ExpressionParseError& e){threw=true;check(e.offset()<=text.size(),"error offset in source");}check(threw,"malformed corpus accepted");}
    ExpressionParseLimits p;p.maxSourceBytes=1;check(parseExpression("1",0,p).nodeCount()==1,"byte boundary");rejects([&]{(void)parseExpression("12",0,p);},C::Resource);
    p={};p.maxTokens=3;(void)parseExpression("1+2",0,p);p.maxTokens=2;rejects([&]{(void)parseExpression("1+2",0,p);},C::Resource);
    p={};p.maxTokenBytes=2;(void)parseExpression("12",0,p);rejects([&]{(void)parseExpression("123",0,p);},C::Resource);
    p={};p.maxNesting=2;(void)parseExpression("((1))",0,p);rejects([&]{(void)parseExpression("(((1)))",0,p);},C::Resource);
    p={};p.maxConstantNodes=1;(void)parseExpression("1",0,p);rejects([&]{(void)parseExpression("1+2",0,p);},C::Resource);
    p={};p.maxConstantCoefficients=2;(void)parseTypedExpression("vector[2](1,2)",{},TypedShape::vector(2),p);rejects([&]{(void)parseTypedExpression("vector[3](1,2,3)",{},TypedShape::vector(3),p);},C::Resource);
    p={};p.maxWorkspaceBytes=1;rejects([&]{(void)parseExpression("1",0,p);},C::Resource);
    // The parser workspace bound is a deterministic requested-payload charge.
    p={};p.maxWorkspaceBytes=64*1024+sizeof(ExpressionNode)+sizeof(std::size_t)+4*sizeof(double);(void)parseExpression("1",0,p);--p.maxWorkspaceBytes;rejects([&]{(void)parseExpression("1",0,p);},C::Resource);
    p={};p.maxVariables=1;bool config=false;try{(void)parseExpression("1",2,p);}catch(const ExpressionParseError&){throw;}catch(const std::invalid_argument&){config=true;}check(config,"schema API configuration distinct");
    for(std::size_t nesting:{0u,129u}){p={};p.maxNesting=nesting;config=false;try{(void)parseExpression("1",0,p);}catch(const ExpressionParseError&){throw;}catch(const std::invalid_argument&){config=true;}check(config,"unsafe parser nesting configuration");}
    ExpressionLimits e{3,2,1,1};(void)parseExpression("1+1",0,{},e);e.maxNodes=2;rejects([&]{(void)parseExpression("1+1",0,{},e);},C::Resource);e.maxNodes=3;e.maxDepth=1;rejects([&]{(void)parseExpression("1+1",0,{},e);},C::Resource);e={3,2,1,0};rejects([&]{(void)parseExpression("1+1",0,{},e);},C::Resource);
    TypedExpressionLimits t;t.maxConstantNodes=0;rejects([&]{(void)parseTypedExpression("scalar(1)",{},TypedShape::scalar(),{},t);},C::Resource);
    t={};t.maxArithmeticOperations=0;rejects([&]{(void)parseTypedExpression("1+2",{},TypedShape::scalar(),{},t);},C::Resource);
    t={};t.maxTemporaryCoefficients=1;rejects([&]{(void)parseTypedExpression("vector[2](1,2)",{},TypedShape::vector(2),{},t);},C::Resource);
    e={3,2,1,10};(void)parseExpression("1",0,{},e);rejects([&]{(void)parseExpression("1.01",0,{},e);},C::Resource);
    t={};t.values.maxVectorLength=2;(void)parseTypedExpression("vector[2](1,2)",{},TypedShape::vector(2),{},t);rejects([&]{(void)parseTypedExpression("vector[3](1,2,3)",{},TypedShape::scalar(),{},t);},C::Resource);
    t={};t.values.maxMatrixRows=2;t.values.maxMatrixColumns=2;(void)parseTypedExpression("matrix[2,2](1,2,3,4)",{},TypedShape::matrix(2,2),{},t);rejects([&]{(void)parseTypedExpression("matrix[3,1](1,2,3)",{},TypedShape::scalar(),{},t);},C::Resource);rejects([&]{(void)parseTypedExpression("matrix[1,3](1,2,3)",{},TypedShape::scalar(),{},t);},C::Resource);
    t={};t.values.maxCoefficients=2;(void)parseTypedExpression("vector[2](1,2)",{},TypedShape::vector(2),{},t);rejects([&]{(void)parseTypedExpression("vector[3](1,2,3)",{},TypedShape::scalar(),{},t);},C::Resource);
    t={};t.maxConstantNodes=1;t.maxConstantCoefficients=2;(void)parseTypedExpression("vector[2](1,2)",{},TypedShape::vector(2),{},t);rejects([&]{(void)parseTypedExpression("vector[3](1,2,3)",{},TypedShape::vector(3),{},t);},C::Resource);rejects([&]{(void)parseTypedExpression("1+1",{},TypedShape::scalar(),{},t);},C::Resource);
    p={};p.maxConstantNodes=0;(void)parseExpression("x0",1,p);rejects([&]{(void)parseExpression("1",1,p);},C::Resource);
    p={};p.maxConstantCoefficients=0;rejects([&]{(void)parseTypedExpression("scalar(1)",{},TypedShape::scalar(),p);},C::Resource);
    rejects([]{(void)parseExpression(std::string(300,'1'),0);},C::Resource);
    rejects([]{(void)parseExpression(std::string(200,'(')+"1"+std::string(200,')'),0);},C::Resource);
}
static void ownershipAndFailures() {
    std::string text="(-0.0)+x0";auto e=parseExpression(text,1);text.assign(text.size(),'!');check(e.toString()=="((-0.0) + x0)","source alias safety");
    std::vector<TypedShape> schema{TypedShape::vector(2)};auto t=parseTypedExpression("x0 + vector[2](1,2)",schema,TypedShape::vector(2));schema[0]=TypedShape::scalar();check(t.schema()[0]==TypedShape::vector(2),"schema alias safety");
    auto copy=t;round(copy,std::array{TypedValue::vector(std::array{2.,3.})});
    const auto before=e.toString();try{e=parseExpression("x99",1);}catch(const ExpressionParseError&){}check(e.toString()==before,"failed parse preserves destination");
    std::size_t failures=0;
    for(long n=0;n<18;++n) {
        fault::after=n;
        try{e=parseExpression("sin(x0)+(1.0*2.0)",1);}catch(const std::bad_alloc&){++failures;check(e.toString()==before,"allocation failure preserves destination");}
        fault::after=-1;e=parseExpression(before,1);
    }
    check(failures>=3,"allocation failure locations exercised");
    const auto typedBefore=t.toString();failures=0;
    for(long n=0;n<28;++n) {
        fault::after=n;
        try{t=parseTypedExpression("vector[2](1,2)+x0",t.schema(),t.shape());}catch(const std::bad_alloc&){++failures;check(t.toString()==typedBefore,"typed allocation failure preserves destination");}
        fault::after=-1;t=copy;
    }
    check(failures>=3,"typed allocation failures exercised");
}
static void fitting() {
    const auto x=Expression::variable(0);const auto direct=Expression::binary(K::Multiply,Expression::constant(1,1),x);
    const auto parsed=parseExpression(direct.toString(),1);auto p=RegressionProblem::create({{{-1},-1.75},{{0},0},{{1},1.75}},{15,5,8});
    TrainingObjective a(p,256),b(p,256);const auto f=fitConstants(direct,a),g=fitConstants(parsed,b);
    check(f.expression.sameRepresentation(g.expression)&&f.loss==g.loss&&f.loss.score()==0&&a.work().lossAttempts==b.work().lossAttempts&&a.work().rowAttempts==b.work().rowAttempts,"parsed scalar fitting/work exact");
    auto progress=Expression::binary(K::Add,Expression::constant(0),Expression::unary(K::Sqrt,Expression::constant(0)));
    auto progressProblem=RegressionProblem::create({{{},1.2}},{15,5,8});TrainingObjective pa(progressProblem,32),pb(progressProblem,32);
    ConstantFitConfig oneSweep;oneSweep.maxSweeps=1;oneSweep.maxLossAttempts=32;
    auto pf=fitConstants(progress,pa,oneSweep),pg=fitConstants(parseExpression(progress.toString(),0),pb,oneSweep);
    check(pf.expression.sameRepresentation(pg.expression)&&pf.loss==pg.loss&&pb.work().invalid>0&&pb.work().accepted>0&&pa.work().lossAttempts==pb.work().lossAttempts,"parsed invalid probe retains accepted work");
    const std::array schema{TypedShape::vector(2)};
    const auto typed=TypedExpression::binary(T::Add,TypedExpression::variable(0,schema),TypedExpression::constant(TypedValue::vector(std::array{1.,1.}),schema));
    const auto imported=parseTypedExpression(typed.toString(),schema,TypedShape::vector(2));
    auto tp=TypedRegressionProblem::create(schema,TypedShape::vector(2),{{{TypedValue::vector(std::array{0.,0.})},TypedValue::vector(std::array{1.75,-.5})}});
    TypedTrainingObjective ta(tp,256),tb(tp,256);auto u=fitTypedConstants(typed,ta),v=fitTypedConstants(imported,tb);same(u.expression,v.expression);
    check(u.loss==v.loss&&u.loss.score()==0&&ta.work().lossAttempts==tb.work().lossAttempts&&ta.arithmeticOperations()==tb.arithmeticOperations(),"parsed aggregate fitting/work exact");
}
static void generated() {
    std::mt19937 rng(2027);std::vector<Expression> pool{Expression::variable(0),Expression::constant(-0.,1),Expression::constant(.1,1)};
    for(int i=0;i<200;++i) {
        const auto& a=pool[rng()%pool.size()];Expression e=a;
        if(i%3==0)e=Expression::unary(std::array{K::Sin,K::Cos,K::Log,K::Exp,K::Sqrt}[rng()%5],a);
        else if(i%3==1)e=Expression::power(a,rng()%9);
        else e=Expression::binary(std::array{K::Add,K::Subtract,K::Multiply,K::Divide}[rng()%4],a,pool[rng()%pool.size()]);
        round(e,std::array{.25});
        if(e.depth()<5 && e.nodeCount()<25){pool.push_back(e);if(pool.size()>30)pool.erase(pool.begin()+3);}
        const auto s=e.toString();for(auto bad:{s+"?",s+",",s+" x0"}){bool failed=false;try{(void)parseExpression(bad,1);}catch(const ExpressionParseError&){failed=true;}check(failed,"bounded malformed generated corpus");}
    }
    const std::array schema{TypedShape::vector(2)};auto a=TypedExpression::variable(0,schema);auto b=TypedExpression::constant(TypedValue::vector(std::array{.1,-0.}),schema);
    for(int i=0;i<50;++i) {
        const auto e=TypedExpression::binary(i%2?T::Add:T::Subtract,a,b);round(e,std::array{TypedValue::vector(std::array{.5,2.})});
        round(TypedExpression::binary(T::Dot,e,b),std::array{TypedValue::vector(std::array{.5,2.})});
        b=TypedExpression::constant(TypedValue::vector(std::array{static_cast<double>(rng()%10)/8.,-0.}),schema);
    }
}
static void overlap() {
    const std::string source="sin(x0)+powi(x1,3)";
    const std::array schema{TypedShape::vector(2)};const std::string aggregate="x0+vector[2](1,-0.0)";
    std::barrier gate(2);
    auto work=[&] {
        fault::gate=&gate;
        for(int i=0;i<100;++i) {
            {if(i==0)fault::gate=&gate;auto e=parseTypedExpression(aggregate,schema,TypedShape::vector(2));check(e.evaluate(std::array{TypedValue::vector(std::array{2.,3.})}).value().at(0)==3,"overlap typed");}
            {auto e=parseExpression(source,2);check(e.evaluate(std::array{0.,2.}).value()==8,"overlap scalar");}
        }
    };
    auto a=std::async(std::launch::async,work),b=std::async(std::launch::async,work);a.get();b.get();
}
int main(int argc,char**) {
    try {if(argc>1){overlap();}else{scalar();typed();malformedAndLimits();ownershipAndFailures();fitting();generated();overlap();}std::cout<<"parsing contracts passed\n";}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
