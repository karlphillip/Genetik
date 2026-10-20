#include "../examples/RegressionData.hpp"
#include "../examples/RegressionWorkflow.hpp"
#include "../examples/MultivariateData.hpp"
#include <array>
#include <filesystem>
#include <locale>
#include <set>
#include <sstream>

using namespace regression_example;
static void check(bool value,const char* detail){if(!value)throw std::runtime_error(detail);}
template<class F> static void apiError(F action){bool caught=false;try{action();}catch(const std::invalid_argument&){caught=true;}check(caught,"expected API error");}
static Expression add(const Expression&a,const Expression&b){return Expression::binary(ExpressionKind::Add,a,b);}
static Expression mul(const Expression&a,const Expression&b){return Expression::binary(ExpressionKind::Multiply,a,b);}
static void bindingsAndPolicies() {
    const auto x0=Expression::variable(0,3),x1=Expression::variable(1,3),x2=Expression::variable(2,3);
    const auto expression=add(add(mul(Expression::constant(2,3),x0),mul(Expression::constant(-3,3),x1)),mul(Expression::constant(.5,3),x2));
    check(expression.evaluate(std::array{1.,2.,4.}).value()==-2,"independently known three-input result");
    check(expression.evaluate(std::array{2.,2.,4.}).value()==0,"change only x0");
    check(expression.evaluate(std::array{1.,3.,4.}).value()==-5,"change only x1");
    check(expression.evaluate(std::array{1.,2.,5.}).value()==-1.5,"change only x2");
    check(expression.toString()=="(((2.0 * x0) + ((-3.0) * x1)) + (0.5 * x2))","export preserves every variable identity");
    apiError([&]{expression.evaluate(std::array{1.,2.});});
    apiError([]{Expression::variable(3,3);});
    apiError([]{RegressionProblem::create({{{1,2},3},{{1},2}});});
    apiError([]{RegressionProblem::create({{{1,std::numeric_limits<double>::infinity()},3}});});
    apiError([]{RegressionProblem::create({{{1,2,3},std::numeric_limits<double>::quiet_NaN()}});});
    const auto problem=RegressionProblem::create({{{1,2,4},-2}},{31,6,8});
    const auto independent=RegressionProblem::create({{{1,2,4},99}},{31,6,8});
    ExpressionCandidate parent(expression,problem);parent.calculateFitness();
    check(parent.fitness()==0 && independent->measure(expression).loss.score()==10201 && parent.fitness()==0,"separate problems do not reuse cached fitness");
    RandomContext run(1);
    using P=Population<ExpressionCandidate,RegressionOrdering>;
    P population(std::vector{parent},run,ObjectiveDirection::Minimize);
    const auto calls=run.evaluationAttempts();
    apiError([&]{population.publishGeneration(std::vector{ExpressionCandidate(expression,independent)});});
    check(population.bestSeen()->fitness()==0 && population.generation()==0 && run.evaluationAttempts()==calls,"changed problem rejected before evaluation without corrupting committed state");
    apiError([&]{problem->measure(Expression::variable(0,2));});
    ExpressionGrammar grammar;grammar.constants.clear();grammar.operators.clear();
    ExpressionVariation variation(problem,grammar);
    std::set<std::size_t> initialized,mutated;
    ExpressionCandidate terminal(x0,problem);
    for(int i=0;i<96;++i) {
        initialized.insert(variation.initialize(run.engine()).expression().nodes()[0].variableIndex());
        mutated.insert(variation.mutation(terminal,1,run.engine()).expression().nodes()[0].variableIndex());
    }
    check(initialized==std::set<std::size_t>{0,1,2} && mutated==initialized,"initialization and mutation reach all permitted indices including x2");
    const auto replacement=replaceExpressionSubtree(add(x0,x1),0,x2,{31,6,8});
    check(replacement && replacement->toString()=="(x2 + x1)" && replacement->evaluate(std::array{10.,3.,5.}).value()==8,"subtree replacement preserves variable identities");
    ExpressionCandidate left(x0,problem),right(x2,problem),a=left,b=right;
    variation.crossover(left,right,a,b,1,run.engine());
    check(a.text()=="x2" && b.text()=="x0" && left.text()=="x0" && right.text()=="x2","subtree crossover preserves higher variable identity and parents");
    std::vector<RegressionRow> rows;
    for(double u:{-1.,1.})for(double v:{-1.,1.})for(double w:{-1.,1.})rows.push_back({{u,v,w},1.5*u-.25*v+2*w+.5});
    const auto fittingProblem=RegressionProblem::create(rows,{31,6,8});
    const auto one=Expression::constant(1,3);
    const auto skeleton=add(add(add(mul(one,x0),mul(one,x1)),mul(one,x2)),one);
    TrainingObjective objective(fittingProblem);
    ConstantFitConfig config;config.lossThreshold=0;
    const auto fitted=fitConstants(skeleton,objective,config);
    check(fitted.loss.score()==0 && fitted.lossAttempts<=256,"constant fitting is dimension independent");
}
static std::filesystem::path folder;
static std::string write(const std::string& name,const std::string& contents) {
    const auto path=folder/name;std::ofstream out(path,std::ios::binary);out<<contents;out.close();
    if(!out)throw std::runtime_error("cannot write test input");
    return path.string();
}
static void sameRows(const std::vector<RegressionRow>& a,const std::vector<RegressionRow>& b) {
    check(a.size()==b.size(),"same row count");
    for(std::size_t i=0;i<a.size();++i)check(a[i].inputs==b[i].inputs && a[i].target==b[i].target,"round-tripped input/target binary64 values");
}
static void parsingAndSearch() {
    writeMultivariateFiles(folder);
    const auto training=loadData((folder/"training.csv").string(),{"first","second"},"target");
    const auto memory=multivariateRows(0);
    sameRows(training.rows,memory);
    const auto x0=Expression::variable(0,2),x1=Expression::variable(1,2);
    const auto known=add(Expression::binary(ExpressionKind::Subtract,mul(Expression::constant(2.5,2),x0),
                            mul(Expression::constant(.75,2),x1)),Expression::constant(1.2,2));
    check(RegressionProblem::create(memory,{31,6,8})->measure(known).loss.score()==0,"constructed two-input affine fixture has independently known zero loss");
    check(known.evaluate(std::array{0.,0.}).value()==1.2 && known.evaluate(std::array{1.,0.}).value()==3.7 &&
          known.evaluate(std::array{0.,-1.}).value()==1.95,"two-input bindings change independently");
    check(memory.size()==25 && multivariateRows(1).size()==16 && multivariateRows(2).size()==8,"declared fixture shapes");
    for(const auto& row:multivariateRows(2))check(std::abs(row.inputs[0])>2 || std::abs(row.inputs[1])>2,"exterior rows lie outside training square");
    std::ostringstream reordered;reordered.imbue(std::locale::classic());reordered<<std::setprecision(17)<<"target,second,first,unused\r\n";
    for(const auto& row:memory)reordered<<row.target<<','<<row.inputs[1]<<','<<row.inputs[0]<<",1e2\r\n";
    const auto order=loadData(write("reordered.csv",reordered.str()),{"first","second"},"target");
    sameRows(order.rows,memory);
    check(order.inputColumns==std::vector<std::size_t>{2,1} && order.targetColumn==0,"explicit selected order overrides file column positions");
    const auto otherOrder=loadData((folder/"reordered.csv").string(),{"second","first"},"target");
    check(otherOrder.rows[0].inputs==std::vector<double>{memory[0].inputs[1],memory[0].inputs[0]} &&
          otherOrder.rows[1].inputs==std::vector<double>{memory[1].inputs[1],memory[1].inputs[0]},"reversed selected order is honored");
    const auto whitespace=loadData(write("whitespace.csv"," \t\r\n a , b , y \r\n\r\n +1.25e+1 , -.5 , 1e-2\r\n\t\r\n-0,2.,.25"),{"a","b"},"y");
    check(whitespace.rows.size()==2 && whitespace.rows[0].inputs==std::vector<double>{12.5,-.5} && whitespace.rows[0].target==.01 && std::signbit(whitespace.rows[1].inputs[0]),"whitespace CRLF decimal scientific final line and signed zero");
    struct Comma:std::numpunct<char>{char do_decimal_point()const override{return ',';}};
    const auto saved=std::locale();std::locale::global(std::locale(saved,new Comma));
    const auto localeData=loadData((folder/"whitespace.csv").string(),{"a","b"},"y");
    writeMultivariateFiles(folder/"locale");std::locale::global(saved);
    sameRows(localeData.rows,whitespace.rows);
    sameRows(loadData((folder/"locale/training.csv").string(),{"first","second"},"target").rows,memory);
    auto config=searchConfig(256);config.population=8;config.elites=1;config.maxGenerationAttempts=2;
    config.fitting->maxLossAttempts=8;
    RandomContext parsedRun(42),memoryRun(42),orderedRun(42);
    const auto a=symbolicRegression(RegressionProblem::create(training.rows,{31,6,8}),config,parsedRun);
    const auto b=symbolicRegression(RegressionProblem::create(memory,{31,6,8}),config,memoryRun);
    const auto c=symbolicRegression(RegressionProblem::create(order.rows,{31,6,8}),config,orderedRun);
    for(const auto* value:{&b,&c}) {
        check(a.best && value->best && a.best->text()==value->best->text() && a.best->evaluation()==value->best->evaluation(),"parsed/in-memory searches have exact expression/fitness pairing");
        check(a.trainingWork==value->trainingWork && a.variation==value->variation && a.candidateEvaluations==value->candidateEvaluations &&
              a.attemptedGenerations==value->attemptedGenerations && a.publishedGenerations==value->publishedGenerations &&
              a.bestGeneration==value->bestGeneration && a.stoppingReason==value->stoppingReason &&
              a.rawInitialExpressions==value->rawInitialExpressions && a.initialBestMSE==value->initialBestMSE &&
              a.rawInitialBestMSE==value->rawInitialBestMSE && a.improvements.size()==value->improvements.size(),"deterministic search results/counters with separate equivalent problems");
        for(std::size_t i=0;i<a.improvements.size();++i)check(a.improvements[i].generation==value->improvements[i].generation &&
            a.improvements[i].lossAttempts==value->improvements[i].lossAttempts && a.improvements[i].mse==value->improvements[i].mse,"deterministic improvement trace");
    }
    check(parsedRun.engine()==memoryRun.engine() && parsedRun.engine()==orderedRun.engine(),"identical consumed random state");
}
static void invalidFiles() {
    const std::vector<std::string> invalid={"","a,b,y\n","a,,y\n1,2,3\n","a,a,y\n1,2,3\n","a,b,y\n1,2\n","a,b,y\n1,2,3,4\n",
        "a,b,y\n1,,3\n","a,b,y\n1,2,3junk\n","a,b,y\n1,2,nan\n","a,b,y\ninf,2,3\n","a,b,y\n1e999,2,3\n",
        "a,b,y\n1e-999,2,3\n","a,b,y\n\"1\",2,3\n","\"a\",b,y\n1,2,3\n","a,b,y\n1,2,3\n4,5,invalid\n",
        "a,b,y,unused\n1,2,3,nan\n","a,b,y\n+-1,2,3\n"};
    for(std::size_t i=0;i<invalid.size();++i) {
        const auto path=write("invalid"+std::to_string(i)+".csv",invalid[i]);
        bool caught=false;try{loadData(path,{"a","b"},"y");}catch(const std::invalid_argument& error){caught=std::string(error.what()).find(path+":row ")!=std::string::npos;}
        check(caught,"invalid file rejected wholly with filename/row/column diagnostics");
    }
    const auto path=write("good.csv","a,b,y\n1,2,3\n");
    for(const auto& selected:std::vector<std::vector<std::string>>{{},{"a","a"},{"a","y"},{"unknown"},{""}})
        apiError([&]{loadData(path,selected,"y");});
    apiError([&]{loadData(path,{"a"},"unknown");});
    apiError([&]{loadData((folder/"missing.csv").string(),{"a"},"y");});
    for(int which=0;which<6;++which) {
        TableLimits limits;
        if(which==0)limits.maxBytes=10;
        if(which==1)limits.maxLineBytes=4;
        if(which==2)limits.maxColumns=2;
        if(which==3)limits.maxNameBytes=1;
        if(which==4)limits.maxNumberBytes=1;
        if(which==5)limits.maxRows=1;
        const auto limited=write("limited.csv","aa,b,y\n12,2,3\n4,5,6\n");
        apiError([&]{loadData(limited,{"aa"},"y",limits);});
    }
    TableLimits limits;
    limits.maxBytes=12;limits.maxLineBytes=5;limits.maxRows=1;limits.maxColumns=3;limits.maxNameBytes=1;limits.maxNumberBytes=1;
    check(loadData(path,{"a"},"y",limits).rows.size()==1,"file resource limits include the exact boundary");
    limits.maxRows=0;apiError([&]{loadData(path,{"a"},"y",limits);});
}
int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::runtime_error("test workspace argument required");
        folder=argv[1];std::filesystem::create_directories(folder);
        bindingsAndPolicies();parsingAndSearch();invalidFiles();
        std::cout<<"multivariate and file-input contracts passed\n";
    }catch(const std::exception& error){printError(error);return 1;}
}
