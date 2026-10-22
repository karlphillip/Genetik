#pragma once
#include "ExecutionOptions.hpp"
#include "PrimeExactSearch.hpp"
namespace prime_example {
inline bool exactModeRequested(int argc,char** argv) {
    for(int i=1;i<argc;++i) {
        const std::string_view option=argv[i];
        if(option=="--search-exact")return true;
        for(auto name:{"--output","--prime-file","--loss-budget","--fit-every","--seed","--threads","--top","--report-seconds"})
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
    std::size_t top=5;
    double reportSeconds=30;
    ExecutionConfig execution;
    std::string primeFile;
    std::filesystem::path outputDirectory;
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
           option!="--top" && option!="--fit-every" && option!="--prime-file" && option!="--output")
            throw std::invalid_argument("unknown exact-search option "+option);
        if(++i==argc)throw std::invalid_argument("missing value for "+option);
        const std::string_view value=argv[i];
        if(option=="--seed")options.seed=exactUnsigned<std::uint32_t>(value,option,false);
        if(option=="--threads")options.execution=ExecutionConfig::parse(value);
        if(option=="--loss-budget")options.budget=exactUnsigned<std::uint64_t>(value,option);
        if(option=="--top")options.top=exactUnsigned<std::size_t>(value,option);
        if(option=="--fit-every")options.fitEvery=exactUnsigned<std::uint64_t>(value,option);
        if(option=="--prime-file")options.primeFile=value;
        if(option=="--output")options.outputDirectory=value;
        if(option=="--report-seconds") {
            const auto parsed=std::from_chars(value.data(),value.data()+value.size(),options.reportSeconds);
            if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || !std::isfinite(options.reportSeconds) ||
               options.reportSeconds<=0 || options.reportSeconds>86400 ||
               std::chrono::duration_cast<ExactClock::duration>(std::chrono::duration<double>(options.reportSeconds))<=ExactClock::duration::zero())
                throw std::invalid_argument("--report-seconds: expected positive representable seconds <=86400");
        }
    }
    return options;
}
inline void exactHelp(std::ostream& out) {
    out<<"genetik_prime_regression --search-exact [--seed 42] [--threads N] [--loss-budget 1000000]\n"
          "  [--report-seconds 30] [--top 5] [--fit-every N] [--no-fit] [--prime-file FIRST1000.txt] [--output DIR]\n"
          "Bounded exact rounded-prefix search, current prime grammar/population/fitter; no target formula is injected.\n"
          "Reports initial/final committed populations and at the first successful publication after each deadline.\n"
          "Long generations may delay reports. No reporter thread; deadlines stay aligned with search start.\n"
          "report-seconds accepts finite positive fractional seconds through86400; top is a positive integer, capped by available individuals.\n"
          "Ctrl+C requests cooperative cancellation; admitted worker work drains and unpublished state is discarded.\n";
}
inline int exactMain(int argc,char** argv) {
    const auto options=exactOptions(argc,argv);
    if(options.help){exactHelp(std::cout);return 0;}
    auto ref=reference();
    if(!options.primeFile.empty())(void)loadPrefix(options.primeFile,ref);
    if(!options.outputDirectory.empty())exportData(options.outputDirectory,ref);
    ref.resize(1000);
    auto cfg=config(options.budget);cfg.fitEveryGenerations=options.fitEvery;
    if(options.noFit)cfg.fitting.reset();
    ExactSignalGuard signals;
    auto problem=std::make_shared<const ExactPrefixProblem>(ref,ExpressionLimits{31,6,8},&exactInterrupt);
    RandomContext random(options.seed,options.execution);
    std::cout<<"mode=search-exact objective=mismatches+sum_wrong(error/(1+error))/1000 recovery=1000/1000\n"
             <<"x0=double(n)/1000.0 target=double(prime[n])/10000.0 decoded=10000*g(x0) indices=1..1000 rounding=halfway_away_from_zero\n"
             <<"seed="<<options.seed<<" report_seconds="<<options.reportSeconds<<" top="<<options.top<<" cancellation=SIGINT_cooperative\n";
    genetik_example::printExecution(options.execution,cfg.population);regression_example::printConfig(cfg);
    const auto result=runExactSearch(problem,cfg,random,std::cout,options.top,
        std::chrono::duration_cast<ExactClock::duration>(std::chrono::duration<double>(options.reportSeconds)));
    std::cout<<"search_complete stopping_reason="<<searchStopName(result.stop)
             <<" training_loss_attempts="<<result.training.lossAttempts<<" training_rows="<<result.training.rowAttempts
             <<" ordinary_loss_attempts="<<result.training.ordinaryLossAttempts<<" fitting_baselines="<<result.training.fittingBaselineAttempts
             <<" fitting_probes="<<result.training.fittingProbeAttempts<<" fits="<<result.training.fittingInvocations
             <<" accepted_fit_steps="<<result.training.accepted<<" mutation_changes="<<result.variation.mutationChanges
             <<" crossover_changes="<<result.variation.crossoverChanges<<" unpublished_loss_attempts="<<result.unpublishedLosses
             <<" reporting_snapshots="<<result.reporting.snapshots<<" reporting_prediction_rows="<<result.reporting.predictionRows
             <<" reporting_loss_attempts=0 exact_recovered="<<(result.stop==SearchStop::ExactTrainingFit)<<'\n';
    return 0; // Bounded budget/cancellation are normal outcomes, not exact-fit claims.
}
} // namespace prime_example
