#include <DerivativeRegression.hpp>
#include <ExpressionParsing.hpp>
#include "examples/ExecutionOptions.hpp"
#include "examples/RegressionWorkflow.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>

using Problem=DerivativeRegressionProblem;
static std::vector<double> points(int set) {
    if(set==0)return {-2,-1.5,-1,-.5,0,.5,1,1.5,2};
    if(set==1)return {-1.75,-1.25,-.75,-.25,.25,.75,1.25,1.75};
    return {-4,-3,3,4};
}
static std::shared_ptr<const Problem> training(bool fit=false) {
    std::vector<RegressionRow> derivatives;
    for(double x:points(0))derivatives.push_back({{x},fit?3*x-.5:2*x});
    return Problem::create(1,{{std::nullopt,{{{0},fit?1.25:1}},1,1},{0,derivatives,1,1}},fit?ExpressionLimits{15,6,8}:ExpressionLimits{9,4,2});
}
static std::shared_ptr<const Problem> assessment(int set,bool fit=false) {
    std::vector<RegressionRow> values,partials;
    // Independent ordinary dyadic arithmetic; never evaluatePartial as target oracle.
    for(double x:points(set)) {values.push_back({{x},fit?(1.5*x*x-.5*x)+1.25:x*x+1});partials.push_back({{x},fit?3*x-.5:2*x});}
    return Problem::create(1,{{std::nullopt,values,1,1},{0,partials,1,1}},{15,6,8});
}
static void work(std::ostream& out,const ObservationWork& w) {
    out<<" observations="<<w.observations<<" values="<<w.valueEvaluations<<" partials="<<w.partialEvaluations
       <<" primal_operations="<<w.primalOperations<<" derivative_arithmetic="<<w.derivativeArithmetic
       <<" derivative_math_calls="<<w.derivativeMathCalls<<" partial_nodes="<<w.partialNodeVisits<<" aggregate_operations="<<w.aggregationOperations<<" primal_invalid="<<w.primalInvalidObservations<<" unavailable="<<w.unavailableObservations<<" loss_failures="<<w.lossArithmeticFailures<<" resource_stops="<<w.resourceStops;
}
static void errors(std::ostream& out,const Expression& model,int set,bool fit=false) {
    const auto m=assessment(set,fit)->assess(model,ObservationEvaluationLimits{});
    out<<" set="<<set<<" complete="<<m.complete;
    for(std::size_t c=0;c<2;++c) {
        const auto& r=m.channels[c];out<<(c?" derivative_max_error=":" value_max_error=");
        if(r.maxAbsoluteError)out<<*r.maxAbsoluteError;else out<<"unavailable";
        out<<(c?" derivative_invalid=":" value_invalid=")<<r.primalInvalid<<" unavailable_"<<c<<'='<<r.derivativeUnavailable<<" resources_"<<c<<'='<<r.resourceStops;
    }
    out<<'\n';
}
int main(int argc,char** argv) {
    try {
        const auto execution=genetik_example::executionOptions(argc,argv);std::filesystem::path output;bool smoke=false;
        for(int i=1;i<argc;++i) {
            if(std::string_view(argv[i])=="--smoke"){smoke=true;continue;}
            if(std::string_view(argv[i])!="--output" || !output.empty() || ++i==argc)throw std::invalid_argument("usage: genetik_derivative_regression_campaign --output NEW_DIR [--smoke] [--threads N]");
            output=argv[i];
        }
        if(output.empty() || std::filesystem::exists(output))throw std::invalid_argument("explicit new output directory required");
        std::filesystem::create_directories(output);
        std::ofstream protocol(output/"PROTOCOL.txt");
        protocol<<"x0=x; channels=value anchor f(0)=1; partial index0 at {-2,-1.5,-1,-.5,0,.5,1,1.5,2}, target=2*x; scales=1 weights=1\n"
            <<"objective=0.5*partial_MSE+0.5*anchor_squared_error; channel order=value then partial0; preserve row order; no additional value labels in training\n"
            <<"seeds="<<(smoke?"0":"0,1,2,3,42")<<" population="<<(smoke?8:128)<<" objective_limit="<<(smoke?32:10000)<<" candidate_limit=same generation_limit="<<(smoke?3:2000)<<"\n"
            <<"operators=add,sub,mul uniform feasible; constants=-2,-1,0,1,2; continuous_probability=0; fitting=off; nodes=9 depth=4 max_abs_constant=2 arithmetic_limit=SIZE_MAX\n"
            <<"rank_selection elites=2 crossover=.9 mutation=.35; initial_depth=4 mutation_depth=3 grow_probability=.65 full_probability=.5 proposal_limit=8; fit_every=1 (disabled)\n"
            <<"construction dimensions<=64 channels<=65 observations<=65536 input_coefficients<=1048576; evaluation observations<=65536 aggregation<=1000000; value nodes<=65536 operations<=1000000 workspace<=8388608\n"
            <<"partial operations<=1000000 nodes<=65536 bindings<=65536 workspace<=8388608; opaque native calls count as operations; assessment uses separate default allowance\n"
            <<"stop exact zero composite objective; select by training objective,nodes,text; overall representative tie=first declared seed; freeze all models before assessments\n"
            <<"post-selection sets: training abscissae above (value labels except anchor withheld from search), interior={-1.75,-1.25,-.75,-.25,.25,.75,1.25,1.75}; exterior={-4,-3,3,4}; targets=x*x+1 and 2*x by ordinary arithmetic\n"
            <<"report recovery when max value AND partial errors<=1e-6 on each set; no retry/tuning/target injection; threads="<<execution.threads<<" seeds sequential\n"
            <<"separate coefficient fixture a*x*x+b*x+c initial=(1,1,1) target=(1.5,-.5,1.25); same derivative points target3*x-.5, anchor1.25; scale/weights1; max512 calls; bounds[-8,8] step1 min1e-8 sweeps128 proposals8192 threshold=none\n";
        protocol.close();
        auto problem=training();
        std::ofstream inputs(output/"training.csv");inputs<<std::setprecision(17)<<"channel,x,target,scale,weight\n";
        for(const auto& c:problem->channels())for(const auto& row:c.observations)inputs<<(c.partial?"partial0":"value")<<','<<row.inputs[0]<<','<<row.target<<','<<c.scale<<','<<c.weight<<'\n';
        inputs.close();
        SymbolicRegressionConfig cfg;cfg.population=smoke?8:128;cfg.elites=2;cfg.maxGenerationAttempts=smoke?3:2000;cfg.maxEvaluations=smoke?32:10000;cfg.maxTrainingLossAttempts=cfg.maxEvaluations;cfg.trainingLossThreshold=0;
        genetik_example::printExecution(execution,cfg.population);
        struct Frozen{unsigned seed;DerivativeRegressionResult result;};std::vector<Frozen> frozen;
        const std::vector<unsigned> seeds=smoke?std::vector<unsigned>{0}:std::vector<unsigned>{0,1,2,3,42};
        std::ofstream search(output/"search.txt");search<<std::setprecision(17);
        for(auto seed:seeds) {
            RandomContext random(seed,execution);const auto start=std::chrono::steady_clock::now();auto r=derivativeRegression(problem,cfg,random);
            const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();const auto& s=r.search;
            search<<"seed="<<seed<<" initial="<<(s.initialBestMSE?*s.initialBestMSE:-1)<<" published="<<s.publishedGenerations<<" generation_attempts="<<s.attemptedGenerations
                  <<" objective_attempts="<<s.trainingWork.lossAttempts<<" unused="<<cfg.maxTrainingLossAttempts-s.trainingWork.lossAttempts<<" unpublished="<<s.unpublishedLossAttempts
                  <<" discovery="<<(s.bestGeneration?std::to_string(*s.bestGeneration):"none")<<" seconds="<<seconds<<" stop="<<searchStopName(s.stoppingReason);
            work(search,r.observationWork);
            search<<" mutation_proposals="<<s.variation.mutationProposals<<" crossover_proposals="<<s.variation.crossoverProposals<<" rejections="<<s.variation.structuralRejections
                  <<" mutation_changes="<<s.variation.mutationChanges<<" crossover_changes="<<s.variation.crossoverChanges<<" mutation_fallbacks="<<s.variation.mutationFallbacks<<" crossover_fallbacks="<<s.variation.crossoverFallbacks<<'\n';
            if(s.best)search<<"model="<<s.best->text()<<"\nobjective="<<s.best->fitness()<<" nodes="<<s.best->expression().nodeCount()<<" depth="<<s.best->expression().depth()<<'\n';
            for(const auto& t:s.improvements)search<<"trace="<<seed<<','<<t.lossAttempts<<','<<t.generation<<','<<t.mse<<'\n';
            frozen.push_back({seed,std::move(r)});
        }
        std::optional<std::size_t> representative;RegressionOrdering order;
        for(std::size_t i=0;i<frozen.size();++i)if(frozen[i].result.search.best && (!representative || order.better(*frozen[i].result.search.best,*frozen[*representative].result.search.best,ObjectiveDirection::Minimize)))representative=i;
        search<<"all_models_frozen representative_seed="<<(representative?std::to_string(frozen[*representative].seed):"none")<<'\n';search.close();
        std::ofstream report(output/"assessment.txt");report<<std::setprecision(17);
        for(const auto& f:frozen)if(const auto& best=f.result.search.best) {
            const auto train=problem->assess(best->expression());
            report<<"seed="<<f.seed<<" value_anchor_MSE="<<*train.channels[0].rawMSE<<" anchor_error="<<*train.channels[0].maxAbsoluteError<<" partial_training_MSE="<<*train.channels[1].rawMSE<<" anchor_normalized="<<*train.channels[0].normalizedMean<<" partial_normalized="<<*train.channels[1].normalizedMean<<'\n';
            for(int set=0;set<3;++set){report<<"seed="<<f.seed;errors(report,best->expression(),set);}
        }
        auto coefficientProblem=training(true);DerivativeTrainingObjective objective(coefficientProblem,512);ConstantFitConfig fit;fit.maxLossAttempts=512;
        const auto initial=parseExpression("1*x0*x0+1*x0+1",1);auto fitted=fitConstants(initial,objective,fit);
        std::ofstream fixture(output/"fitting.txt");fixture<<std::setprecision(17)<<"initial="<<initial.toString()<<" initial_objective="<<fitted.startingLoss.score()<<"\nmodel="<<fitted.expression.toString()<<"\nobjective="<<fitted.loss.score()<<" calls="<<objective.work().lossAttempts<<" accepted="<<objective.work().accepted<<" stop="<<constantFitStopName(fitted.stop);
        work(fixture,objective.observationWork());fixture<<'\n';
        for(int set=0;set<3;++set)errors(fixture,fitted.expression,set,true);
        std::cout<<"records="<<output<<" frozen_runs="<<frozen.size()<<'\n';
    }catch(const std::exception& e){regression_example::printError(e);return 1;}
}
