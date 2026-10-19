#include <SymbolicRegression.hpp>
#include <array>
#include <iostream>
#include <iomanip>

static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class E, class F> static void throws(F f) { bool ok = false; try { f(); } catch (const E&) { ok = true; } check(ok, "missing expected exception"); }
using K = ExpressionKind;
static Expression mul(const Expression& a, const Expression& b) { return Expression::binary(K::Multiply, a, b); }
static Expression add(const Expression& a, const Expression& b) { return Expression::binary(K::Add, a, b); }
static Expression c(double value) { return Expression::constant(value, 1); }
static std::vector<RegressionRow> rows(double a, double b, double d, int mode = 0) {
    std::vector<RegressionRow> result;
    std::vector<double> xs;
    if (mode == 2) xs = {-4,-3,3,4};
    else for (int k = 0; k < (mode == 0 ? 17 : 16); ++k) xs.push_back((mode == 0 ? -2. : -15./8) + k/4.);
    for (double x : xs) result.push_back({{x}, a*x*x+b*x+d});
    return result;
}
static Expression skeleton() {
    const auto x = Expression::variable(0);
    return add(add(mul(c(1),mul(x,x)),mul(c(1),x)),c(1));
}
static void fixtures() {
    for (const auto coefficients : {std::array{2.5,-1.75,.625}, std::array{1.7,-.65,2.3}}) {
        auto problem = RegressionProblem::create(rows(coefficients[0],coefficients[1],coefficients[2]),{31,6,8});
        ConstantFitConfig config;
        config.maxLossAttempts = 1024; config.maxSweeps = 512; config.maxProposals = 8192;
        config.minimumStep = 1e-11; config.lossThreshold = 1e-20;
        const auto input = skeleton();
        TrainingObjective objective(problem), repeat(problem);
        const auto result = fitConstants(input, objective, config);
        const auto again = fitConstants(input, repeat, config);
        check(result.expression.toString() == again.expression.toString() && result.loss == again.loss && objective.work() == repeat.work(),
              "deterministic reference fit and accounting");
        check(result.improved && result.lossAttempts <= 1024 && result.expression.nodeCount() == input.nodeCount() &&
              result.expression.depth() == input.depth() && input.toString() == skeleton().toString(), "bounded fixed topology and unchanged input");
        const auto before = input.nodes(), after = result.expression.nodes();
        std::size_t parameter = 0;
        for (std::size_t i=0;i<before.size();++i) {
            check(before[i].kind() == after[i].kind(), "fixed operators and topology");
            if (before[i].kind() == K::Constant) check(std::abs(after[i].constantValue()-coefficients[parameter++]) <= 1e-6, "identifiable parameter recovery");
            else if (before[i].kind() == K::Variable) check(before[i].variableIndex() == after[i].variableIndex(), "fixed variable identities");
        }
        std::array<double,3> errors{};
        for (int mode=0;mode<3;++mode) {
            const auto report = RegressionProblem::create(rows(coefficients[0],coefficients[1],coefficients[2],mode),{31,6,8});
            errors[mode] = *report->measure(result.expression).maxAbsoluteError;
            check(errors[mode] <= 1e-6, "fixed structure prediction recovery");
        }
        check(objective.measure(result.expression).loss == result.loss, "exact returned score verified and charged");
        check(objective.work().rowAttempts == objective.work().lossAttempts*17, "all row attempts accounted");
        std::cout << "fixture=" << coefficients[0] << ',' << coefficients[1] << ',' << coefficients[2]
                  << " calls=" << result.lossAttempts << " mse=" << result.loss.score()
                  << " training_max=" << errors[0] << " interior_max=" << errors[1] << " exterior_max=" << errors[2]
                  << " stop=" << constantFitStopName(result.stop) << " expression=" << result.expression.toString() << '\n';
    }
}
static void boundaries() {
    auto problem = RegressionProblem::create({{{0},2},{{1},4}},{31,6,8}); // constant LS optimum = 3, MSE=1
    TrainingObjective objective(problem);
    auto result = fitConstants(c(1),objective);
    check(result.expression.nodes()[0].constantValue()==3 && result.loss.score()==1, "independent known constant least squares optimum");
    ConstantFitConfig config; config.lower=0;config.upper=2;
    TrainingObjective bounded(problem);
    result = fitConstants(c(1),bounded,config);
    check(result.expression.nodes()[0].constantValue()==2 && result.loss.score()==2 && bounded.work().infeasible>0,"constrained optimum and infeasible skips");
    throws<std::invalid_argument>([&]{ fitConstants(c(3),bounded,config); });
    config.maxLossAttempts=2;
    TrainingObjective limited(problem);
    result=fitConstants(c(1),limited,config);
    check(result.improved && result.stop==ConstantFitStop::EvaluationLimit && result.loss.score()==2 && result.lossAttempts==2,"local budget retains improvement");
    config={};config.maxProposals=1;
    TrainingObjective proposal(problem);
    check(fitConstants(c(1),proposal,config).stop==ConstantFitStop::ProposalLimit,"finite proposal cap");
    config={};config.maxSweeps=1;
    TrainingObjective sweep(problem);
    check(fitConstants(c(1),sweep,config).stop==ConstantFitStop::SweepLimit,"finite sweep cap");
    config={};config.lossThreshold=1;
    TrainingObjective done(problem);
    result=fitConstants(c(3),done,config);
    check(!result.improved && result.lossAttempts==1 && result.stop==ConstantFitStop::LossThreshold,"already meets threshold");
    TrainingObjective noConstants(problem);
    result=fitConstants(Expression::variable(0),noConstants);
    check(result.stop==ConstantFitStop::NoConstants && result.lossAttempts==1,"constant free baseline");
    const auto zeros = RegressionProblem::create({{{0},0},{{1},1}},{31,6,8});
    TrainingObjective coupled(zeros);
    result=fitConstants(mul(mul(c(0),c(0)),Expression::variable(0)),coupled);
    check(!result.improved && result.loss.score()==.5 && result.stop==ConstantFitStop::MinimumStep,"coupled zero stall is not success");
    TrainingObjective unused(zeros);
    result=fitConstants(add(Expression::variable(0),mul(c(1),c(0))),unused);
    check(!result.improved && result.loss.score()==0,"unused parameters do not force changes");
    auto independentProblem=RegressionProblem::create({{{-1},-1},{{0},1},{{1},3}},{31,6,8});
    TrainingObjective independent(independentProblem);
    result=fitConstants(add(mul(c(1),Expression::variable(0)),c(1)),independent);
    auto nodes=result.expression.nodes();
    check(result.loss.score()==0 && nodes[0].constantValue()==2 && nodes[3].constantValue()==1,"equal valued occurrences independent");
    auto hugeProblem=RegressionProblem::create({{{1e200},0}},{31,6,8});
    TrainingObjective invalid(hugeProblem);
    result=fitConstants(mul(c(2),Expression::variable(0)),invalid);
    check(result.stop==ConstantFitStop::InvalidStart && result.loss.state()==FitnessState::Invalid && result.lossAttempts==1,"invalid start ordinary outcome");
    auto overflowProblem=RegressionProblem::create({{{1e154},7e153}},{31,6,8});
    TrainingObjective trials(overflowProblem);
    result=fitConstants(mul(c(-.5),Expression::variable(0)),trials);
    check(result.improved && trials.work().invalid>0 && result.loss.score()<1e300,"later invalid and worsening probes retain accepted best");
    config={};config.initialStep=1e-300;config.minimumStep=1e-301;
    TrainingObjective unchanged(problem);
    result=fitConstants(c(1),unchanged,config);
    check(!result.improved && unchanged.work().duplicates>0 && result.lossAttempts==1,"unrepresentable probes bounded without evaluations");
    check(objective.work().duplicates > 0, "previously scored duplicate expressions are skipped");
    const double huge=std::numeric_limits<double>::max();
    auto wide=RegressionProblem::create({{{0},0}},{31,6,huge});
    TrainingObjective nonfinite(wide);
    config={};config.lower=-huge;config.upper=huge;config.initialStep=huge;config.minimumStep=huge;
    result=fitConstants(mul(c(huge),c(0)),nonfinite,config);
    check(nonfinite.work().infeasible>0 && result.loss.score()==0,"non-finite parameter proposals skipped without output clamping");
    TrainingObjective limitedConstants(problem);
    config={};
    const auto narrow=Expression::constant(1,1,{1,1,1});
    result=fitConstants(narrow,limitedConstants,config);
    check(result.expression.limits().maxAbsConstant==1 && result.expression.limits().maxNodes==1 &&
          result.expression.limits().maxDepth==1 && result.expression.nodes()[0].constantValue()==1,"input value's own hard limits retained");
    config={};config.initialStep=std::numeric_limits<double>::infinity();
    throws<std::invalid_argument>([&]{fitConstants(c(1),unchanged,config);});
}
class ThrowingObjective : public TrainingObjective {
public:
    using TrainingObjective::TrainingObjective;
    std::uint64_t throwAt=0;
protected:
    RegressionMetrics compute(const Expression& e) override {
        const auto result=TrainingObjective::compute(e);
        if (work().lossAttempts==throwAt) throw std::logic_error("deliberate fitting fault");
        return result;
    }
};
static bool hasCause(const std::exception& e) {
    if (std::string(e.what())=="deliberate fitting fault") return true;
    try {std::rethrow_if_nested(e);} catch(const std::exception& n) {return hasCause(n);} catch(...) {}
    return false;
}
static void integration() {
    auto problem=RegressionProblem::create({{{0},2},{{1},4}},{31,6,8});
    auto objective=std::make_shared<ThrowingObjective>(problem);
    ConstantFitConfig config; config.maxLossAttempts=4;
    auto preparation=std::make_shared<ConstantRefinement>(objective,config);
    ExpressionCandidate parent(c(0),problem,preparation);
    RandomContext random(7);
    using P=Population<ExpressionCandidate,RegressionOrdering>;
    P population(std::vector{parent,parent},random,ObjectiveDirection::Minimize);
    check(parent.fitnessState()==FitnessState::Unevaluated && parent.text()=="0.0", "initial fitting private copy");
    const auto saved=population.creatures(); const auto best=population.bestSeen();
    check(best->text()!="0.0" && objective->measure(best->expression()).loss==best->evaluation(),"installed refined expression and matching fitness");
    const auto previous=objective->work();
    objective->throwAt=previous.lossAttempts+3;
    ExpressionGrammar grammar; grammar.continuousProbability=.5;
    GeneticAlgorithm<P,ExpressionCandidate,ExpressionVariation> ga(.9f,.35f,random,ExpressionVariation(problem,grammar,preparation));
    const auto engine=random.engine(); const auto calls=random.evaluationAttempts();
    bool thrown=false;
    try {ga.evolve(population);} catch(const std::exception& e) {thrown=hasCause(e);}
    check(thrown && ga.attempts()==1 && ga.evolutions()==0,"nested operational cause retained after partial fitting");
    check(population.generation()==0 && population.bestGeneration()==0 && population.bestSeen()->text()==best->text() &&
          population.bestSeen()->evaluation()==best->evaluation(),"failed fitting preserves committed archive");
    const auto after=population.creatures();
    for(std::size_t i=0;i<saved->size();++i)check((*saved)[i].text()==(*after)[i].text() && (*saved)[i].evaluation()==(*after)[i].evaluation(),"committed members and caches preserved");
    check(objective->work().lossAttempts==previous.lossAttempts+3 && objective->work().rowAttempts==previous.rowAttempts+6 &&
          random.evaluationAttempts()==calls+1 && random.engine()!=engine,"failed work and randomness consumed");
    ExpressionCandidate changed=*best;changed.replaceExpression(c(1));
    check(changed.fitnessState()==FitnessState::Unevaluated && best->text()==population.bestSeen()->text(),"edits invalidate without parent aliases");
    throws<std::invalid_argument>([&]{changed.replaceExpression(c(9));});
    check(changed.text()=="1.0","failed replacement preserved value");
    ExpressionCandidate intact=*best;
    throws<std::invalid_argument>([&]{intact.replaceExpression(c(9));});
    check(intact.text()==best->text() && intact.evaluation()==best->evaluation(),"failed replacement preserves valid cache");
    // Global exhaustion deliberately interrupts a fit after a successful initial publication.
    auto cap=std::make_shared<TrainingObjective>(problem,7);
    config.maxLossAttempts=2;
    auto prep=std::make_shared<ConstantRefinement>(cap,config);
    RandomContext rng(3);
    ExpressionCandidate candidate(c(0),problem,prep);
    P limited(std::vector{candidate,candidate},rng,ObjectiveDirection::Minimize);
    const auto committed=limited.bestSeen();
    throws<std::runtime_error>([&]{limited.updateValues();});
    check(cap->work().lossAttempts==7 && limited.generation()==0 && limited.bestSeen()->text()==committed->text(),"global cap discards incomplete generation and retains improvement only in committed state");
    auto invalidSuccessorProblem=RegressionProblem::create({{{1e200},0}},{31,6,8});
    auto invalidSuccessorObjective=std::make_shared<TrainingObjective>(invalidSuccessorProblem);
    auto invalidSuccessorPrep=std::make_shared<ConstantRefinement>(invalidSuccessorObjective,ConstantFitConfig{});
    ExpressionCandidate viable(c(0),invalidSuccessorProblem,invalidSuccessorPrep);
    RandomContext viableRng(0);P viablePopulation(std::vector{viable},viableRng,ObjectiveDirection::Minimize);
    const auto viableBest=viablePopulation.bestSeen();
    viable.replaceExpression(mul(c(2),Expression::variable(0)));
    check(viablePopulation.publishGeneration(std::vector{viable})==GenerationOutcome::NoViableCandidates &&
          viablePopulation.generation()==0 && viablePopulation.bestSeen()->text()==viableBest->text(),"all invalid refined successor preserves committed state");
    auto invalidProblem=RegressionProblem::create({{{1e200},0}},{31,6,8});
    auto badObjective=std::make_shared<TrainingObjective>(invalidProblem);
    auto badPrep=std::make_shared<ConstantRefinement>(badObjective,ConstantFitConfig{});
    ExpressionCandidate bad(mul(c(2),Expression::variable(0)),invalidProblem,badPrep);
    RandomContext badRng(0);P none(std::vector{bad},badRng,ObjectiveDirection::Minimize);
    check(none.lastOutcome()==GenerationOutcome::NoViableCandidates && !none.bestSeen() && !none.generation(),"all invalid refined initialization no commitment");
}
static void sampling() {
    const auto problem=RegressionProblem::create({{{0},0}},{31,6,8});
    ExpressionGrammar grammar; grammar.operators.clear();grammar.continuousProbability=.5;
    ExpressionVariation variation(problem,grammar);
    RandomContext random(99);
    bool sawContinuous=false,sawDiscrete=false,sawVariable=false;
    for(int i=0;i<200;++i){
        const auto value=variation.initialize(random.engine());
        const auto node=value.expression().nodes()[0];
        if(node.kind()==K::Variable){sawVariable=true;continue;}
        const double number=node.constantValue();
        check(std::isfinite(number) && number>=-8 && number<=8,"bounded continuous sampling");
        if(number==std::trunc(number) && std::abs(number)<=2)sawDiscrete=true;else sawContinuous=true;
    }
    check(sawContinuous && sawDiscrete && sawVariable,"seeded sampling exercises all terminal sources");
    grammar.constants.clear();grammar.continuousProbability=1;
    ExpressionVariation onlyContinuous(problem,grammar);
    for(int i=0;i<10;++i)(void)onlyContinuous.initialize(random.engine());
    grammar.continuousProbability=.5;
    throws<std::invalid_argument>([&]{ExpressionVariation bad(problem,grammar);});
    grammar.constants={1};grammar.continuousLower=-9;
    throws<std::invalid_argument>([&]{ExpressionVariation bad(problem,grammar);});
    grammar.continuousLower=1;grammar.continuousUpper=-1;
    throws<std::invalid_argument>([&]{ExpressionVariation bad(problem,grammar);});
    grammar.continuousProbability=std::numeric_limits<double>::quiet_NaN();
    throws<std::invalid_argument>([&]{ExpressionVariation bad(problem,grammar);});
}
static void searches() {
    auto problem=RegressionProblem::create(rows(2.5,-1.75,.625),{31,6,8});
    SymbolicRegressionConfig config;config.population=6;config.elites=1;config.maxGenerationAttempts=2;
    config.maxEvaluations=18;config.maxTrainingLossAttempts=200;config.fitting=ConstantFitConfig{};
    config.fitting->maxLossAttempts=4;config.grammar.continuousProbability=.5;
    RandomContext first(12),second(12),raw(12);
    const auto a=symbolicRegression(problem,config,first),b=symbolicRegression(problem,config,second);
    check(a.best->text()==b.best->text() && a.best->evaluation()==b.best->evaluation() && a.trainingWork==b.trainingWork &&
          a.variation==b.variation && a.candidateEvaluations==b.candidateEvaluations && a.bestGeneration==b.bestGeneration &&
          first.engine()==second.engine(),"serial integrated repeatability");
    config.fitting.reset();const auto disabled=symbolicRegression(problem,config,raw);
    check(a.rawInitialExpressions==disabled.rawInitialExpressions && a.rawInitialBestMSE==disabled.rawInitialBestMSE,"paired raw population and baseline match");
    // A caller-retained objective keeps detailed accounting observable if the high-level API throws.
    auto fault=std::make_shared<ThrowingObjective>(problem,config.maxTrainingLossAttempts);
    fault->throwAt=3;
    config.fitting=ConstantFitConfig{};config.fitting->maxLossAttempts=4;
    RandomContext failing(12);
    bool retainedCause=false;
    try {(void)symbolicRegression(problem,config,failing,fault);}catch(const std::exception& e){retainedCause=hasCause(e);}
    check(retainedCause && fault->work().lossAttempts==3 && fault->work().rowAttempts==51 &&
          failing.evaluationAttempts()>=1 && failing.evaluationAttempts()<=3,"caller-retained objective reports failed high-level attempted work");
    RandomContext rejectReused(12);
    throws<std::invalid_argument>([&]{symbolicRegression(problem,config,rejectReused,fault);});
    config.fitting=ConstantFitConfig{};config.maxTrainingLossAttempts=7;
    RandomContext initialFailure(12);const auto empty=symbolicRegression(problem,config,initialFailure);
    check(!empty.best && empty.trainingWork.lossAttempts==7 && empty.stoppingReason==SearchStop::EvaluationBudget,"initial global exhaustion publishes no partial initial population");
    config.fitting->maxLossAttempts=2;config.maxTrainingLossAttempts=15;
    RandomContext partial(12);const auto stopped=symbolicRegression(problem,config,partial);
    check(stopped.trainingWork.lossAttempts<=15 && stopped.stoppingReason==SearchStop::EvaluationBudget &&
          stopped.publishedGenerations==0 && stopped.bestGeneration==0,"driver global cap preserves committed boundary");
}
int main() {
    std::cout << std::setprecision(17);
    try {fixtures();boundaries();integration();sampling();searches();std::cout<<"constant fitting contracts passed\n";}
    catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
