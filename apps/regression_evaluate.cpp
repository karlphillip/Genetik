#include "examples/ExpressionTextFile.hpp"
#include "examples/RegressionData.hpp"
#include "examples/RegressionWorkflow.hpp"

int main(int argc,char** argv) {
    using namespace regression_example;
    std::string formula;
    try {
        if(argc<2 || std::string_view(argv[1])=="--help") {
            std::cout<<"Usage: genetik_regression_evaluate TRAIN.csv --expression FORMULA.txt --inputs first,second --target target [--evaluate FILE ...] [--fit] [--fit-budget N]\n"
                     <<"Synchronous supplied-formula evaluation; never starts evolution. x0,x1 follow selected column order. Expression file limit=1048576 bytes.\n";
            return argc<2 ? 1:0;
        }
        std::vector<std::string> inputs,evaluations;std::string target;bool fitting=false;
        std::uint64_t allowance=256;std::set<std::string> seen;
        for(int i=2;i<argc;++i) {
            const std::string option=argv[i];
            if(option!="--evaluate" && !seen.insert(option).second)throw std::invalid_argument("repeated option "+option);
            if(option=="--fit"){fitting=true;continue;}
            if(option!="--expression" && option!="--inputs" && option!="--target" && option!="--evaluate" && option!="--fit-budget")throw std::invalid_argument("unknown option "+option);
            if(++i==argc)throw std::invalid_argument("missing option value");
            const std::string value=argv[i];
            if(option=="--expression")formula=value;
            if(option=="--inputs")inputs=splitColumns(value);
            if(option=="--target")target=value;
            if(option=="--evaluate") {if(evaluations.size()>=8)throw std::invalid_argument("at most8 evaluation files");evaluations.push_back(value);}
            if(option=="--fit-budget") {
                const auto r=std::from_chars(value.data(),value.data()+value.size(),allowance);
                if(r.ec!=std::errc{} || r.ptr!=value.data()+value.size() || !allowance || allowance>1000000)throw std::invalid_argument("fit budget must be in [1,1000000]");
            }
        }
        if(formula.empty()||inputs.empty()||target.empty())throw std::invalid_argument("expression, inputs and target are required");
        if(seen.contains("--fit-budget")&&!fitting)throw std::invalid_argument("fit-budget requires explicit --fit");
        const auto training=loadData(argv[1],inputs,target);std::vector<LoadedData> heldout;
        for(const auto& p:evaluations)heldout.push_back(loadData(p,inputs,target));
        const ExpressionParseLimits limits;
        auto expression=parseExpression(genetik_example::readExpressionFile(formula,limits.maxSourceBytes),inputs.size(),limits);
        std::cout<<std::setprecision(17)<<"mode=supplied_expression evolution=false fitting="<<fitting<<" source_byte_limit="<<limits.maxSourceBytes<<'\n';
        for(std::size_t i=0;i<inputs.size();++i)std::cout<<"x"<<i<<'='<<inputs[i]<<" training_file_column="<<training.inputColumns[i]+1<<'\n';
        std::cout<<"target="<<target<<" imported_expression="<<expression.toString()<<'\n';
        if(fitting) {
            auto problem=RegressionProblem::create(training.rows,expression.limits());TrainingObjective objective(problem,allowance);
            ConstantFitConfig config;config.maxLossAttempts=allowance;
            auto result=fitConstants(expression,objective,config);expression=std::move(result.expression);
            std::cout<<"fit_bounds=[-8,8] fit_step=1 fit_min_step=1e-8 fit_sweeps=128 fit_proposals=8192 fit_loss_attempts="<<objective.work().lossAttempts
                     <<" fit_rows="<<objective.work().rowAttempts<<" improved="<<result.improved<<" fit_stop="<<constantFitStopName(result.stop)<<'\n';
        }
        std::cout<<"finalized_expression="<<expression.toString()<<" nodes="<<expression.nodeCount()<<" depth="<<expression.depth()<<'\n';
        auto report=[&](const std::string& label,const LoadedData& data) {
            const auto r=evaluationReport(expression,data.rows,expression.limits());
            std::cout<<"report="<<label<<" mse=";
            if(r.metrics.loss.state()==FitnessState::Valid)std::cout<<r.metrics.loss.score();else std::cout<<"invalid";
            std::cout<<" max_abs_error=";
            if(r.metrics.maxAbsoluteError)std::cout<<*r.metrics.maxAbsoluteError;else std::cout<<"invalid";
            std::cout<<" prediction_rows="<<r.predictions.rows<<" invalid_predictions_total="<<r.predictions.invalid
                     <<" invalid_errors_total="<<r.predictions.invalidErrors<<" reporting_loss_rows="<<r.metrics.samplesAttempted<<" reporting_loss_calls=1\n";
        };
        report("training",training);
        for(std::size_t i=0;i<heldout.size();++i)report(evaluations[i],heldout[i]);
        return 0;
    } catch(const ExpressionParseError& e) {std::cerr<<formula<<": parse_category="<<static_cast<int>(e.category())<<": "<<e.what()<<'\n';return 1;}
      catch(const std::exception& e){printError(e);return 1;}
}
