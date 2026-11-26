#include <ComplexSymbolicRegression.hpp>
#include <ExpressionParsing.hpp>

#include <array>
#include <cmath>
#include <complex>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
using C = std::complex<double>;
struct Task {
    const char* name;
    C amplitude;
    C frequency;
};
const char* fitStopName(LMStop stop) {
    switch (stop) {
    case LMStop::GradientTolerance: return "gradient_tolerance";
    case LMStop::StepTolerance: return "step_tolerance";
    case LMStop::LossTolerance: return "loss_tolerance";
    case LMStop::IterationLimit: return "iteration_limit";
    case LMStop::EvaluationLimit: return "evaluation_limit";
    case LMStop::DerivativeLimit: return "derivative_limit";
    case LMStop::WorkspaceLimit: return "workspace_limit";
    case LMStop::SingularSystem: return "singular_system";
    case LMStop::InvalidStart: return "invalid_start";
    case LMStop::DampingLimit: return "damping_limit";
    case LMStop::Cancelled: return "cancelled";
    }
    return "unknown";
}
const std::array<C, 12> trainInputs{{C{-.95, .15}, C{-.8, -.1}, C{-.55, .2}, C{-.3, -.15},
                                     C{-.1, .1}, C{.2, -.2}, C{.4, .12}, C{.65, -.08},
                                     C{.82, .18}, C{1.0, -.1}, C{1.15, .07}, C{-1.1, -.18}}};
const std::array<C, 6> heldoutInputs{{C{-1.2, .13}, C{-.72, -.17}, C{.08, .19},
                                      C{.48, -.11}, C{.93, .14}, C{1.25, -.16}}};
C target(const Task& task, C z) {
    if (std::string(task.name) == "affine")
        return task.amplitude * z + task.frequency;
    return task.amplitude * std::cos(task.frequency * z);
}
std::vector<ComplexObservation> rows(const Task& task, bool heldout, bool synthetic = false) {
    std::vector<ComplexObservation> result;
    if (heldout) {
        for (const auto z : heldoutInputs)
            result.push_back({{z}, target(task, z)});
    } else {
        for (const auto z : trainInputs)
            result.push_back({{z}, synthetic ? C{.2, .1} : target(task, z)});
    }
    return result;
}
double heldoutRmse(const ComplexExpression& expression, const std::vector<ComplexObservation>& data) {
    double loss = 0;
    for (const auto& row : data) {
        const auto value = expression.evaluate(row.inputs);
        if (!value.success())
            return std::numeric_limits<double>::infinity();
        loss += std::norm(*value.value - row.target) / data.size();
    }
    return std::sqrt(loss);
}
double heldoutRmse(const Expression& expression, const ComplexRegressionProblem& problem,
                   const std::vector<ComplexObservation>& data) {
    const auto complex = ComplexExpression::fromScalarGenome(expression);
    (void)problem;
    return heldoutRmse(complex, data);
}
std::string coefficients(const std::optional<ComplexExpression>& expression) {
    if (!expression) return "[]";
    std::ostringstream output;output<<std::setprecision(17)<<'[';bool first=true;
    for(const auto& node:expression->nodes())if(node.kind==ComplexKind::Constant){if(!first)output<<',';first=false;output<<'['<<node.value.real()<<','<<node.value.imag()<<']';}
    output<<']';return output.str();
}
struct Tree {
    std::vector<std::array<std::size_t, 3>> children;
};
Tree indexTree(const Expression& expression) {
    Tree tree; const auto nodes = expression.nodes(); tree.children.resize(nodes.size());
    std::vector<std::size_t> roots;
    for (std::size_t i=0;i<nodes.size();++i) {
        for (std::size_t child=expressionArity(nodes[i].kind());child>0;--child) {
            tree.children[i][child-1]=roots.back();roots.pop_back();
        }
        roots.push_back(i);
    }
    return tree;
}
bool constant(const ExpressionNode& n) { return n.kind()==ExpressionKind::Constant; }
bool variable(const ExpressionNode& n) { return n.kind()==ExpressionKind::Variable&&n.variableIndex()==0; }
bool affineTopology(const Expression& e) {
    const auto n=e.nodes();if(n.back().kind()!=ExpressionKind::Add)return false;const auto t=indexTree(e);const auto a=t.children.back()[0],b=t.children.back()[1];
    const auto multiplication=[&](std::size_t i){if(n[i].kind()!=ExpressionKind::Multiply)return false;const auto l=t.children[i][0],r=t.children[i][1];return (variable(n[l])&&constant(n[r]))||(constant(n[l])&&variable(n[r]));};
    return (constant(n[a])&&multiplication(b))||(constant(n[b])&&multiplication(a));
}
bool cosineTopology(const Expression& e) {
    const auto n=e.nodes();if(n.back().kind()!=ExpressionKind::Multiply)return false;const auto t=indexTree(e);const auto a=t.children.back()[0],b=t.children.back()[1];
    const auto shape=[&](std::size_t coefficient,std::size_t cosine){if(!constant(n[coefficient])||n[cosine].kind()!=ExpressionKind::Cos)return false;const auto argument=t.children[cosine][0];if(n[argument].kind()!=ExpressionKind::Multiply)return false;const auto l=t.children[argument][0],r=t.children[argument][1];return (variable(n[l])&&constant(n[r]))||(constant(n[l])&&variable(n[r]));};
    return shape(a,b)||shape(b,a);
}
Task caseTask(std::string_view name) {
    if(name=="affine")return {"affine",C{1.27,.48},C{-.31,.27}};
    return {"nonlinear",C{.73,-.28},C{1.13,.32}};
}
void printNested(const std::exception& error,unsigned depth=0) {
    std::cerr<<std::string(depth*2,' ')<<error.what()<<'\n';
    try { std::rethrow_if_nested(error); }
    catch(const std::exception& nested){if(depth<8)printNested(nested,depth+1);}
    catch(...) {}
}
void run(const Task& task,unsigned seed,bool synthetic) {
    const auto training=rows(task,false,synthetic);const auto heldout=rows(task,true);
    auto problem=std::make_shared<const ComplexRegressionProblem>(training,ExpressionLimits{15,6,1.5,24});
    SymbolicRegressionConfig config;config.population=32;config.elites=2;config.maxGenerationAttempts=40;
    config.maxEvaluations=1312;config.maxTrainingLossAttempts=1312*training.size();
    config.grammar.operators={ExpressionKind::Add,ExpressionKind::Subtract,ExpressionKind::Multiply,ExpressionKind::Cos};
    config.grammar.constants={-1,0,1};config.grammar.initialMaxDepth=3;config.grammar.mutationMaxDepth=2;
    config.selectionStrategy=SymbolicRegressionConfig::SelectionStrategy::Pareto;
    config.maximumParetoFrontier=16;config.maximumParetoComparisons=1'000'000;
    config.maximumParetoEstimatedMemoryBytes=32*1024*1024;
    RandomContext baselineRandom(seed);const auto baseline=symbolicRegression(problem,config,baselineRandom);
    RandomContext candidateRandom(seed);ComplexCoefficientFitConfig fit;fit.lower=-2;fit.upper=2;
    fit.optimizer.maxObjectiveEvaluations=256;fit.optimizer.maxIterations=64;fit.optimizer.maxParameters=16;
    fit.optimizer.maxResiduals=2*training.size();
    const auto autonomous=complexSymbolicRegression(problem,config,candidateRandom,fit);
    bool same=baseline.best&&autonomous.structureSearch.best&&baseline.best->text()==autonomous.structureSearch.best->text();
    const auto baseHeld=baseline.best?heldoutRmse(baseline.best->expression(),*problem,heldout):-1;
    const auto fittedHeld=autonomous.fittedExpression?heldoutRmse(*autonomous.fittedExpression,heldout):-1;
    Expression initialTopology=parseExpression(task.name==std::string("affine")?"1.0*x0+1.0":"1.0*cos(1.0*x0)",1,{},ExpressionLimits{15,6,1.5,24});
    auto diagnosticInitial=ComplexExpression::fromScalarGenome(initialTopology);
    auto diagnostic=fitComplexCoefficients(*problem,diagnosticInitial,fit);
    const auto diagnosticHeld=heldoutRmse(diagnostic.expression,heldout);
    const auto& expression=baseline.best?baseline.best->expression():initialTopology;
    const bool baseRecovered=std::string(task.name)=="affine"?affineTopology(expression):cosineTopology(expression);
    const bool candidateRecovered=autonomous.selectedTopology&&
        (std::string(task.name)=="affine"?affineTopology(*autonomous.selectedTopology):cosineTopology(*autonomous.selectedTopology));
    std::cout<<std::setprecision(17)<<"{\"case\":\""<<task.name<<"\",\"seed\":"<<seed
             <<",\"synthetic\":"<<(synthetic?"true":"false")
             <<",\"baseline_expression\":\""<<(baseline.best?baseline.best->text():"")
             <<"\",\"baseline_structure_recovery\":"<<(baseRecovered?"true":"false")
             <<",\"baseline_heldout_rmse\":"<<baseHeld
             <<",\"baseline_candidate_evaluations\":"<<baseline.candidateEvaluations
             <<",\"baseline_training_rows\":"<<baseline.trainingWork.rowAttempts
             <<",\"candidate_search_expression_matches_baseline\":"<<(same?"true":"false")
             <<",\"candidate_topology\":\""<<(autonomous.selectedTopology?autonomous.selectedTopology->toString():"")
             <<"\",\"candidate_structure_recovery\":"<<(candidateRecovered?"true":"false")
             <<",\"complex_fit_stop\":\""<<(autonomous.coefficientFit?fitStopName(autonomous.coefficientFit->stop):"no_coefficients")
             <<"\",\"complex_fit_objective_evaluations\":"<<(autonomous.coefficientFit?autonomous.coefficientFit->objectiveEvaluations:0)
             <<",\"complex_fit_topologies\":"<<autonomous.fittedTopologies
             <<",\"complex_fit_total_objective_evaluations\":"<<autonomous.coefficientFitObjectiveEvaluations
             <<",\"complex_fit_training_loss\":"<<(autonomous.coefficientFit&&autonomous.coefficientFit->trainingLoss.state()==FitnessState::Valid?autonomous.coefficientFit->trainingLoss.score():-1)
             <<",\"candidate_heldout_rmse\":"<<fittedHeld
             <<",\"candidate_complex_coefficients\":"<<coefficients(autonomous.fittedExpression)
             <<",\"diagnostic_supplied_topology\":\""<<(std::string(task.name)=="affine"?"1.0*x0+1.0":"1.0*cos(1.0*x0)")
             <<"\",\"diagnostic_training_loss\":"<<(diagnostic.trainingLoss.state()==FitnessState::Valid?diagnostic.trainingLoss.score():-1)
             <<",\"diagnostic_heldout_rmse\":"<<diagnosticHeld
             <<",\"diagnostic_complex_coefficients\":"<<coefficients(std::optional<ComplexExpression>{diagnostic.expression})<<"}\n";
}
}
int main(int argc,char**argv){
    if(argc!=4){std::cerr<<"usage: complex_autonomous_comparison affine|nonlinear seed synthetic|target\n";return 2;}
    try { run(caseTask(argv[1]),static_cast<unsigned>(std::stoul(argv[2])),std::string(argv[3])=="synthetic");return 0; }
    catch(const std::exception& error){printNested(error);return 1;}
}
