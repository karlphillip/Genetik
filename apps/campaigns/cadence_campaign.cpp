#include "examples/ExecutionOptions.hpp"
#include "examples/PrimeRegression.hpp"
#include "examples/StrictArithmeticData.hpp"
#include <chrono>
#include <sstream>
using namespace regression_example;
using Clock=std::chrono::steady_clock;
struct CadenceRun {
    std::string problem;unsigned seed,interval;SymbolicRegressionResult result;double seconds;std::string rng;
};
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static bool same(const CadenceRun& a,const CadenceRun& b) {
    const auto& x=a.result;const auto& y=b.result;
    return x.best.has_value()==y.best.has_value() && (!x.best || (x.best->text()==y.best->text() && x.best->evaluation()==y.best->evaluation())) &&
        x.rawInitialExpressions==y.rawInitialExpressions && x.preparedInitialExpressions==y.preparedInitialExpressions && x.preparedInitialFitness==y.preparedInitialFitness &&
        x.initialWork==y.initialWork && x.initialBestMSE==y.initialBestMSE && x.rawInitialBestMSE==y.rawInitialBestMSE &&
        x.trainingWork==y.trainingWork && x.preparationWork==y.preparationWork && x.variation==y.variation && x.bestGeneration==y.bestGeneration &&
        x.candidateEvaluations==y.candidateEvaluations && x.publishedGenerations==y.publishedGenerations && x.attemptedGenerations==y.attemptedGenerations &&
        x.stoppingReason==y.stoppingReason && a.rng==b.rng;
}
static void details(const CadenceRun& run,std::ostream& out) {
    const auto& r=run.result;const auto& w=r.trainingWork;
    out<<"problem="<<run.problem<<" seed="<<run.seed<<" interval="<<run.interval<<'\n';
    // Shared reporting has all legacy work counters, expression and complexity.
    auto* previous=std::cout.rdbuf(out.rdbuf());printResult(r,100000,run.seconds);std::cout.rdbuf(previous);
    out<<"initial_loss_calls="<<r.initialWork.lossAttempts<<" initial_rows="<<r.initialWork.rowAttempts
       <<" completed_improved="<<r.preparationWork.completedImproved<<" completed_unchanged="<<r.preparationWork.completedUnchanged
       <<" no_constants="<<r.preparationWork.noConstants<<" invalid_starts="<<r.preparationWork.invalidStarts<<" aborted_fits="<<r.preparationWork.abortedFits
       <<" disjoint_check="<<(w.lossAttempts==w.ordinaryLossAttempts+w.fittingBaselineAttempts+w.fittingProbeAttempts)<<'\n';
    for(std::size_t i=0;i<5;++i) {
        const auto& s=r.preparationWork.origins[i];
        out<<"origin="<<std::array{"initialization","changed_offspring","unchanged_offspring","elite","unspecified"}[i]
           <<" requests="<<s.requests<<" fits="<<s.fits<<" skips="<<s.skipped<<" ordinary="<<s.ordinary<<" baselines="<<s.baselines
           <<" probes="<<s.probes<<" rows="<<s.rows<<" invocations_with_progress="<<s.improved<<'\n';
    }
    for(std::size_t i=0;i<7;++i)out<<"fit_stop="<<constantFitStopName(static_cast<ConstantFitStop>(i))<<" count="<<r.preparationWork.terminations[i]<<'\n';
}
static void profile(const CadenceRun& run,const RefinementDiagnostics& d,std::ostream& out) {
    details(run,out);
    out<<"profile_search_seconds="<<run.seconds<<" dataset_seconds="<<d.datasetSeconds
       <<" fitting_inclusive_seconds="<<d.fittingSeconds<<" fitting_dataset_seconds="<<d.fittingDatasetSeconds
       <<" fitting_overhead_seconds="<<d.fittingSeconds-d.fittingDatasetSeconds
       <<" preparation_inclusive_seconds="<<d.preparationSeconds<<" generation_inclusive_seconds="<<d.generationSeconds
       <<" publication_seconds="<<d.publicationSeconds
       <<" other_search_seconds="<<run.seconds-d.datasetSeconds-(d.fittingSeconds-d.fittingDatasetSeconds)-d.publicationSeconds<<'\n';
    out<<"diagnostic_capacity="<<d.capacity<<" stored_expressions="<<d.expressions.size()<<" observed_fits="<<d.observed<<" untracked_requests="<<d.untracked
       <<" capacity_drops="<<d.capacityDrops<<" repeated_input_requests="<<d.repeated<<" repeat_loss_attempts="<<d.repeatLosses
       <<" repeated_completed_nochange_loss_attempts="<<d.repeatNoChangeLosses<<" repeated_with_progress="<<d.repeatImproved
       <<" previous_output_requests="<<d.previousOutput<<" previous_output_loss_attempts="<<d.previousOutputLosses
       <<" previous_output_with_progress="<<d.previousOutputImproved<<" previous_output_completed_nochange="<<d.previousOutputUnchanged<<'\n';
}
int main(int argc,char** argv) {
    std::cout<<std::setprecision(17)<<std::unitbuf;
    try {
        const auto execution=genetik_example::executionOptions(argc,argv);
        genetik_example::printExecution(execution, 64);
        if(argc!=3 || std::string(argv[1])!="--output")throw std::invalid_argument("genetik_cadence_campaign --output NEW_DIRECTORY [--threads N] (serial: two profiles + twenty unprofiled comparisons)");
        const std::filesystem::path directory=argv[2];
        if(std::filesystem::exists(directory/"campaign.txt"))throw std::invalid_argument("choose a new output directory; preserve prior results");
        std::filesystem::create_directories(directory);
        const auto primes=prime_example::reference();const auto primeRows=prime_example::rows(primes);
        const auto arithmetic=arithmeticRows(0);
        auto log=prime_example::output(directory/"campaign.txt");
        log<<"threads_requested="<<execution.threads<<" threads_effective="<<std::min(execution.threads,64u)<<'\n';
        log<<"order=prime_then_arithmetic seeds=0,1,2,3,42 even_pair=1_then_4 odd_pair=4_then_1 diagnostics=off budget=100000\n";
        std::vector<CadenceRun> profiles,runs;
        const auto execute=[&](const std::string& name,unsigned seed,unsigned interval,std::shared_ptr<RefinementDiagnostics> diagnostic={}) {
            const bool prime=name=="prime";
            const auto problem=RegressionProblem::create(prime?primeRows:arithmetic,{31,6,8});
            auto config=prime_example::config(100000);if(!prime){config.trainingLossThreshold=1e-14;config.fitting->lossThreshold=1e-14;}
            config.fitEveryGenerations=interval;config.diagnostics=std::move(diagnostic);
            auto* previous=std::cout.rdbuf(log.rdbuf());printConfig(config);std::cout.rdbuf(previous);
            RandomContext random(seed,execution);const auto start=Clock::now();auto result=symbolicRegression(problem,config,random);
            const auto elapsed=std::chrono::duration<double>(Clock::now()-start).count();
            std::ostringstream rng;rng<<random.engine();return CadenceRun{name,seed,interval,std::move(result),elapsed,rng.str()};
        };
        if(execution.threads>1)log<<"detailed_profiles=unsupported_skipped parallel_work=unprofiled_runs\n";
        for(const std::string& name: execution.threads==1 ? std::vector<std::string>{"prime","arithmetic"}:std::vector<std::string>{}) {
            auto diagnostic=std::make_shared<RefinementDiagnostics>();auto run=execute(name,2,1,diagnostic);
            auto out=prime_example::output(directory/("profile-"+name+".txt"));profile(run,*diagnostic,out);profiles.push_back(std::move(run));
            std::cout<<"profile_complete="<<name<<'\n';
        }
        for(const std::string name:{"prime","arithmetic"})for(std::size_t pair=0;pair<prime_example::seeds.size();++pair) {
            const auto seed=prime_example::seeds[pair];const std::array<unsigned,2> intervals=pair%2 ? std::array{4u,1u}:std::array{1u,4u};
            for(auto interval:intervals) {
                auto run=execute(name,seed,interval);details(run,log);
                if(execution.threads==1 && seed==2 && interval==1) {
                    const auto& observed=profiles.at(name=="prime"?0:1);
                    require(same(run,observed),"diagnostics changed results, counters, preparation or RNG");log<<"profile_equivalence="<<name<<" verified=1\n";
                }
                auto trace=prime_example::output(directory/(name+"-"+std::to_string(seed)+"-"+std::to_string(interval)+"-trace.csv"));
                trace<<"loss_attempts,generation,mse\n";for(auto p:run.result.improvements)trace<<p.lossAttempts<<','<<p.generation<<','<<p.mse<<'\n';
                runs.push_back(std::move(run));std::cout<<"comparison_complete="<<name<<" seed="<<seed<<" interval="<<interval<<'\n';
            }
            const auto& a=runs[runs.size()-2].result;const auto& b=runs.back().result;
            require(a.rawInitialExpressions==b.rawInitialExpressions && a.preparedInitialExpressions==b.preparedInitialExpressions &&
                a.preparedInitialFitness==b.preparedInitialFitness && a.initialWork==b.initialWork && a.initialBestMSE==b.initialBestMSE,
                "paired initialization differs");
            log<<"paired_initialization="<<name<<" seed="<<seed<<" raw_prepared_fitness_work_equal=1\n";
        }
        // All training-based model and representative decisions finish before assessment.
        for(const std::string name:{"prime","arithmetic"})for(unsigned interval:{1u,4u}) {
            const CadenceRun* best=nullptr;
            for(const auto& r:runs)if(r.problem==name && r.interval==interval && (!best || prime_example::better(r.result,r.seed,best->result,best->seed)))best=&r;
            log<<"representative_problem="<<name<<" interval="<<interval<<" seed="<<(best?std::to_string(best->seed):"none")<<'\n';
        }
        log<<"all_models_frozen=1 prime_later_range=previously_examined_1001..1200\n";
        const auto interior=arithmeticRows(1),exterior=arithmeticRows(2);
        for(const auto& run:runs)if(run.result.best) {
            const auto name=run.problem+"-"+std::to_string(run.seed)+"-"+std::to_string(run.interval);
            const auto& expression=run.result.best->expression();auto* previous=std::cout.rdbuf(log.rdbuf());
            if(run.problem=="prime") {
                std::vector<prime_example::Prediction> fitting,later;
                for(std::size_t n=1;n<=1200;++n)(n<=1000?fitting:later).push_back(prime_example::prediction(n,primes[n-1],prime_example::decode(expression,n)));
                prime_example::printMetrics(name,"fitting",prime_example::metrics(fitting));prime_example::printMetrics(name,"examined_extrapolation",prime_example::metrics(later));
                fitting.insert(fitting.end(),later.begin(),later.end());prime_example::writePredictions(directory/(name+"-predictions.csv"),fitting);
            } else {
                printPredictionReport(name+"-training",predictionReport(expression,arithmetic));
                printPredictionReport(name+"-interior",predictionReport(expression,interior));
                printPredictionReport(name+"-exterior",predictionReport(expression,exterior));
            }
            std::cout.rdbuf(previous);
        }
        log<<"comparison_runs=20 profile_runs="<<profiles.size()<<" prime_reporting_rows=12000 arithmetic_reporting_rows=490\n";
        return 0;
    }catch(const std::exception& e){printError(e);return 1;}
}
