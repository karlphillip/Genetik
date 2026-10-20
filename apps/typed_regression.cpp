#include "examples/TypedRegressionData.hpp"
#include "examples/ExecutionOptions.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
using namespace typed_example;
static std::ofstream file(const std::filesystem::path& path){std::ofstream f(path);if(!f)throw std::runtime_error("cannot write "+path.string());f<<std::setprecision(17);return f;}
int main(int argc,char** argv){try{
    const auto execution=genetik_example::executionOptions(argc,argv);
    bool smoke=false;std::filesystem::path directory;
    for(int i=1;i<argc;++i){const std::string a=argv[i];if(a=="--smoke")smoke=true;else if(a=="--output" && i+1<argc)directory=argv[++i];else throw std::invalid_argument("usage: genetik_typed_regression --output NEW_DIR [--smoke] [--threads N]");}
    if(directory.empty() || std::filesystem::exists(directory/"results.txt"))throw std::invalid_argument("supply a new --output directory");
    std::filesystem::create_directories(directory);
    const auto training=data(0),interior=data(1),exterior=data(2);
    const auto problem=TypedRegressionProblem::create(schema,TypedShape::vector(2),training,limits());
    auto config=typed_example::config();if(smoke){config.population=8;config.elites=1;config.maxEvaluations=24;config.maxTrainingLossAttempts=24;config.maxGenerationAttempts=2;}
    // Freeze complete protocol and data files before any search outcomes exist.
    auto protocol=file(directory/"protocol.txt");
    protocol<<"target=A*x+b reference=independent_row_major_loops data_seeds=73001,73002,73003 rows=64,64,64\n"
        <<"schema=x0:matrix[2,3],x1:vector[3],x2:vector[2] result=vector[2] seeds=0,1,2,3,42\n"
        <<"population="<<config.population<<" loss_budget="<<config.maxTrainingLossAttempts<<" candidate_budget="<<config.maxEvaluations
        <<" generations="<<config.maxGenerationAttempts<<" nodes=15 depth=5 constant_nodes=4 coefficients=16 constant_abs=8\n"
        <<"operators=add,sub,mul,dot,matvec,matmul,transpose policy=uniform_feasible_productions operation_probability=.65\n"
        <<"constant_shapes=scalar constant_values=-1,0,1 initial_depth=4 mutation_depth=3 proposals=8 elites="<<config.elites
        <<" crossover=.9 mutation=.35 fitting=off stop_loss=0 recovery_max_error=1e-6\n"
        <<"ordering=loss,nodes,depth,constant_occurrences,coefficients,text final_tie=seed_list_order threads="<<execution.threads<<'\n';protocol.close();
    const std::array<const std::vector<TypedRegressionRow>*,3> datasets{&training,&interior,&exterior};
    for(std::size_t split=0;split<3;++split){auto out=file(directory/("data-"+std::to_string(split)+".csv"));out<<"a00,a01,a02,a10,a11,a12,x0,x1,x2,b0,b1,y0,y1\n";
        for(const auto& row:*datasets[split]){bool first=true;for(const auto& input:row.inputs)for(double v:input.coefficients()){if(!first)out<<',';first=false;out<<v;}for(double v:row.target.coefficients())out<<','<<v;out<<'\n';}}
    struct Run{unsigned seed;TypedSymbolicRegressionResult result;double seconds;};std::vector<Run> runs;
    for(unsigned seed:smoke?std::vector<unsigned>{42}:std::vector<unsigned>{0,1,2,3,42}){
        RandomContext random(seed,execution);const auto begin=std::chrono::steady_clock::now();auto result=typedSymbolicRegression(problem,config,random);
        const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();runs.push_back({seed,std::move(result),seconds});
        std::cout<<"seed="<<seed<<" complete\n";
    }
    std::optional<std::size_t> representative;
    auto frozen=file(directory/"frozen-models.txt");
    for(std::size_t i=0;i<runs.size();++i)if(runs[i].result.best){const auto& c=*runs[i].result.best;frozen<<runs[i].seed<<'\t'<<c.fitness()<<'\t'<<c.text()<<'\n';
        if(!representative || TypedRegressionOrdering{}.better(c,*runs[*representative].result.best,ObjectiveDirection::Minimize))representative=i;}
    frozen<<"representative_seed="<<(representative?std::to_string(runs[*representative].seed):"none")<<'\n';frozen.close();
    // All model and representative decisions are final before evaluation scoring begins.
    auto out=file(directory/"results.txt");out<<"representative_seed="<<(representative?std::to_string(runs[*representative].seed):"none")<<'\n';
    for(const auto& run:runs){const auto& r=run.result;
        out<<"seed="<<run.seed<<" initial="<<(r.initialBestMSE?*r.initialBestMSE:-1)<<" final="<<(r.best?r.best->fitness():-1)
            <<" best_generation="<<(r.bestGeneration?std::to_string(*r.bestGeneration):"none")<<" published="<<r.publishedGenerations
            <<" attempted_generations="<<r.attemptedGenerations<<" loss_attempts="<<r.trainingWork.lossAttempts<<" rows="<<r.trainingWork.rowAttempts
            <<" primitive_work="<<r.arithmeticOperations<<" unused="<<config.maxTrainingLossAttempts-r.trainingWork.lossAttempts
            <<" runtime="<<run.seconds<<" stop="<<searchStopName(r.stoppingReason)<<'\n';
        out<<"mutation_changes="<<r.variation.mutationChanges<<" crossover_changes="<<r.variation.crossoverChanges<<" structural_rejections="<<r.variation.structuralRejections<<" mutation_fallbacks="<<r.variation.mutationFallbacks<<" crossover_fallbacks="<<r.variation.crossoverFallbacks<<'\n';
        if(r.best){const auto& e=r.best->expression();out<<"expression="<<e.toString()<<'\n'<<e.schemaString()<<" output="<<e.shape().toString()<<" nodes="<<e.nodeCount()<<" depth="<<e.depth()<<" constants="<<e.constantCount()<<" coefficients="<<e.constantCoefficients()<<'\n';
            for(std::size_t split=0;split<3;++split){auto assessment=TypedRegressionProblem::create(schema,TypedShape::vector(2),*datasets[split],limits());const auto m=assessment->measure(e,nullptr,nullptr,true);
                out<<"range="<<split<<" loss="<<(m.loss.state()==FitnessState::Valid?std::to_string(m.loss.score()):"invalid")<<" max_error="<<(m.maxAbsoluteError?*m.maxAbsoluteError:-1)<<" invalid_rows="<<m.invalidRows<<" rows="<<m.rowsAttempted<<'\n';}}
        for(const auto& point:r.improvements)out<<"trace="<<point.lossAttempts<<','<<point.generation<<','<<point.mse<<'\n';
    }
    // Three simple baselines, trained/fixed before their evaluation-only assessment.
    std::array<double,2> mean{};for(const auto& row:training)for(std::size_t i=0;i<2;++i)mean[i]+=row.target.coefficients()[i]/64.;
    const auto average=TypedExpression::constant(TypedValue::vector(mean),schema,limits());
    const auto b=TypedExpression::variable(2,schema,limits());
    const auto product=TypedExpression::binary(K::MatVec,TypedExpression::variable(0,schema,limits()),TypedExpression::variable(1,schema,limits()),limits());
    for(const auto& [name,e]:std::array<std::pair<const char*,TypedExpression>,3>{{{"mean",average},{"b",b},{"Ax",product}}})
        for(std::size_t split=0;split<3;++split){auto p=TypedRegressionProblem::create(schema,TypedShape::vector(2),*datasets[split],limits());const auto m=p->measure(e);out<<"baseline="<<name<<" range="<<split<<" mse="<<m.loss.score()<<" max_error="<<*m.maxAbsoluteError<<'\n';}
    std::cout<<"results="<<(directory/"results.txt")<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
