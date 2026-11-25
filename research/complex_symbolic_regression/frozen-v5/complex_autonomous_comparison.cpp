#include <ComplexSymbolicRegression.hpp>
#include <ExpressionParsing.hpp>

#include <algorithm>
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
    case LMStop::GradientTolerance:
        return "gradient_tolerance";
    case LMStop::StepTolerance:
        return "step_tolerance";
    case LMStop::LossTolerance:
        return "loss_tolerance";
    case LMStop::IterationLimit:
        return "iteration_limit";
    case LMStop::EvaluationLimit:
        return "evaluation_limit";
    case LMStop::DerivativeLimit:
        return "derivative_limit";
    case LMStop::WorkspaceLimit:
        return "workspace_limit";
    case LMStop::SingularSystem:
        return "singular_system";
    case LMStop::InvalidStart:
        return "invalid_start";
    case LMStop::DampingLimit:
        return "damping_limit";
    case LMStop::Cancelled:
        return "cancelled";
    }
    return "unknown";
}
const std::array<C, 12> trainInputs{{C{-.95, .15}, C{-.8, -.1}, C{-.55, .2}, C{-.3, -.15},
                                     C{-.1, .1}, C{.2, -.2}, C{.4, .12}, C{.65, -.08}, C{.82, .18},
                                     C{1.0, -.1}, C{1.15, .07}, C{-1.1, -.18}}};
const std::array<C, 6> heldoutInputs{
    {C{-1.2, .13}, C{-.72, -.17}, C{.08, .19}, C{.48, -.11}, C{.93, .14}, C{1.25, -.16}}};
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
double heldoutRmse(const ComplexExpression& expression,
                   const std::vector<ComplexObservation>& data) {
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
    if (!expression)
        return "[]";
    std::ostringstream output;
    output << std::setprecision(17) << '[';
    bool first = true;
    for (const auto& node : expression->nodes())
        if (node.kind == ComplexKind::Constant) {
            if (!first)
                output << ',';
            first = false;
            output << '[' << node.value.real() << ',' << node.value.imag() << ']';
        }
    output << ']';
    return output.str();
}
struct Tree {
    std::vector<std::array<std::size_t, 3>> children;
};
Tree indexTree(const Expression& expression) {
    Tree tree;
    const auto nodes = expression.nodes();
    tree.children.resize(nodes.size());
    std::vector<std::size_t> roots;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        for (std::size_t child = expressionArity(nodes[i].kind()); child > 0; --child) {
            tree.children[i][child - 1] = roots.back();
            roots.pop_back();
        }
        roots.push_back(i);
    }
    return tree;
}
bool constant(const ExpressionNode& n) {
    return n.kind() == ExpressionKind::Constant;
}
bool variable(const ExpressionNode& n) {
    return n.kind() == ExpressionKind::Variable && n.variableIndex() == 0;
}
struct LinearForm {
    bool valid = true, depends = false;
    unsigned degree = 0;
};
bool containsVariable(const Expression& e, const Tree& tree, std::size_t root) {
    const auto nodes = e.nodes();
    if (variable(nodes[root]))
        return true;
    for (std::size_t c = 0; c < expressionArity(nodes[root].kind()); ++c)
        if (containsVariable(e, tree, tree.children[root][c]))
            return true;
    return false;
}
bool containsConstant(const Expression& e, const Tree& tree, std::size_t root) {
    const auto nodes = e.nodes();
    if (constant(nodes[root]))
        return true;
    for (std::size_t c = 0; c < expressionArity(nodes[root].kind()); ++c)
        if (containsConstant(e, tree, tree.children[root][c]))
            return true;
    return false;
}
std::size_t constantCount(const Expression& e) {
    const auto nodes = e.nodes();
    return static_cast<std::size_t>(std::count_if(nodes.begin(), nodes.end(), constant));
}
bool hasVariableScale(const Expression& e, const Tree& tree, std::size_t root) {
    const auto nodes = e.nodes();
    if (nodes[root].kind() == ExpressionKind::Multiply) {
        const auto left = tree.children[root][0], right = tree.children[root][1];
        if ((containsVariable(e, tree, left) && !containsVariable(e, tree, right) &&
             containsConstant(e, tree, right)) ||
            (containsVariable(e, tree, right) && !containsVariable(e, tree, left) &&
             containsConstant(e, tree, left)))
            return true;
    }
    for (std::size_t c = 0; c < expressionArity(nodes[root].kind()); ++c)
        if (hasVariableScale(e, tree, tree.children[root][c]))
            return true;
    return false;
}
bool hasOffsetTerm(const Expression& e, const Tree& tree, std::size_t root) {
    const auto nodes = e.nodes();
    if (nodes[root].kind() == ExpressionKind::Add ||
        nodes[root].kind() == ExpressionKind::Subtract) {
        const auto left = tree.children[root][0], right = tree.children[root][1];
        if ((containsVariable(e, tree, left) && !containsVariable(e, tree, right) &&
             containsConstant(e, tree, right)) ||
            (containsVariable(e, tree, right) && !containsVariable(e, tree, left) &&
             containsConstant(e, tree, left)))
            return true;
    }
    for (std::size_t c = 0; c < expressionArity(nodes[root].kind()); ++c)
        if (hasOffsetTerm(e, tree, tree.children[root][c]))
            return true;
    return false;
}
LinearForm linearForm(const Expression& e, const Tree& tree, std::size_t root) {
    const auto nodes = e.nodes();
    const auto& node = nodes[root];
    if (constant(node))
        return {};
    if (node.kind() == ExpressionKind::Variable)
        return {true, true, 1};
    const auto arity = expressionArity(node.kind());
    if (!containsVariable(e, tree, root))
        return {};
    if ((node.kind() != ExpressionKind::Add && node.kind() != ExpressionKind::Subtract &&
         node.kind() != ExpressionKind::Multiply) ||
        arity != 2)
        return {false, true, 0};
    const auto left = linearForm(e, tree, tree.children[root][0]);
    const auto right = linearForm(e, tree, tree.children[root][1]);
    if (!left.valid || !right.valid)
        return {false, true, 0};
    if (node.kind() == ExpressionKind::Multiply && left.depends && right.depends)
        return {false, true, 0};
    const auto degree = node.kind() == ExpressionKind::Multiply
                            ? left.degree + right.degree
                            : std::max(left.degree, right.degree);
    return {degree <= 1, left.depends || right.depends, degree};
}
Expression subtreeExpression(const Expression& e, std::size_t root) {
    const auto range = expressionSubtree(e, root);
    const auto nodes = e.nodes();
    return Expression(std::span(nodes.data() + range.first, range.second - range.first),
                      e.variableCount(), e.limits());
}
bool affineTopology(const Expression& e) {
    const auto tree = indexTree(e);
    const auto form = linearForm(e, tree, e.nodeCount() - 1);
    // a*z+b requires independently adjustable scale and offset. A one-
    // parameter expression such as z-c is affine but cannot express this task.
    return form.valid && form.depends && form.degree == 1 && constantCount(e) >= 2 &&
           hasVariableScale(e, tree, e.nodeCount() - 1) &&
           hasOffsetTerm(e, tree, e.nodeCount() - 1);
}
bool cosineTopology(const Expression& e) {
    const auto tree = indexTree(e);
    const auto nodes = e.nodes();
    const auto root = e.nodeCount() - 1;
    if (nodes[root].kind() != ExpressionKind::Multiply)
        return false;
    const auto left = tree.children[root][0], right = tree.children[root][1];
    for (const auto [cosine, amplitude] : {std::pair{left, right}, std::pair{right, left}}) {
        if (nodes[cosine].kind() != ExpressionKind::Cos || containsVariable(e, tree, amplitude) ||
            !containsConstant(e, tree, amplitude))
            continue;
        const auto argument = tree.children[cosine][0];
        const auto form = linearForm(e, tree, argument);
        if (!form.valid || !form.depends || form.degree != 1 ||
            !hasVariableScale(e, tree, argument))
            continue;
        const auto expression = subtreeExpression(e, argument);
        const std::array<double, 1> zero{0.0}, one{1.0};
        const auto atZero = expression.evaluate(zero), atOne = expression.evaluate(one);
        if (atZero.valid() && atOne.valid() && std::abs(atZero.value()) < 1e-12 &&
            std::abs(atOne.value()) > 1e-12)
            return true;
    }
    return false;
}
Task caseTask(std::string_view name) {
    if (name == "affine")
        return {"affine", C{1.27, .48}, C{-.31, .27}};
    return {"nonlinear", C{.73, -.28}, C{1.13, .32}};
}
void printNested(const std::exception& error, unsigned depth = 0) {
    std::cerr << std::string(depth * 2, ' ') << error.what() << '\n';
    try {
        std::rethrow_if_nested(error);
    } catch (const std::exception& nested) {
        if (depth < 8)
            printNested(nested, depth + 1);
    } catch (...) {}
}
void run(const Task& task, unsigned seed, bool synthetic) {
    const auto training = rows(task, false, synthetic);
    const auto heldout = rows(task, true);
    auto problem = std::make_shared<const ComplexRegressionProblem>(
        training, ExpressionLimits{15, 6, 1.5, 24});
    SymbolicRegressionConfig config;
    config.population = 32;
    config.elites = 2;
    config.maxGenerationAttempts = 40;
    config.maxEvaluations = 1312;
    config.maxTrainingLossAttempts = 1312 * training.size();
    config.grammar.operators = {ExpressionKind::Add, ExpressionKind::Subtract,
                                ExpressionKind::Multiply, ExpressionKind::Cos};
    config.grammar.constants = {-1, 0, 1};
    config.grammar.initialMaxDepth = 3;
    config.grammar.mutationMaxDepth = 2;
    config.selectionStrategy = SymbolicRegressionConfig::SelectionStrategy::Pareto;
    config.maximumParetoFrontier = 16;
    config.maximumParetoComparisons = 1'000'000;
    config.maximumParetoEstimatedMemoryBytes = 32 * 1024 * 1024;
    RandomContext baselineRandom(seed);
    const auto baseline = symbolicRegression(problem, config, baselineRandom);
    RandomContext candidateRandom(seed);
    ComplexCoefficientFitConfig fit;
    fit.lower = -2;
    fit.upper = 2;
    fit.optimizer.maxObjectiveEvaluations = 256;
    fit.optimizer.maxIterations = 64;
    fit.optimizer.maxParameters = 16;
    fit.optimizer.maxResiduals = 2 * training.size();
    const auto autonomous = complexSymbolicRegression(problem, config, candidateRandom, fit);
    bool same = baseline.best && autonomous.structureSearch.best &&
                baseline.best->text() == autonomous.structureSearch.best->text();
    const auto baseHeld =
        baseline.best ? heldoutRmse(baseline.best->expression(), *problem, heldout) : -1;
    const auto fittedHeld =
        autonomous.fittedExpression ? heldoutRmse(*autonomous.fittedExpression, heldout) : -1;
    Expression initialTopology =
        parseExpression(task.name == std::string("affine") ? "1.0*x0+1.0" : "1.0*cos(1.0*x0)", 1,
                        {}, ExpressionLimits{15, 6, 1.5, 24});
    auto diagnosticInitial = ComplexExpression::fromScalarGenome(initialTopology);
    auto diagnostic = fitComplexCoefficients(*problem, diagnosticInitial, fit);
    const auto diagnosticHeld = heldoutRmse(diagnostic.expression, heldout);
    const auto& expression = baseline.best ? baseline.best->expression() : initialTopology;
    const bool baseRecovered = std::string(task.name) == "affine" ? affineTopology(expression)
                                                                  : cosineTopology(expression);
    const bool candidateRecovered =
        autonomous.selectedTopology &&
        (std::string(task.name) == "affine" ? affineTopology(*autonomous.selectedTopology)
                                            : cosineTopology(*autonomous.selectedTopology));
    std::cout << std::setprecision(17) << "{\"case\":\"" << task.name << "\",\"seed\":" << seed
              << ",\"synthetic\":" << (synthetic ? "true" : "false")
              << ",\"baseline_expression\":\"" << (baseline.best ? baseline.best->text() : "")
              << "\",\"baseline_structure_recovery\":" << (baseRecovered ? "true" : "false")
              << ",\"baseline_heldout_rmse\":" << baseHeld
              << ",\"baseline_candidate_evaluations\":" << baseline.candidateEvaluations
              << ",\"baseline_training_rows\":" << baseline.trainingWork.rowAttempts
              << ",\"candidate_search_expression_matches_baseline\":" << (same ? "true" : "false")
              << ",\"candidate_topology\":\""
              << (autonomous.selectedTopology ? autonomous.selectedTopology->toString() : "")
              << "\",\"candidate_structure_recovery\":" << (candidateRecovered ? "true" : "false")
              << ",\"complex_fit_stop\":\""
              << (autonomous.coefficientFit ? fitStopName(autonomous.coefficientFit->stop)
                                            : "no_coefficients")
              << "\",\"complex_fit_objective_evaluations\":"
              << (autonomous.coefficientFit ? autonomous.coefficientFit->objectiveEvaluations : 0)
              << ",\"complex_fit_topologies\":" << autonomous.fittedTopologies
              << ",\"complex_fit_total_objective_evaluations\":"
              << autonomous.coefficientFitObjectiveEvaluations << ",\"complex_fit_training_loss\":"
              << (autonomous.coefficientFit &&
                          autonomous.coefficientFit->trainingLoss.state() == FitnessState::Valid
                      ? autonomous.coefficientFit->trainingLoss.score()
                      : -1)
              << ",\"candidate_heldout_rmse\":" << fittedHeld
              << ",\"candidate_complex_coefficients\":" << coefficients(autonomous.fittedExpression)
              << ",\"diagnostic_supplied_topology\":\""
              << (std::string(task.name) == "affine" ? "1.0*x0+1.0" : "1.0*cos(1.0*x0)")
              << "\",\"diagnostic_training_loss\":"
              << (diagnostic.trainingLoss.state() == FitnessState::Valid
                      ? diagnostic.trainingLoss.score()
                      : -1)
              << ",\"diagnostic_heldout_rmse\":" << diagnosticHeld
              << ",\"diagnostic_complex_coefficients\":"
              << coefficients(std::optional<ComplexExpression>{diagnostic.expression}) << "}\n";
}
} // namespace
int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr
            << "usage: complex_autonomous_comparison affine|nonlinear seed synthetic|target\n";
        return 2;
    }
    try {
        run(caseTask(argv[1]), static_cast<unsigned>(std::stoul(argv[2])),
            std::string(argv[3]) == "synthetic");
        return 0;
    } catch (const std::exception& error) {
        printNested(error);
        return 1;
    }
}
