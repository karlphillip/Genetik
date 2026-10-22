#include <IntegralRegression.hpp>
#include <ExpressionParsing.hpp>
#include "examples/ExecutionOptions.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <iostream>
namespace fs=std::filesystem;
static const std::vector<double> trainingPoints{-2,-1.5,-1,-.5,.5,1,1.5,2};
static const std::vector<double> interior{-1.75,-1.25,-.75,-.25,.25,.75,1.25,1.75},exterior{-4,-3,3,4};
static auto problem(double a=2,double b=1,double constantBound=2) {
    std::vector<RegressionRow> rows;for(double x:trainingPoints)rows.push_back({{x},a*x*x/2+b*x});
    return IntegralRegressionProblem::create(1,parseExpression("0",1),parseExpression("x0",1),rows,{9,4,constantBound},{},IntegralRestrictions{std::vector<std::size_t>{1},std::vector<ExpressionKind>{ExpressionKind::Add,ExpressionKind::Subtract,ExpressionKind::Multiply}});
}
static void work(std::ostream& out,const IntegralWork& w) {
    out<<" integrals="<<w.integrals<<" bounds="<<w.boundEvaluations<<" samples="<<w.integrandEvaluations<<" expression_ops="<<w.expressionOperations
       <<" node_visits="<<w.nodeVisits<<" bindings="<<w.bindingsValidated<<" quadrature_ops="<<w.quadratureOperations<<" loss_arithmetic="<<w.lossArithmeticOperations
       <<" panels="<<w.completedPanels<<" splits="<<w.subdivisions<<" converged="<<w.successes<<" invalid="<<w.mathematicalFailures<<" numerical="<<w.numericalFailures
       <<" resources="<<w.resources<<" nonconvergence="<<w.nonconvergence<<" resolution="<<w.resolution<<" cancelled="<<w.cancellations<<" peak_payload="<<w.workspaceBytes;
}
static void assess(std::ostream& out,const IntegralRegressionProblem& p,const Expression& g,const std::vector<double>& points,const char* label,double a=2,double b=1) {
    double maxIntegral=0,maxIntegrand=0,maxEstimated=0;std::size_t failed=0,invalidIntegrand=0;IntegralWork total;
    for(double x:points) {
        const auto r=p.model(g).evaluate(std::array{x});bool overflow=false;mergeIntegralWork(total,r.work,overflow);if(overflow)throw std::overflow_error("report work overflow");
        const auto sample=g.evaluate(std::array{0.,x});
        if(r.accepted()){maxIntegral=std::max(maxIntegral,std::abs(*r.estimate-(a*x*x/2+b*x)));maxEstimated=std::max(maxEstimated,*r.estimatedAbsoluteError);}else ++failed;
        if(sample.valid())maxIntegrand=std::max(maxIntegrand,std::abs(sample.value()-(a*x+b)));else ++invalidIntegrand;
    }
    out<<"set="<<label<<" rows="<<points.size()<<" integral_max="<<(failed?-1:maxIntegral)<<" integrand_max="<<(invalidIntegrand?-1:maxIntegrand)<<" failed_integrals="<<failed<<" invalid_integrand="<<invalidIntegrand<<" max_estimated_error="<<maxEstimated
       <<" recovery="<<(!failed && !invalidIntegrand && maxIntegral<=1e-6 && maxIntegrand<=1e-6);work(out,total);out<<'\n';
}
int main(int argc,char** argv) {
    try {
        fs::path output;ExecutionConfig execution;bool smoke=false;
        for(int i=1;i<argc;++i) {
            std::string_view arg=argv[i];if(arg=="--output" && i+1<argc)output=argv[++i];
            else if(arg=="--threads" && i+1<argc)execution=ExecutionConfig::parse(argv[++i]);
            else if(arg=="--smoke")smoke=true;else throw std::invalid_argument("usage: integral_campaign --output NEW_DIR [--threads N] [--smoke]");
        }
        if(output.empty() || fs::exists(output))throw std::invalid_argument("--output must be a new explicit directory");
        fs::create_directories(output);
        std::ofstream protocol(output/"PROTOCOL.txt");
        protocol<<"external x0=x; appended local x1=t; fixed lower0 upperx0; integrand variables ONLY original slot1\n"
          <<"target training F=x*x+x by ordinary dyadic arithmetic, no integrand labels; inputs=-2,-1.5,-1,-.5,.5,1,1.5,2; omit0 (structurally imposed zero)\n"
          <<"seeds=0,1,2,3,42 population128 elites2; generations2000 candidate/objective limits10000; operators add/sub/mul uniform feasible; constants -2,-1,0,1,2; continuous0 fitOFF; nodes9 depth4 constant_bound2\n"
          <<"initial_depth4 mutation_depth3 grow.65 full.5 proposals8 crossover.9 mutation.35 rank_selection; fit_every1 (disabled)\n"
          <<"GK7/15 abs1e-10 rel1e-10 samples4096 panels256 depth32; expression_ops1000000 node_visits1000000 bindings1000000 quadrature_ops1000000 workspace8388608 model_nodes65536 external_max65536\n"
          <<"objective rows<=4096 input_coefficients<=1048576; worst-case samples<=16777216; expression/node/binding/quad allowances<=4294967296 each; one full model objective=one loss attempt\n"
          <<"stop training MSE<=1e-20; select MSE,nodes,faithful text; representative ties first declared seed; freeze ALL models before assessment\n"
          <<"assessment: training inputs(post-selection integrand), interior=-1.75,-1.25,-.75,-.25,.25,.75,1.25,1.75; exterior=-4,-3,3,4; zero point0; reference F=x*x+x and g=2*t+1 independent arithmetic\n"
          <<"recovery max integral AND integrand error<=1e-6 each set; no retries/tuning/injection; seeds sequential threads="<<execution.threads<<" smoke="<<smoke<<"\n"
          <<"separate supplied g=a*x1+b initial1,1 target1.5,-.5 F=.75*x*x-.5*x; cap512; bounds[-8,8] initialstep1 min1e-8 sweeps128 proposals8192 no target; constant_bound8\n";
        protocol.close();auto p=problem();
        std::ofstream data(output/"training.csv");data<<std::setprecision(17)<<"x,target\n";for(const auto& row:p->rows())data<<row.inputs[0]<<','<<row.target<<'\n';data.close();
        SymbolicRegressionConfig cfg;cfg.population=smoke?8:128;cfg.elites=2;cfg.maxGenerationAttempts=smoke?3:2000;cfg.maxEvaluations=smoke?32:10000;cfg.maxTrainingLossAttempts=cfg.maxEvaluations;cfg.trainingLossThreshold=1e-20;
        genetik_example::printExecution(execution,cfg.population);
        struct Frozen{unsigned seed;IntegralRegressionResult result;};std::vector<Frozen> frozen;
        const auto seeds=smoke?std::vector<unsigned>{0}:std::vector<unsigned>{0,1,2,3,42};
        std::ofstream search(output/"search.txt");search<<std::setprecision(17);
        for(unsigned seed:seeds) {
            RandomContext random(seed,execution);const auto start=std::chrono::steady_clock::now();auto r=integralRegression(p,cfg,random);
            const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();const auto& s=r.search;
            search<<"seed="<<seed<<" initial="<<s.initialBestMSE.value_or(-1)<<" final="<<(s.best?s.best->fitness():-1)<<" discovery="<<(s.bestGeneration?std::to_string(*s.bestGeneration):"none")
              <<" published="<<s.publishedGenerations<<" generation_attempts="<<s.attemptedGenerations<<" objective_attempts="<<s.trainingWork.lossAttempts<<" unused="<<cfg.maxTrainingLossAttempts-s.trainingWork.lossAttempts
              <<" unpublished="<<s.unpublishedLossAttempts<<" seconds="<<seconds<<" stop="<<searchStopName(s.stoppingReason);work(search,r.integralWork);
            search<<" mutation_proposals="<<s.variation.mutationProposals<<" mutation_changes="<<s.variation.mutationChanges<<" crossover_proposals="<<s.variation.crossoverProposals<<" crossover_changes="<<s.variation.crossoverChanges
              <<" rejections="<<s.variation.structuralRejections<<" mutation_fallbacks="<<s.variation.mutationFallbacks<<" crossover_fallbacks="<<s.variation.crossoverFallbacks<<'\n';
            if(s.best)search<<"integrand="<<s.best->text()<<" nodes="<<s.best->expression().nodeCount()<<" depth="<<s.best->expression().depth()<<"\nmodel="<<p->model(s.best->expression()).toString()<<'\n';
            for(auto t:s.improvements)search<<"trace="<<seed<<','<<t.lossAttempts<<','<<t.generation<<','<<t.mse<<'\n';
            frozen.push_back({seed,std::move(r)});
        }
        std::optional<std::size_t> representative;RegressionOrdering order;
        for(std::size_t i=0;i<frozen.size();++i)if(frozen[i].result.search.best && (!representative || order.better(*frozen[i].result.search.best,*frozen[*representative].result.search.best,ObjectiveDirection::Minimize)))representative=i;
        search<<"all_models_frozen representative_seed="<<(representative?std::to_string(frozen[*representative].seed):"none")<<'\n';search.close();
        std::ofstream report(output/"assessment.txt");report<<std::setprecision(17);
        for(const auto& run:frozen) {
            report<<"seed="<<run.seed<<'\n';if(!run.result.search.best){report<<"no_model\n";continue;}const auto& g=run.result.search.best->expression();
            assess(report,*p,g,trainingPoints,"training");assess(report,*p,g,interior,"interior");assess(report,*p,g,exterior,"exterior");assess(report,*p,g,{0},"zero");
        }
        auto fittingProblem=problem(1.5,-.5,8);IntegralTrainingObjective objective(fittingProblem,512);ConstantFitConfig fit;fit.maxLossAttempts=512;const auto initial=parseExpression("1*x1+1",2);
        auto f=fitConstants(initial,objective,fit);const auto initialLoss=f.startingLoss; // independent work, no assessment data
        std::ofstream fitted(output/"fitting.txt");fitted<<std::setprecision(17)<<"initial="<<initial.toString()<<" MSE="<<initialLoss.score()<<"\nfinal="<<f.expression.toString()<<" MSE="<<f.loss.score()<<" a="<<f.expression.nodes()[0].constantValue()<<" b="<<f.expression.nodes()[3].constantValue()
          <<" attempts="<<objective.work().lossAttempts<<" unused="<<512-objective.work().lossAttempts<<" accepted="<<objective.work().accepted<<" stop="<<constantFitStopName(f.stop);work(fitted,objective.integralWork());fitted<<"\nmodel="<<fittingProblem->model(f.expression).toString()<<'\n';
        const auto verification=fittingProblem->assess(f.expression); // separate bounded reporting, not a fitter probe
        if(verification.loss!=f.loss)throw std::logic_error("accepted integral fit/score mismatch");
        fitted<<"verification_reporting rows="<<verification.rowsAttempted;work(fitted,verification.work);fitted<<'\n';
        assess(fitted,*fittingProblem,f.expression,trainingPoints,"training",1.5,-.5);assess(fitted,*fittingProblem,f.expression,interior,"interior",1.5,-.5);assess(fitted,*fittingProblem,f.expression,exterior,"exterior",1.5,-.5);
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
