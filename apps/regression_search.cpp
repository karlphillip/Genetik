#include "examples/ExecutionOptions.hpp"
#include "examples/RegressionData.hpp"
#include "examples/RegressionWorkflow.hpp"
#include <chrono>

using namespace regression_example;
static std::uint64_t integer(const std::string& text,std::uint64_t maximum,const std::string& name) {
    std::uint64_t value=0;
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || value>maximum)
        throw std::invalid_argument(name+": expected an unsigned decimal integer <= "+std::to_string(maximum));
    return value;
}
static void usage() {
    std::cout<<"Usage: genetik_regression_search TRAIN.csv --inputs first,second --target target --seed N --loss-budget N [--loss-threshold MSE] [--threads N] [--fit-every N] [--no-fit] [--operators add,sub,mul,div,log] [--evaluate FILE ...]\n"
             <<"Input order defines x0,x1,...; budget is actual training-loss attempts, 0..10000000.\n";
}
int main(int argc,char** argv) {
    std::cout<<std::setprecision(17);
    std::shared_ptr<TrainingObjective> objective;
    try {
        const auto execution=genetik_example::executionOptions(argc,argv,true);
        genetik_example::printExecution(execution, 64);
        if(argc==2 && std::string(argv[1])=="--help"){usage();return 0;}
        if(argc<2){usage();return 1;}
        std::vector<std::string> inputs,evaluations;
        std::string target;
        std::optional<std::uint32_t> seed;
        std::optional<std::uint64_t> budget;
        bool fitting=true;std::uint64_t fitEvery=1;
        std::optional<double> threshold;
        std::optional<std::vector<ExpressionKind>> operators;
        std::set<std::string> seen;
        for(int i=2;i<argc;++i) {
            const std::string option=argv[i];
            if(option!="--evaluate" && !seen.insert(option).second)throw std::invalid_argument("repeated option "+option);
            if(option=="--no-fit"){fitting=false;continue;}
            if(option!="--inputs" && option!="--target" && option!="--seed" && option!="--loss-budget" && option!="--evaluate" && option!="--operators" && option!="--loss-threshold" && option!="--fit-every")
                throw std::invalid_argument("unknown option "+option);
            if(++i==argc)throw std::invalid_argument("missing value for "+option);
            const std::string value=argv[i];
            if(option=="--fit-every") {
                fitEvery=integer(value,std::numeric_limits<std::uint64_t>::max(),option);
                if(fitEvery==0)throw std::invalid_argument("--fit-every must be positive");
            }
            if(option=="--loss-threshold") {
                double v=0;auto parsed=std::from_chars(value.data(),value.data()+value.size(),v);
                if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || !std::isfinite(v) || v<0)throw std::invalid_argument("--loss-threshold: expected finite nonnegative MSE");
                threshold=v;
            }
            if(option=="--operators")operators=selectOperators(splitColumns(value));
            if(option=="--inputs")inputs=splitColumns(value);
            if(option=="--target")target=std::string(trim(value));
            if(option=="--seed")seed=static_cast<std::uint32_t>(integer(value,std::numeric_limits<std::uint32_t>::max(),option));
            if(option=="--loss-budget")budget=integer(value,10000000,option);
            if(option=="--evaluate"){
                if(evaluations.size()==8)throw std::invalid_argument("at most 8 evaluation files");
                evaluations.push_back(value);
            }
        }
        if(inputs.empty() || target.empty() || !seed || !budget)throw std::invalid_argument("--inputs, --target, --seed and --loss-budget are required");
        const auto training=loadData(argv[1],inputs,target);
        std::vector<LoadedData> heldout;
        // Validate all supplied files before starting, but never pass evaluation rows to search.
        for(const auto& path:evaluations)heldout.push_back(loadData(path,inputs,target));
        const auto problem=RegressionProblem::create(training.rows,{31,6,8});
        auto config=searchConfig(*budget,fitting);
        config.fitEveryGenerations=fitEvery;
        if (operators) config.grammar.operators=*operators;
        if(threshold){config.trainingLossThreshold=*threshold;if(config.fitting)config.fitting->lossThreshold=*threshold;}
        printConfig(config);
        const TableLimits limits;
        std::cout<<"file_byte_limit="<<limits.maxBytes<<" line_byte_limit="<<limits.maxLineBytes
                 <<" file_row_limit="<<limits.maxRows<<" column_limit="<<limits.maxColumns
                 <<" header_name_byte_limit="<<limits.maxNameBytes<<" number_byte_limit="<<limits.maxNumberBytes<<'\n';
        std::cout<<"training_file="<<argv[1]<<" training_rows="<<training.rows.size()<<" dimensions="<<inputs.size()
                 <<" target_column="<<target<<" target_file_column="<<training.targetColumn+1<<" seed="<<*seed<<'\n';
        for(std::size_t i=0;i<inputs.size();++i)std::cout<<"x"<<i<<"="<<inputs[i]<<" training_file_column="<<training.inputColumns[i]+1<<'\n';
        RandomContext random(*seed,execution);
        objective=std::make_shared<TrainingObjective>(problem,*budget);
        std::cout<<"search_start\n";
        const auto start=std::chrono::steady_clock::now();
        const auto result=symbolicRegression(problem,config,random,objective);
        const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        printResult(result,*budget,seconds);
        if(!result.best)return 2;
        // Candidate selection is finalized; cached training MSE is not recomputed here.
        printPredictionReport("training",predictionReport(result.best->expression(),training.rows));
        std::cout<<"selection_finalized evaluation_files="<<heldout.size()<<'\n';
        for(std::size_t i=0;i<heldout.size();++i) {
            const auto reporting=evaluationReport(result.best->expression(),heldout[i].rows);
            const auto& metrics=reporting.metrics;
            const auto& predictions=reporting.predictions;
            std::cout<<"evaluation_file="<<evaluations[i]<<" target_column="<<target<<" target_file_column="<<heldout[i].targetColumn+1<<" mse=";
            if(metrics.loss.state()==FitnessState::Valid)std::cout<<metrics.loss.score();else std::cout<<"invalid";
            std::cout<<" max_abs_error=";
            if(metrics.maxAbsoluteError)std::cout<<*metrics.maxAbsoluteError;else std::cout<<"invalid";
            std::cout<<" recovery="<<(metrics.maxAbsoluteError && *metrics.maxAbsoluteError<=1e-6)
                     <<" postsearch_evaluation_loss_calls=1 reporting_loss_rows="<<metrics.samplesAttempted
                     <<" reporting_prediction_rows="<<predictions.rows
                     <<" invalid_predictions_total="<<predictions.invalid<<" invalid_errors_total="<<predictions.invalidErrors<<'\n';
            for(std::size_t j=0;j<inputs.size();++j)std::cout<<"evaluation_x"<<j<<"="<<inputs[j]<<" file_column="<<heldout[i].inputColumns[j]+1<<'\n';
        }
        return 0;
    } catch(const std::exception& error) {
        printError(error);
        if(objective)std::cerr<<"attempted_training_losses="<<objective->work().lossAttempts<<" attempted_rows="<<objective->work().rowAttempts<<'\n';
        return 1;
    }
}
