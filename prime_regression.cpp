#include "examples/PrimeRegression.hpp"
#include <chrono>
using namespace prime_example;
using Clock=std::chrono::steady_clock;
static double seconds(Clock::time_point start){return std::chrono::duration<double>(Clock::now()-start).count();}
int main(int argc,char** argv) {
    std::cout.imbue(std::locale::classic());std::cout<<std::setprecision(17)<<std::unitbuf;
    try {
        std::string mode,prefix;std::filesystem::path directory;std::optional<std::uint64_t> budget;
        std::uint64_t fitEvery=1;
        std::set<std::string> options;
        for(int i=1;i<argc;++i) {
            const std::string option=argv[i];if(!options.insert(option).second)throw std::invalid_argument("repeated option "+option);
            if(option=="--generate" || option=="--pilot" || option=="--campaign"){
                if(!mode.empty())throw std::invalid_argument("choose one mode");
                mode=option;continue;
            }
            if(option=="--help") {
                std::cout<<"genetik_prime_regression (--generate | --pilot | --campaign) --output DIR [--prime-file FIRST1000.txt] [--loss-budget 50000|100000] [--fit-every N]\n";return 0;
            }
            if(option!="--output" && option!="--prime-file" && option!="--loss-budget" && option!="--fit-every")throw std::invalid_argument("unknown option "+option);
            if(++i==argc)throw std::invalid_argument("missing value for "+option);
            const std::string value=argv[i];
            if(option=="--fit-every") {
                auto parsed=std::from_chars(value.data(),value.data()+value.size(),fitEvery);
                if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || fitEvery==0)throw std::invalid_argument("--fit-every: expected positive integer");
            }
            if(option=="--output")directory=value;
            if(option=="--prime-file")prefix=value;
            if(option=="--loss-budget") {
                if(value=="50000")budget=50000;else if(value=="100000")budget=100000;
                else throw std::invalid_argument("campaign loss allowance must be 50000 or 100000");
            }
        }
        if(mode.empty() || directory.empty() || (mode=="--campaign")!=budget.has_value())throw std::invalid_argument("use --help for required mode/output/budget");
        auto ref=reference();
        if(!prefix.empty()) {
            const auto supplied=loadPrefix(prefix,ref);
            std::copy(supplied.begin(),supplied.end(),ref.begin());
        }
        exportData(directory,ref);
        std::cout<<"reference=bounded_sieve_10000 independent_check=trial_division anchors=OEIS_A000040 data_source="<<(prefix.empty()?"local_generation":prefix)
                 <<" x0=n/1000.0 target=prime/10000.0 predicted_prime=10000.0*g(x0) fitting_indices=1..1000 extrapolation_indices=1001..1200\n";
        if(mode=="--generate")return 0;
        const auto problem=RegressionProblem::create(rows(ref),{31,6,8});
        if(mode=="--pilot") {
            auto c=config(4096);c.fitEveryGenerations=fitEvery;regression_example::printConfig(c);
            RandomContext random(2026);ExpressionVariation variation(problem,c.grammar);TrainingObjective objective(problem,4096);
            const auto start=Clock::now();std::size_t completedFits=0;
            try {
                for(;completedFits<512 && objective.remaining();++completedFits) {
                    const auto candidate=variation.initialize(random.engine());
                    (void)fitConstants(candidate.expression(),objective,*c.fitting);
                }
            } catch(const TrainingLossBudgetExceeded&) {} // Expected bounded standalone pilot stop.
            const auto w=objective.work();const auto elapsed=seconds(start);const auto full=w.lossAttempts-w.invalidLosses;
            if(full<128)throw std::runtime_error("pilot lacks 128 valid full-dataset calls; do not estimate from cheap invalids");
            const double estimate=6.0*100000.0*elapsed/static_cast<double>(full)*2.0;
            std::cout<<"pilot_seed=2026 pilot_seconds="<<elapsed<<" loss_attempts="<<w.lossAttempts<<" row_attempts="<<w.rowAttempts
                     <<" valid_full_dataset_calls="<<full<<" invalid_losses="<<w.invalidLosses<<" fit_invocations="<<w.fittingInvocations
                     <<" completed_fits="<<completedFits<<" conservative_six_run_100000_seconds="<<estimate
                     <<" margin=2 recommended_allowance="<<(estimate<=1800?100000:50000)<<" pilot_models_discarded=1\n";return 0;
        }
        auto c=config(*budget);c.fitEveryGenerations=fitEvery;regression_example::printConfig(c);
        std::cout<<"seeds=0,1,2,3,42 representative_order=fitting_mse,nodes,faithful_text,seed no_configuration_adjustment=1\n";
        // Fit references and select ALL committed models before any later-range predictions.
        const auto references=baselines(rows(ref));
        auto baselineFile=output(directory/"baselines.txt");
        for(const auto& b:references) {
            baselineFile<<b.name<<" coefficients=";for(auto value:b.coefficients)baselineFile<<value<<',';
            if(b.normalized)baselineFile<<" normalized_expression="<<b.normalized->toString()<<" representable=1 nodes="<<b.normalized->nodeCount()<<" depth="<<b.normalized->depth();
            else baselineFile<<" actual_raw_form=n*std::log(n) fitted_parameters=0 normalized_family_representable=1_algebraically_not_bitwise g=(0.1*x0)*(log(x0)+log(1000))";
            baselineFile<<'\n';
        }
        struct Run {std::uint32_t seed;SymbolicRegressionResult result;};std::vector<Run> runs;
        auto frozen=output(directory/"frozen-models.txt");
        std::optional<std::size_t> representative;
        for(auto seed:seeds) {
            RandomContext random(seed);const auto start=Clock::now();
            auto result=symbolicRegression(problem,c,random);
            std::cout<<"seed="<<seed<<'\n';regression_example::printResult(result,*budget,seconds(start));
            if(result.best) {
                std::size_t constants=0;for(auto node:result.best->expression().nodes())if(node.kind()==ExpressionKind::Constant)++constants;
                std::cout<<"constant_occurrences="<<constants<<'\n';
                frozen<<"seed="<<seed<<" normalized_mse="<<result.best->fitness()<<" expression="<<result.best->text()<<'\n';
            } else frozen<<"seed="<<seed<<" no_committed_model=1\n";
            auto trace=output(directory/("seed-"+std::to_string(seed)+"-trace.csv"));trace<<"loss_attempts,generation,normalized_mse\n";
            for(auto entry:result.improvements)trace<<entry.lossAttempts<<','<<entry.generation<<','<<entry.mse<<'\n';
            runs.push_back({seed,std::move(result)});
            const auto i=runs.size()-1;
            if(runs[i].result.best && (!representative || better(runs[i].result,seed,runs[*representative].result,runs[*representative].seed)))representative=i;
        }
        if(representative)frozen<<"representative_seed="<<runs[*representative].seed<<'\n';
        frozen.close();baselineFile.close();
        std::cout<<"all_models_and_baselines_frozen=1 representative_seed=";
        if(representative)std::cout<<runs[*representative].seed;else std::cout<<"none";
        std::cout<<" baseline_qr_fits=4 baseline_design_rows=4000 baseline_search_loss_calls=0\n";
        std::size_t reportingPredictions=0;
        const auto assess=[&](const std::string& name,const auto& predict,bool selected) {
            std::vector<Prediction> fitting,extrapolation;
            for(std::size_t n=1;n<=1200;++n) {
                ++reportingPredictions;auto p=prediction(n,ref[n-1],predict(n));
                (n<=1000?fitting:extrapolation).push_back(p);
            }
            printMetrics(name,"fitting",metrics(fitting));printMetrics(name,"extrapolation",metrics(extrapolation));
            if(selected)writePredictions(directory/"representative-first-1000.csv",fitting);
            fitting.insert(fitting.end(),extrapolation.begin(),extrapolation.end());writePredictions(directory/(name+"-predictions.csv"),fitting);
        };
        for(std::size_t i=0;i<runs.size();++i)if(runs[i].result.best) {
            const auto& expression=runs[i].result.best->expression();
            assess("seed-"+std::to_string(runs[i].seed),[&](std::size_t n){return decode(expression,n);},representative && i==*representative);
        }
        for(const auto& b:references)assess(b.name,[&](std::size_t n){return b.predict(n);},false);
        std::cout<<"postsearch_prediction_rows="<<reportingPredictions<<" reporting_search_loss_calls=0 baseline_and_postsearch_work_separate=1\n";
        return 0;
    }catch(const std::exception& e){regression_example::printError(e);return 1;}
}
