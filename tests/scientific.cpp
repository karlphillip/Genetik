#include <TypedSymbolicRegression.hpp>
#include <array>
#include <bit>
#include <cfenv>
#include <cerrno>
#include <iostream>
#include <thread>
#include <barrier>
#include <sstream>
#include "fixtures/scientific/references.hpp"
using K=ExpressionKind;using T=TypedExpressionKind;
static void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
template<class E=std::exception,class F>static void rejects(F f){try{f();}catch(const E&){return;}throw std::runtime_error("expected API/resource rejection");}
static T typed(K k){switch(k){case K::Sin:return T::Sin;case K::Cos:return T::Cos;case K::Exp:return T::Exp;case K::Sqrt:return T::Sqrt;default:throw std::logic_error("bad fixture");}}
static bool bits(double a,double b){return std::bit_cast<std::uint64_t>(a)==std::bit_cast<std::uint64_t>(b);}
static void arithmetic(){
    const auto x=Expression::variable(0);const std::array schema{TypedShape::scalar()};const auto tx=TypedExpression::variable(0,schema);
    for(const auto& r:scientificReferences){
        const auto a=Expression::unary(r.kind,x).evaluate(std::array{r.input});
        const auto b=TypedExpression::unary(typed(r.kind),tx).evaluate(std::array{TypedValue::scalar(r.input)});
        // 4 ulps at the reference's scale, with a denormal floor; no relative-only zero test.
        const double tolerance=4*std::max(std::abs(std::nextafter(r.expected,std::numeric_limits<double>::infinity())-r.expected),std::numeric_limits<double>::denorm_min());
        check(a.valid() && b.valid() && std::abs(a.value()-r.expected)<=tolerance,"independent libm reference");
        check(bits(a.value(),b.value().scalarValue()),"shared scalar/typed semantics");
    }
    for(double zero:{0.,-0.}){
        for(auto k:{K::Sin,K::Sqrt})check(bits(Expression::unary(k,x).evaluate(std::array{zero}).value(),zero),"signed zero native function");
        check(Expression::unary(K::Cos,x).evaluate(std::array{zero}).value()==1,"cos zeros");
        check(Expression::unary(K::Exp,x).evaluate(std::array{zero}).value()==1,"exp zeros");
        for(unsigned e:{0u,1u,2u,3u,8u}){
            const auto p=Expression::power(x,e).evaluate(std::array{zero});
            check(bits(p.value(),e==0?1.:e%2?zero:0.),"power signed zero parity");
        }
    }
    errno=EDOM;std::feraiseexcept(FE_INVALID);
    check(Expression::unary(K::Sqrt,x).evaluate(std::array{4.}).value()==2,"stale errno/flags do not invalidate");std::feclearexcept(FE_ALL_EXCEPT);
    for(auto kind:{K::Sin,K::Cos,K::Exp,K::Sqrt})for(double input:{-1000.,-0.,0.,std::numeric_limits<double>::denorm_min(),1000.}) {
        const auto a=Expression::unary(kind,x).evaluate(std::array{input});
        const auto b=TypedExpression::unary(typed(kind),tx).evaluate(std::array{TypedValue::scalar(input)});
        check(a.valid()==b.valid(),"scalar/typed boundary classification");
        if(a.valid())check(bits(a.value(),b.value().scalarValue()),"scalar/typed boundary bits");
    }
    const auto typedBad=TypedExpression::binary(T::Multiply,TypedExpression::constant(TypedValue::scalar(1e308),schema),TypedExpression::constant(TypedValue::scalar(1e308),schema));
    check(!TypedExpression::power(typedBad,0).evaluate(std::array{TypedValue::scalar(0)}).valid(),"typed invalid child under power zero");
    check(!Expression::unary(K::Sqrt,x).evaluate(std::array{-1.}).valid(),"sqrt negative domain");
    check(!Expression::unary(K::Exp,x).evaluate(std::array{1000.}).valid(),"exp overflow");
    check(bits(Expression::unary(K::Exp,x).evaluate(std::array{-1000.}).value(),0.),"exp finite zero underflow");
    const auto bad=Expression::binary(K::Multiply,Expression::constant(1e308,1),Expression::constant(1e308,1));
    check(!Expression::power(bad,0).evaluate(std::array{0.}).valid(),"invalid child under exponent zero");
    check(!Expression::unary(K::Exp,bad).evaluate(std::array{0.}).valid(),"invalid child under native function");
    auto masked=Expression::binary(K::Multiply,Expression::constant(0,1),Expression::unary(K::Sqrt,Expression::constant(-1,1)));
    check(!masked.evaluate(std::array{0.}).valid(),"invalid intermediate cannot be masked");
    for(unsigned e=0;e<=8;++e){
        auto p=Expression::power(x,e);auto q=TypedExpression::power(tx,e);
        double expected=1;for(unsigned i=0;i<e;++i)expected*=-2;
        auto a=p.evaluate(std::array{-2.});auto b=q.evaluate(std::array{TypedValue::scalar(-2.)});
        check(a.value()==expected && b.value().scalarValue()==expected,"integer power independent fixture");
        check(p.arithmeticCost()==(e?e-1:0) && a.arithmeticOperations()==p.arithmeticCost() && q.arithmeticCost()==p.arithmeticCost() && b.arithmeticOperations()==p.arithmeticCost(),"exact power work");
        check(p.toString()=="powi(x0,"+std::to_string(e)+")" && p.toString()==q.toString(),"faithful exponent export");
    }
    check(!Expression::power(x,8).evaluate(std::array{1e100}).valid(),"checked intermediate power overflow");
    auto q=TypedExpression::power(tx,8);check(q.evaluate(std::array{TypedValue::scalar(2.)},{6,100}).status()==TypedEvaluationStatus::ResourceLimit,"power local work admission");
    auto limits=q.limits();limits.maxArithmeticOperations=6;rejects<TypedLimitError>([&]{(void)TypedExpression::power(tx,8,limits);});
    auto scalarLimits=x.limits();scalarLimits.maxArithmeticOperations=6;rejects<std::length_error>([&]{(void)Expression::power(x,8,scalarLimits);});
    auto overflow=q.evaluate(std::array{TypedValue::scalar(1e100)});check(!overflow.valid() && overflow.arithmeticOperations()==3,"overflow work stops after third multiplication");
    rejects<std::invalid_argument>([&]{(void)Expression::power(x,9);});rejects<std::invalid_argument>([&]{(void)TypedExpression::power(tx,99);});
    rejects<std::invalid_argument>([]{(void)ExpressionNode::unary(K::Powi);});rejects<std::invalid_argument>([]{(void)TypedExpressionNode::unary(T::Powi);});
    for(auto kind:{T::Sin,T::Cos,T::Exp,T::Sqrt,T::Powi}){
        auto vector=TypedExpression::constant(TypedValue::vector(std::array{1.,2.}));
        rejects<std::invalid_argument>([&]{if(kind==T::Powi)(void)TypedExpression::power(vector,2);else (void)TypedExpression::unary(kind,vector);});
    }
    const std::array shapes{TypedShape::scalar(),TypedShape::matrix(2,3),TypedShape::vector(3)};
    auto composition=TypedExpression::binary(T::Multiply,TypedExpression::unary(T::Exp,TypedExpression::variable(0,shapes)),
        TypedExpression::binary(T::MatVec,TypedExpression::variable(1,shapes),TypedExpression::variable(2,shapes)));
    const auto result=composition.evaluate(std::array{TypedValue::scalar(0),TypedValue::matrix(2,3,std::array{1.,2.,3.,-1.,0.,4.}),TypedValue::vector(std::array{2.,-1.,3.})});
    check(result.value().at(0)==9 && result.value().at(1)==10 && result.arithmeticOperations()==13,"exp scales rectangular matvec");
}
static void variation(){
    auto p=RegressionProblem::create({{{.25},0},{{1.},0}},{15,5,8});ExpressionGrammar g;g.operators={K::Sin,K::Cos,K::Exp,K::Sqrt,K::Powi};g.exponents={2,3};
    ExpressionVariation v(p,g);std::mt19937 random(42);std::array<bool,5> seen{};bool discreteChanged=false;
    ExpressionCandidate parent(Expression::power(Expression::variable(0),2),p);
    for(unsigned i=0;i<160;++i){auto a=v.initialize(random);auto b=v.mutation(parent,1,random);auto c=a,d=b;v.crossover(a,b,c,d,1,random);
        for(const auto* expression:{&a.expression(),&b.expression(),&c.expression(),&d.expression()})for(const auto& node:expression->nodes()){
            for(std::size_t j=0;j<g.operators.size();++j)if(node.kind()==g.operators[j])seen[j]=true;
            if(node.kind()==K::Powi)check(node.exponent()==2 || node.exponent()==3,"generated exponent subset");
        }
        if(b.text()=="powi(x0,3)")discreteChanged=true;
    }
    for(bool b:seen){check(b,"all scientific scalar productions sampled");}
    check(discreteChanged,"explicit discrete exponent mutation");
    check(parent.text()=="powi(x0,2)","parent ownership");
    auto other=Expression::power(Expression::variable(0),3);check(!parent.expression().sameRepresentation(other),"exact equality includes exponent");
    check(replaceExpressionSubtree(parent.expression(),1,other,p->limits())->toString()==other.toString(),"root crossover preserves exponent");
    auto bad=g;bad.exponents={9};rejects<std::invalid_argument>([&]{ExpressionVariation invalid(p,bad);});bad=g;bad.exponents={2,2};rejects<std::invalid_argument>([&]{ExpressionVariation invalid(p,bad);});
    const std::array schema{TypedShape::scalar()};auto tp=TypedRegressionProblem::create(schema,TypedShape::scalar(),{{{TypedValue::scalar(.25)},TypedValue::scalar(0)}});
    TypedGrammar tg;tg.operators={T::Sin,T::Cos,T::Exp,T::Sqrt,T::Powi};tg.exponents={2,3};TypedVariation tv(tp,tg);
    TypedExpressionCandidate original(TypedExpression::power(TypedExpression::variable(0,schema),2),tp);bool changed=false;std::array<bool,5> typedSeen{};
    for(unsigned i=0;i<100;++i){auto a=tv.initialize(random);auto b=tv.mutation(original,1,random);auto c=a,d=b;tv.crossover(a,b,c,d,1,random);
        for(const auto* e:{&a.expression(),&b.expression(),&c.expression(),&d.expression()})for(const auto& node:e->nodes()){
            for(std::size_t j=0;j<tg.operators.size();++j)if(node.kind()==tg.operators[j])typedSeen[j]=true;
            if(node.kind()==T::Powi)check(node.exponent()==2 || node.exponent()==3,"typed exponent subset");
        }
        changed|=b.text()=="powi(x0,3)";
    }
    for(bool b:typedSeen){check(b,"typed scientific productions sampled");}
    check(changed,"typed discrete mutation");
}
static void fitting(){
    const auto x=Expression::variable(0),sine=Expression::unary(K::Sin,x);
    // Independent rounded high-precision sin(0.125) reference from retained fixtures.
    auto p=RegressionProblem::create({{{.125},1.75*scientificReferences[0].expected}}, {15,5,8});
    auto expression=Expression::binary(K::Multiply,Expression::constant(1,1),sine);
    TrainingObjective objective(p,1024);ConstantFitConfig cfg;cfg.maxLossAttempts=1024;cfg.minimumStep=1e-10;
    auto result=fitConstants(expression,objective,cfg);check(result.improved && result.expression.nodes()[0].constantValue()==1.75 && result.loss.score()==0,"fixed sine coefficient recovery");
    for(bool exponential:{false,true}){
        // Coordinate a improves first; the subsequent c probes worsen or are invalid.
        const auto c=Expression::constant(exponential?709.:0.,1);
        const auto fn=Expression::unary(exponential?K::Exp:K::Sqrt,c);
        const double base=exponential?8.218407461554972e307:0.;
        const double scale=exponential?1e-308:1.;
        // Keep scale a variable instead, so it is not an adjustable third parameter.
        const auto part=exponential?Expression::binary(K::Multiply,Expression::variable(0),fn):fn;
        const auto e=Expression::binary(K::Add,Expression::constant(0,1),part);
        auto problem=RegressionProblem::create({{{scale},base*scale+1.2}},{15,5,710});TrainingObjective obj(problem,64);
        ConstantFitConfig fit;fit.lower=-710;fit.upper=710;fit.maxSweeps=1;fit.maxLossAttempts=32;
        auto fitted=fitConstants(e,obj,fit);
        check(fitted.improved && fitted.expression.nodes()[0].constantValue()==1,"progress before invalid probe");
        check(obj.work().invalid>0 && obj.work().accepted>0 && obj.work().lossAttempts==obj.work().fittingLossAttempts,"invalid probe accounted once");
        check(fitted.loss==problem->measure(fitted.expression).loss,"retained expression fitness pair");
    }
    const std::array schema{TypedShape::scalar()};
    auto tp=TypedRegressionProblem::create(schema,TypedShape::scalar(),{{{TypedValue::scalar(.125)},TypedValue::scalar(1.75*scientificReferences[0].expected)}});
    auto ts=TypedExpression::binary(T::Multiply,TypedExpression::constant(TypedValue::scalar(1),schema),TypedExpression::unary(T::Sin,TypedExpression::variable(0,schema)));
    TypedTrainingObjective to(tp,1024);auto fitted=fitTypedConstants(ts,to,cfg);
    check(fitted.improved && fitted.loss.score()<1e-24 && tp->measure(fitted.expression).loss==fitted.loss,"typed scientific fitting exact pairing");
    auto typedPower=TypedExpression::power(TypedExpression::constant(TypedValue::scalar(1),schema),3);
    auto pp=TypedRegressionProblem::create(schema,TypedShape::scalar(),{{{TypedValue::scalar(0)},TypedValue::scalar(8)}});TypedTrainingObjective po(pp,64);
    auto pf=fitTypedConstants(typedPower,po);
    check(pf.expression.nodes().back().exponent()==3 && pf.loss.score()==0,"typed fitting excludes discrete metadata");
    auto power=Expression::power(Expression::constant(1,1),3);auto problem=RegressionProblem::create({{{0},8}},{15,5,8});TrainingObjective obj(problem,64);auto fit=fitConstants(power,obj);
    check(fit.expression.nodes().back().exponent()==3 && fit.expression.evaluate(std::array{0.}).value()==8,"fitting excludes exponent metadata");
}
static void parallelSearch(){
    auto p=RegressionProblem::create({{{.125},.5},{{.75},1.2}},{15,5,8});
    SymbolicRegressionConfig config;config.population=6;config.elites=1;config.maxGenerationAttempts=2;config.maxEvaluations=18;config.maxTrainingLossAttempts=2000;
    config.grammar.operators={K::Add,K::Multiply,K::Sin,K::Cos,K::Exp,K::Sqrt,K::Powi};config.grammar.exponents={0,2,3};
    config.fitting=ConstantFitConfig{};config.fitting->maxLossAttempts=4;
    std::optional<SymbolicRegressionResult> reference;std::string state;
    for(unsigned n:{1u,2u,3u,4u}){
        RandomContext random(42,{n});auto result=symbolicRegression(p,config,random);std::ostringstream rng;rng<<random.engine();
        if(!reference){reference=result;state=rng.str();continue;}
        check(result.best && reference->best && result.best->text()==reference->best->text() && result.best->evaluation()==reference->best->evaluation(),"scientific scalar worker model equivalence");
        check(result.trainingWork==reference->trainingWork && result.variation==reference->variation && result.rawInitialExpressions==reference->rawInitialExpressions && result.preparedInitialExpressions==reference->preparedInitialExpressions && result.preparedInitialFitness==reference->preparedInitialFitness && result.publishedGenerations==reference->publishedGenerations && result.stoppingReason==reference->stoppingReason && rng.str()==state,"scientific scalar worker counters and RNG equivalence");
        check(result.improvements.size()==reference->improvements.size(),"scalar improvement history size");
        for(std::size_t i=0;i<result.improvements.size();++i){const auto& a=result.improvements[i];const auto& b=reference->improvements[i];check(a.lossAttempts==b.lossAttempts && a.generation==b.generation && bits(a.mse,b.mse),"scalar committed improvement history");}
    }
    const std::array schema{TypedShape::scalar()};
    auto tp=TypedRegressionProblem::create(schema,TypedShape::scalar(),{{{TypedValue::scalar(.125)},TypedValue::scalar(.5)},{{TypedValue::scalar(.75)},TypedValue::scalar(1.2)}});
    TypedSymbolicRegressionConfig cfg;cfg.population=6;cfg.elites=1;cfg.maxGenerationAttempts=2;cfg.maxEvaluations=18;cfg.maxTrainingLossAttempts=2000;
    cfg.grammar.operators={T::Add,T::Multiply,T::Sin,T::Cos,T::Exp,T::Sqrt,T::Powi};cfg.grammar.exponents={0,2,3};cfg.fitting=config.fitting;
    std::optional<TypedSymbolicRegressionResult> typedReference;
    for(unsigned n:{1u,2u,3u,4u}){
        RandomContext random(42,{n});auto result=typedSymbolicRegression(tp,cfg,random);std::ostringstream rng;rng<<random.engine();
        if(!typedReference){typedReference=result;state=rng.str();continue;}
        check(result.best && typedReference->best && result.best->text()==typedReference->best->text() && result.best->evaluation()==typedReference->best->evaluation(),"scientific typed worker model equivalence");
        check(result.trainingWork==typedReference->trainingWork && result.arithmeticOperations==typedReference->arithmeticOperations && result.variation==typedReference->variation && result.publishedGenerations==typedReference->publishedGenerations && result.stoppingReason==typedReference->stoppingReason && rng.str()==state,"scientific typed worker work and RNG equivalence");
    }
}
static void overlap(){
    std::barrier gate(2);std::array<bool,2> okay{false,false};
    const auto x=Expression::variable(0);const auto target=Expression::binary(K::Add,Expression::unary(K::Sin,x),Expression::power(x,3));
    auto worker=[&](unsigned slot){gate.arrive_and_wait();for(unsigned i=0;i<100;++i){auto a=target.evaluate(std::array{0.});if(!a.valid() || !bits(a.value(),0.))return;}okay[slot]=true;};
    std::jthread a(worker,0),b(worker,1);a.join();b.join();check(okay[0] && okay[1],"overlapping immutable scientific evaluation");
}
int main(){try{arithmetic();variation();fitting();parallelSearch();overlap();std::cout<<"scientific contracts passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
