#include <SymbolicRegression.hpp>
#include "../examples/RegressionWorkflow.hpp"
#include <array>
#include <cfenv>
#include <set>

using K=ExpressionKind;
using N=ExpressionNode;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class E,class F>static void throws(F f){bool caught=false;try{f();}catch(const E&){caught=true;}check(caught,"expected exception");}
static Expression c(double x){return Expression::constant(x);}
static Expression div(const Expression&a,const Expression&b){return Expression::binary(K::Divide,a,b);}
static Expression log(const Expression&a){return Expression::unary(K::Log,a);}
static Expression mul(const Expression&a,const Expression&b){return Expression::binary(K::Multiply,a,b);}
static double value(const Expression&e){const auto result=e.evaluate({});check(result.valid(),"expected finite result");return result.value();}
static void arithmetic() {
    check(value(div(c(9),c(3)))==3 && value(div(c(3),c(9)))==1./3,"division operand order");
    const auto a=div(div(c(8),c(4)),c(2)),b=div(c(8),div(c(4),c(2)));
    check(value(a)==1 && value(b)==4 && a.toString()=="((8.0 / 4.0) / 2.0)" && b.toString()=="(8.0 / (4.0 / 2.0))","division grouping");
    for(double zero:{0.,-0.})for(double numerator:{0.,-0.,1.}) {
        std::feclearexcept(FE_ALL_EXCEPT);
        const auto result=div(c(numerator),c(zero)).evaluate({});
        check(!result.valid() && result.failedNode()==2,"either signed-zero denominator is invalid");
        check((std::fetestexcept(FE_DIVBYZERO|FE_INVALID))==0,"domain checked before division");
    }
    for(double numerator:{0.,-0.})for(double denominator:{2.,-2.}) {
        const double result=value(div(c(numerator),c(denominator)));
        check(result==0 && std::signbit(result)==(std::signbit(numerator)!=std::signbit(denominator)),"signed-zero numerator result signs");
    }
    const auto tiny=std::numeric_limits<double>::denorm_min();
    const auto normal=std::numeric_limits<double>::min();
    const auto huge=std::numeric_limits<double>::max();
    check(value(div(c(tiny),c(tiny)))==1,"native division permits a nonzero subnormal denominator");
    check(value(div(c(normal),c(2)))==std::ldexp(1.,-1023),"finite subnormal quotient");
    const auto positiveZero=div(c(tiny),c(2)),negativeZero=div(c(-tiny),c(2));
    check(value(positiveZero)==0 && !std::signbit(value(positiveZero)) && value(negativeZero)==0 && std::signbit(value(negativeZero)),"finite signed-zero underflow is valid");
    check(!div(c(huge),c(.5)).evaluate({}).valid(),"quotient overflow invalid");
    check(value(log(c(1)))==0,"log one");
    check(std::abs(value(log(c(2)))-.69314718055994530942)<2e-15,"log two independent decimal reference");
    check(std::abs(value(log(c(tiny)))+744.44007192138121809)<2e-12,"positive subnormal log argument");
    check(std::abs(value(log(c(.5)))+.69314718055994530942)<2e-15,"positive argument below one gives negative logarithm");
    for(double argument:{0.,-0.,-1.}) {
        std::feclearexcept(FE_ALL_EXCEPT);
        check(!log(c(argument)).evaluate({}).valid(),"nonpositive log argument invalid");
        check((std::fetestexcept(FE_DIVBYZERO|FE_INVALID))==0,"domain checked before calling log");
    }
    const auto masked=mul(c(0),log(c(-1)));
    check(!masked.evaluate({}).valid() && masked.evaluate({}).failedNode()==2,"zero cannot mask invalid log child");
    const auto overflow=mul(c(huge),c(2));
    check(!div(overflow,c(2)).evaluate({}).valid() && !div(c(1),overflow).evaluate({}).valid(),"division cannot mask overflowing child");
    check(!log(positiveZero).evaluate({}).valid() && !div(c(1),negativeZero).evaluate({}).valid(),"parent independently rejects valid underflowed zero");
    check(log(c(-0.)).toString()=="log((-0.0))","export preserves signed zero and log domain");
    const auto x=Expression::variable(0);
    check(div(x,x).toString()=="(x0 / x0)" && !div(x,x).evaluate(std::array{0.}).valid(),"no x/x algebraic rewrite");
}
static Expression target() {
    const auto x=Expression::variable(0,2),y=Expression::variable(1,2);
    return Expression::binary(K::Add,log(x),div(y,x));
}
static void structure() {
    const auto expression=target();
    check(expression.nodeCount()==6 && expression.depth()==3 && expression.toString()=="(log(x0) + (x1 / x0))","six-node mixed arity fixture");
    check(std::abs(expression.evaluate(std::array{2.,4.}).value()-2.6931471805599453094)<2e-15,"mixed expression arithmetic");
    for(double x:{0.,-0.,-1.})check(!expression.evaluate(std::array{x,2.}).valid(),"constructed target domain probes have no fabricated labels");
    check(div(Expression::variable(1,2),Expression::variable(0,2)).evaluate(std::array{-2.,4.}).value()==-2,"negative nonzero denominator permitted without log");
    const auto nodes=expression.nodes();
    check(Expression(nodes,2,{6,3,8}).nodeCount()==6,"inclusive mixed node/depth limits");
    throws<std::length_error>([&]{Expression(nodes,2,{5,3,8});});
    throws<std::length_error>([&]{Expression(nodes,2,{6,2,8});});
    const auto x=Expression::variable(0,2),y=Expression::variable(1,2);
    check(Expression::unary(K::Log,x,{2,2}).nodeCount()==2,"two-node unary expression");
    throws<std::length_error>([&]{Expression::unary(K::Log,x,{1,2});});
    throws<std::length_error>([&]{Expression::unary(K::Log,x,{2,1});});
    check(Expression::unary(K::Log,x,{SIZE_MAX,SIZE_MAX}).depth()==2,"unary limits avoid overflow");
    throws<std::invalid_argument>([]{N::unary(K::Divide);});
    throws<std::invalid_argument>([]{N::binary(K::Log);});
    for(const std::vector<N>& malformed:{std::vector<N>{N::unary(K::Log)},
        {N::variable(0),N::binary(K::Divide)}, {N::variable(0),N::variable(1),N::unary(K::Log)},
        {N::variable(0),N::unary(K::Log),N::binary(K::Add)}})
        throws<std::invalid_argument>([&]{Expression(malformed,2);});
    check(expressionSubtree(expression,1)==std::pair<std::size_t,std::size_t>{0,2} &&
          expressionSubtree(expression,4)==std::pair<std::size_t,std::size_t>{2,5} &&
          expressionSubtree(expression,5)==std::pair<std::size_t,std::size_t>{0,6},"mixed subtree boundaries");
    const auto unaryReplaced=replaceExpressionSubtree(expression,1,y,{6,3,8});
    const auto binaryReplaced=replaceExpressionSubtree(expression,4,log(y),{6,3,8});
    const auto terminalReplaced=replaceExpressionSubtree(expression,0,div(x,y),{8,4,8});
    check(unaryReplaced && unaryReplaced->toString()=="(x1 + (x1 / x0))","unary-rooted replacement");
    check(binaryReplaced && binaryReplaced->toString()=="(log(x0) + log(x1))","binary-rooted replacement by unary");
    check(terminalReplaced && terminalReplaced->toString()=="(log((x0 / x1)) + (x1 / x0))","terminal-rooted replacement");
    check(!replaceExpressionSubtree(expression,0,log(log(x)),{31,4,8}),"replacement depth checked at destination");
    check(expression.toString()=="(log(x0) + (x1 / x0))","replacement preserves source");
    Expression copy=expression;
    throws<std::length_error>([&]{copy=Expression::unary(K::Log,expression,{6,3});});
    check(copy.toString()==expression.toString(),"failed unary construction preserves existing value");
    constexpr std::size_t levels=4096;
    std::vector<N> chain{N::constant(1)};
    for(std::size_t i=0;i<levels;++i)chain.push_back(N::unary(K::Log));
    const Expression deep(chain,0,{levels+1,levels+1});
    check(deep.depth()==levels+1 && deep.toString().size()==5*levels+3 && deep.evaluate({}).failedNode()==2,"bounded iterative unary-chain validation/export/evaluation");
    const Expression detached=deep;chain[0]=N::constant(-1);
    check(detached.nodes()[0].constantValue()==1,"unary values detach storage and copies");
}
static void variationAndLoss() {
    auto problem=RegressionProblem::create({{{1,2},2},{{2,1},1}},{31,6,8});
    ExpressionGrammar grammar;grammar.operators={K::Add,K::Subtract,K::Multiply,K::Divide,K::Log};
    grammar.continuousProbability=.5;
    ExpressionVariation variation(problem,grammar);
    RandomContext random(42);
    std::set<K> observed;
    for(int i=0;i<128;++i) {
        const auto parent=variation.initialize(random.engine()),other=variation.initialize(random.engine());
        auto a=parent,b=other;variation.crossover(parent,other,a,b,1,random.engine());
        const auto child=variation.mutation(a,1,random.engine());
        for(const auto* candidate:std::array<const ExpressionCandidate*,5>{&parent,&other,&a,&b,&child}) {
            const auto& e=candidate->expression();
            check(e.nodeCount()<=31 && e.depth()<=6 && e.variableCount()==2,"mixed-arity variation bounds");
            const auto tokens=e.nodes();const Expression verified(tokens,2,{31,6,8});
            check(verified.toString()==e.toString(),"all varied expressions validate as one tree");
            for(const auto& token:tokens)observed.insert(token.kind());
        }
    }
    check(observed.count(K::Log) && observed.count(K::Divide) && variation.statistics().mutationChanges && variation.statistics().crossoverChanges,"expanded operators actually generated and varied");
    grammar.operators={K::Log};grammar.constants.clear();grammar.continuousProbability=0;
    auto small=RegressionProblem::create({{{1},0}},{2,2,8});
    ExpressionVariation unaryOnly(small,grammar);
    bool sawUnary=false;
    for(int i=0;i<64;++i){const auto e=unaryOnly.initialize(random.engine()).expression();sawUnary|=e.nodeCount()==2;check(e.nodeCount()<=2 && e.depth()<=2,"feasible unary allocation with two-node budget");}
    check(sawUnary,"unary can use a two-node budget");
    ExpressionGrammar disabled;disabled.operators={K::Add,K::Subtract,K::Multiply};
    ExpressionVariation defaults(problem,disabled);
    for(int i=0;i<64;++i)for(const auto& n:defaults.initialize(random.engine()).expression().nodes())
        check(n.kind()!=K::Log && n.kind()!=K::Divide,"explicit old operator list excludes new operators");
    grammar.operators={K::Constant};throws<std::invalid_argument>([&]{ExpressionVariation bad(problem,grammar);});
    const auto partial=RegressionProblem::create({{{1},0},{{0},0},{{-1},0}},{31,6,8});
    const auto measured=partial->measure(log(Expression::variable(0)));
    check(measured.loss.state()==FitnessState::Invalid && !measured.maxAbsoluteError && measured.samplesAttempted==2,"late invalid training row invalidates full loss without partial favorable score");
    const auto report=regression_example::predictionReport(log(Expression::variable(0)),{{{1},0},{{0},0},{{-1},0}});
    check(report.rows==3 && report.invalid==2 && !report.maximum,"diagnostic invalid total inspects every row");
    // Legitimate linear labels, not labels of an undefined log target. Exercise the
    // SAME reporting path as the CLI with a constructed model, without GP discovery.
    const auto finalProblem=RegressionProblem::create({{{1},0},{{2},.6931471805599453}},{31,6,8});
    ExpressionCandidate finalized(log(Expression::variable(0)),finalProblem);finalized.calculateFitness();
    const auto savedLoss=finalized.evaluation(); const auto savedText=finalized.text();
    const auto evaluation=regression_example::evaluationReport(finalized.expression(),
        {{{1},0},{{0},-.6931471805599453},{{-1},-1.3862943611198906},{{2},.6931471805599453}});
    check(finalized.fitnessState()==FitnessState::Valid && evaluation.metrics.loss.state()==FitnessState::Invalid &&
          evaluation.metrics.samplesAttempted==2 && evaluation.predictions.rows==4 && evaluation.predictions.invalid==2 &&
          !evaluation.predictions.maximum && finalized.evaluation()==savedLoss && finalized.text()==savedText,
          "finalized training-valid expression retained while full evaluation loss is invalid and all invalid predictions counted");
    const auto selected=regression_example::selectOperators({"log","div"});
    check(selected==std::vector<K>{K::Log,K::Divide},"operator selection preserves requested order");
    for(const auto& names:std::vector<std::vector<std::string>>{{},{""},{"log","log"},{"protected_div"}})
        throws<std::invalid_argument>([&]{regression_example::selectOperators(names);});
    SymbolicRegressionConfig config;config.population=6;config.elites=1;config.maxGenerationAttempts=2;config.maxEvaluations=18;
    config.maxTrainingLossAttempts=128;config.fitting=ConstantFitConfig{};config.fitting->maxLossAttempts=4;
    config.grammar.operators={K::Add,K::Subtract,K::Multiply,K::Divide,K::Log};
    RandomContext first(8),second(8);
    const auto a=symbolicRegression(problem,config,first),b=symbolicRegression(problem,config,second);
    check(a.best && b.best && a.best->text()==b.best->text() && a.best->evaluation()==b.best->evaluation() &&
        a.trainingWork==b.trainingWork && a.variation==b.variation && a.candidateEvaluations==b.candidateEvaluations &&
        a.stoppingReason==b.stoppingReason && first.engine()==second.engine(),"short serial expanded search repeatability");
}
int main(){try{arithmetic();structure();variationAndLoss();std::cout<<"strict division/log contracts passed\n";}catch(const std::exception&e){regression_example::printError(e);return 1;}}
