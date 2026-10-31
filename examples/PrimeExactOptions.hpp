#pragma once
#include "ExecutionOptions.hpp"
#include "PrimeExactSearch.hpp"
#include "ResearchPaths.hpp"
namespace prime_example {
inline bool exactModeRequested(int argc,char** argv) {
    for(int i=1;i<argc;++i) {
        const std::string_view option=argv[i];
        if(option=="--search-exact"||option=="--resume")return true;
        for(auto name:{"--output","--prime-file","--loss-budget","--fit-every","--seed","--threads","--top","--report-seconds","--report-format","--formula-precision","--metrics-file","--checkpoint","--checkpoint-seconds","--checkpoint-keep","--resume"})
            if(option==name && i+1<argc){++i;break;}
    }
    return false;
}
template<class UInt> inline UInt exactUnsigned(std::string_view text,std::string_view name,bool positive=true) {
    UInt value{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(text.empty() || parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || (positive && value==0))
        throw std::invalid_argument(std::string(name)+": expected "+(positive?"positive":"unsigned")+" integer");
    return value;
}
struct ExactOptions {
    std::uint32_t seed=42;
    std::uint64_t budget=1000000,fitEvery=1;
    std::size_t top=1,checkpointKeep=3;
    unsigned formulaPrecision=6;
    double reportSeconds=30,checkpointSeconds=300;
    ExecutionConfig execution;
    std::string primeFile;
    std::filesystem::path outputDirectory,metricsFile,checkpointDirectory,resumePath;
    std::string reportFormat="pretty";
    std::set<std::string> specified;
    bool noFit=false,help=false;
};
inline ExactOptions exactOptions(int argc,char** argv) {
    ExactOptions options;std::set<std::string> seen;
    for(int i=1;i<argc;++i) {
        const std::string option=argv[i];
        if(!seen.insert(option).second)throw std::invalid_argument("repeated option "+option);
        if(option=="--search-exact")continue;
        if(option=="--help"){options.help=true;continue;}
        if(option=="--no-fit"){options.noFit=true;continue;}
        if(option!="--seed" && option!="--threads" && option!="--loss-budget" && option!="--report-seconds" &&
           option!="--top" && option!="--fit-every" && option!="--prime-file" && option!="--output" && option!="--report-format" && option!="--formula-precision" && option!="--metrics-file" && option!="--checkpoint" && option!="--checkpoint-seconds" && option!="--checkpoint-keep" && option!="--resume")
            throw std::invalid_argument("unknown exact-search option "+option);
        if(++i==argc)throw std::invalid_argument("missing value for "+option);
        const std::string_view value=argv[i];
        if((option=="--checkpoint"||option=="--resume"||option=="--metrics-file")&&value.empty())throw std::invalid_argument(option+": empty path");
        if(option=="--seed")options.seed=exactUnsigned<std::uint32_t>(value,option,false);
        if(option=="--threads")options.execution=ExecutionConfig::parse(value);
        if(option=="--loss-budget")options.budget=exactUnsigned<std::uint64_t>(value,option);
        if(option=="--formula-precision"){options.formulaPrecision=exactUnsigned<unsigned>(value,option);if(options.formulaPrecision>17)throw std::invalid_argument("--formula-precision requires1..17 significant digits");}
        if(option=="--top")options.top=exactUnsigned<std::size_t>(value,option);
        if(option=="--fit-every")options.fitEvery=exactUnsigned<std::uint64_t>(value,option);
        if(option=="--prime-file")options.primeFile=value;
        if(option=="--output")options.outputDirectory=value;
        if(option=="--report-format"){options.reportFormat=value;if(value!="pretty"&&value!="raw")throw std::invalid_argument("--report-format requires pretty or raw");}
        if(option=="--metrics-file")options.metricsFile=value;
        if(option=="--checkpoint")options.checkpointDirectory=value;
        if(option=="--resume")options.resumePath=value;
        if(option=="--checkpoint-keep"){options.checkpointKeep=exactUnsigned<std::size_t>(value,option);if(options.checkpointKeep>10000)throw std::invalid_argument("--checkpoint-keep exceeds10000");}
        if(option=="--report-seconds"||option=="--checkpoint-seconds") {
            auto& seconds=option=="--report-seconds"?options.reportSeconds:options.checkpointSeconds;
            const auto parsed=std::from_chars(value.data(),value.data()+value.size(),seconds);
            if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || !std::isfinite(seconds) ||
               seconds<=0 || seconds>86400 ||
               std::chrono::duration_cast<ExactClock::duration>(std::chrono::duration<double>(seconds))<=ExactClock::duration::zero())
                throw std::invalid_argument(option+": expected positive representable seconds <=86400");
        }
    }
    if(options.reportFormat=="raw"&&!seen.contains("--top"))options.top=5;
    if(options.checkpointDirectory.empty()&&options.resumePath.empty()&&(seen.contains("--checkpoint-seconds")||seen.contains("--checkpoint-keep")))throw std::invalid_argument("checkpoint timing/retention requires --checkpoint or --resume");
    options.specified=std::move(seen);return options;
}
inline void exactHelp(std::ostream& out) {
    out<<"genetik_prime_regression --search-exact [--seed 42] [--threads N] [--loss-budget 1000000]\n"
          "  [--report-seconds 30] [--top 1] [--formula-precision 6] [--report-format pretty|raw] [--fit-every N] [--no-fit] [--prime-file FIRST1000.txt] [--output DIR]\n"
          "  [--checkpoint DIR] [--checkpoint-seconds 300] [--checkpoint-keep 3] [--resume FILE|DIR] [--metrics-file PATH]\n"
          "Formula precision1..17 controls display only; rounded equations use approx, canonical/JSONL remain full precision.\n"
          "Pretty output defaults to one best individual; explicit --top adds compact entries. Raw defaults to five.\n"
          "--loss-budget on resume is the TOTAL allowance, including previously consumed work; worker changes are rejected.\n"
          "Checkpoint at safe committed boundaries, including initial/final; SIGINT/SIGTERM drain and save, never serialize in a handler.\n"
          "SIGKILL/power loss cannot save a final checkpoint. POSIX fsync/rename persistence; paths are directories for --checkpoint.\n"
          "Bounded exact rounded-prefix search, current prime grammar/population/fitter; no target formula is injected.\n"
          "Reports initial/final committed populations and at the first successful publication after each deadline.\n"
          "Long generations may delay reports. No reporter thread; deadlines stay aligned with search start.\n"
          "report-seconds accepts finite positive fractional seconds through86400; top is a positive integer, capped by available individuals.\n"
          "Ctrl+C requests cooperative cancellation; admitted worker work drains and unpublished state is discarded.\n";
}
inline int exactMain(int argc,char** argv) {
    auto options=exactOptions(argc,argv);
    if(options.help){exactHelp(std::cout);return 0;}
    ExactRunOptions io;io.pretty=options.reportFormat=="pretty";io.formulaPrecision=options.formulaPrecision;
    if(!options.resumePath.empty()){
        if(std::filesystem::is_directory(options.resumePath))options.resumePath/="latest.gtkc";
        io.resume=loadScalarCheckpoint(options.resumePath);const auto saved=decodeExactRun(*io.resume);
        if(!options.specified.contains("--seed"))options.seed=saved.seed;
        if(!options.specified.contains("--threads"))options.execution=ExecutionConfig{saved.threads};
        if(!options.specified.contains("--loss-budget"))options.budget=saved.allowance;
        if(!options.specified.contains("--fit-every"))options.fitEvery=saved.fitEvery;
        if(!options.specified.contains("--no-fit"))options.noFit=!saved.fitting;
        if(options.seed!=saved.seed||options.execution.threads!=saved.threads)throw CheckpointError("resume seed/worker count differs");
        if(options.checkpointDirectory.empty()){options.checkpointDirectory=options.resumePath.parent_path();if(options.checkpointDirectory.empty())options.checkpointDirectory=researchRunPath(std::filesystem::path("prime-exact")/("seed"+std::to_string(options.seed))/"checkpoints");}
    }
    io.checkpointDirectory=options.checkpointDirectory;io.checkpointKeep=options.checkpointKeep;
    io.checkpointInterval=std::chrono::duration_cast<ExactClock::duration>(std::chrono::duration<double>(options.checkpointSeconds));
    std::ofstream metrics;
    if(!options.metricsFile.empty()){if(!options.metricsFile.parent_path().empty())std::filesystem::create_directories(options.metricsFile.parent_path());metrics.open(options.metricsFile,std::ios::app);metrics.imbue(std::locale::classic());if(!metrics)throw std::runtime_error("cannot open metrics file");io.metrics=&metrics;}
    auto ref=reference();
    if(!options.primeFile.empty())(void)loadPrefix(options.primeFile,ref);
    if(!options.outputDirectory.empty())exportData(options.outputDirectory,ref);
    ref.resize(1000);
    auto cfg=config(options.budget);cfg.fitEveryGenerations=options.fitEvery;
    if(options.noFit)cfg.fitting.reset();
    ExactSignalGuard signals;
    auto problem=std::make_shared<const ExactPrefixProblem>(ref,ExpressionLimits{31,6,8},&exactInterrupt);
    RandomContext random(options.seed,options.execution);
    if(!io.pretty){std::cout<<"mode=search-exact objective=mismatches+sum_wrong(error/(1+error))/1000 recovery=1000/1000\n"
             <<"x0=double(n)/1000.0 target=double(prime[n])/10000.0 decoded=10000*g(x0) indices=1..1000 rounding=halfway_away_from_zero\n"
             <<"seed="<<options.seed<<" report_seconds="<<options.reportSeconds<<" top="<<options.top<<" cancellation=SIGINT_cooperative\n";
    genetik_example::printExecution(options.execution,cfg.population);regression_example::printConfig(cfg);}
    else std::cout<<"Prime exact-prefix search • seed "<<options.seed<<" • "<<options.execution.threads<<" threads • loss allowance "<<exactCount(options.budget)<<"\n";
    const auto result=runExactSearch(problem,cfg,random,std::cout,options.top,
        std::chrono::duration_cast<ExactClock::duration>(std::chrono::duration<double>(options.reportSeconds)),true,[] {return ExactClock::now();},io);
    if(!io.pretty)std::cout<<"search_complete stopping_reason="<<searchStopName(result.stop)
             <<" training_loss_attempts="<<result.training.lossAttempts<<" training_rows="<<result.training.rowAttempts
             <<" ordinary_loss_attempts="<<result.training.ordinaryLossAttempts<<" fitting_baselines="<<result.training.fittingBaselineAttempts
             <<" fitting_probes="<<result.training.fittingProbeAttempts<<" fits="<<result.training.fittingInvocations
             <<" accepted_fit_steps="<<result.training.accepted<<" mutation_changes="<<result.variation.mutationChanges
             <<" crossover_changes="<<result.variation.crossoverChanges<<" unpublished_loss_attempts="<<result.unpublishedLosses
             <<" reporting_snapshots="<<result.reporting.snapshots<<" reporting_prediction_rows="<<result.reporting.predictionRows
             <<" reporting_loss_attempts=0 exact_recovered="<<(result.stop==SearchStop::ExactTrainingFit)<<'\n';
    else std::cout<<"Search stopped: "<<searchStopName(result.stop)<<" • "<<result.training.lossAttempts<<" actual loss attempts • "<<result.reporting.predictionRows<<" reporting rows (zero training attempts)\n";
    if(!result.checkpoint.empty())std::cout<<(io.pretty?"Saved checkpoint: ":"checkpoint=")<<result.checkpoint.string()<<'\n';
    return 0; // Bounded budget/cancellation are normal outcomes, not exact-fit claims.
}
} // namespace prime_example
