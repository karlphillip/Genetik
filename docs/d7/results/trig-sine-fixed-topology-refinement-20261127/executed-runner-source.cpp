// Bounded fixed-topology D7 coefficient-refinement comparison. Training rows
// only are read here; held-out scoring is a separate post-freeze process.
#include <ConstantFitting.hpp>
#include <ExpressionParsing.hpp>
#include <charconv>
#include <algorithm>
#include <iomanip>
#include <tuple>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
double number(std::string_view text) {
    double value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || !std::isfinite(value))
        throw std::runtime_error("invalid training number");
    return value;
}
std::vector<RegressionRow> rows(const char* path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open training CSV");
    std::string line;
    if (!std::getline(input, line) || line != "x0,y") throw std::runtime_error("unexpected training header");
    std::vector<RegressionRow> data;
    while (std::getline(input, line)) {
        std::istringstream fields(line);
        std::string x, y, extra;
        if (!std::getline(fields, x, ',') || !std::getline(fields, y, ',') || std::getline(fields, extra, ','))
            throw std::runtime_error("malformed training row");
        data.push_back({{number(x)}, number(y)});
    }
    if (data.empty()) throw std::runtime_error("empty training CSV");
    return data;
}

struct RankSelection {
    std::vector<std::size_t> nodes;
    std::size_t derivative_rows = 0;
    std::size_t primal_ops = 0;
    std::size_t derivative_ops = 0;
    double tolerance = 0;
};

const char* nativeStopName(LMStop stop) {
    switch (stop) {
    case LMStop::GradientTolerance: return "gradient_tolerance";
    case LMStop::StepTolerance: return "step_tolerance";
    case LMStop::LossTolerance: return "loss_tolerance";
    case LMStop::IterationLimit: return "iteration_limit";
    case LMStop::EvaluationLimit: return "evaluation_limit";
    case LMStop::DerivativeLimit: return "derivative_limit";
    case LMStop::WorkspaceLimit: return "workspace_limit";
    case LMStop::DampingLimit: return "damping_limit";
    case LMStop::Cancelled: return "cancelled";
    case LMStop::InvalidStart: return "invalid_start";
    }
    return "unknown";
}

// Pivoted modified Gram-Schmidt, with initial postfix order as the tie break.
// Columns are scaled only by the common maximum-column norm, so the rank
// decision and ranking are invariant to a common rescaling of the Jacobian.
RankSelection selectIndependent(const Expression& expression,
                                const std::vector<RegressionRow>& training) {
    const auto& nodes = expression.nodes();
    std::vector<std::size_t> constantNodes;
    for (std::size_t i = 0; i < nodes.size(); ++i)
        if (nodes[i].kind() == ExpressionKind::Constant) constantNodes.push_back(i);
    if (constantNodes.empty()) return {};

    std::vector<std::vector<double>> columns(constantNodes.size());
    RankSelection result;
    for (auto& column : columns) column.reserve(training.size());
    const double hScale = std::sqrt(std::numeric_limits<double>::epsilon());
    for (const auto& row : training) {
        for (std::size_t j = 0; j < constantNodes.size(); ++j) {
            const auto node = constantNodes[j];
            const double center = nodes[node].constantValue();
            const double h = hScale * std::max(1.0, std::abs(center));
            const auto plus = expression.withConstant(node, center + h).evaluate(row.inputs);
            const auto minus = expression.withConstant(node, center - h).evaluate(row.inputs);
            if (!plus.valid() || !minus.valid())
                throw std::runtime_error("initial finite-difference training Jacobian unavailable");
            columns[j].push_back((plus.value() - minus.value()) / (2.0 * h));
            result.primal_ops += plus.arithmeticOperations() + minus.arithmeticOperations();
        }
        ++result.derivative_rows;
    }
    auto norm = [](const std::vector<double>& v) {
        double scale = 0, sum = 1;
        for (double x : v) {
            const double ax = std::abs(x);
            if (ax == 0) continue;
            if (scale < ax) { const double ratio = scale / ax; sum = 1 + sum * ratio * ratio; scale = ax; }
            else { const double ratio = ax / scale; sum += ratio * ratio; }
        }
        return scale == 0 ? 0 : scale * std::sqrt(sum);
    };
    double largestOriginal = 0;
    for (const auto& column : columns) largestOriginal = std::max(largestOriginal, norm(column));
    result.tolerance = 1e-8 * largestOriginal;
    std::vector<bool> chosen(columns.size(), false);
    std::vector<std::vector<double>> basis;
    for (;;) {
        std::size_t pivot = columns.size();
        double pivotNorm = -1;
        std::vector<double> pivotResidual;
        for (std::size_t j = 0; j < columns.size(); ++j) {
            if (chosen[j]) continue;
            auto residual = columns[j];
            for (const auto& q : basis) {
                double dot = 0;
                for (std::size_t r = 0; r < q.size(); ++r) dot += q[r] * residual[r];
                for (std::size_t r = 0; r < q.size(); ++r) residual[r] -= dot * q[r];
            }
            const auto residualNorm = norm(residual);
            if (residualNorm > pivotNorm) { pivot = j; pivotNorm = residualNorm; pivotResidual = std::move(residual); }
        }
        if (pivot == columns.size() || pivotNorm <= result.tolerance) break;
        chosen[pivot] = true;
        for (double& value : pivotResidual) value /= pivotNorm;
        basis.push_back(std::move(pivotResidual));
        result.nodes.push_back(constantNodes[pivot]);
    }
    std::sort(result.nodes.begin(), result.nodes.end());
    return result;
}

std::vector<std::string> expressions(const char* path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open frozen expression file");
    std::vector<std::string> result;
    for (std::string line; std::getline(input, line);) if (!line.empty()) result.push_back(line);
    if (result.size() != 2) throw std::runtime_error("expected exactly two frozen expressions");
    return result;
}

void emit(std::ostream& out, std::uint64_t seed, const char* method,
          const ConstantFitResult& fit, std::size_t parameters,
          const std::vector<std::size_t>& selectedNodes, double rankTolerance,
          std::size_t selectionRows, std::size_t selectionOps,
          double selectionSeconds, double cpuSeconds, const TrainingWork& work) {
    out << seed << '\t' << method << '\t' << parameters << '\t'
        << std::setprecision(17) << fit.startingLoss.score() << '\t' << fit.loss.score() << '\t'
        << constantFitStopName(fit.stop) << '\t'
        << (fit.optimizerStop ? nativeStopName(*fit.optimizerStop) : "n/a") << '\t'
        << fit.lossAttempts << '\t' << fit.proposals << '\t' << fit.sweeps << '\t'
        << fit.optimizerWork.objectiveEvaluations << '\t'
        << fit.optimizerWork.jacobianEvaluations << '\t'
        << fit.optimizerWork.forwardEvaluations << '\t'
        << fit.optimizerWork.derivativeEvaluations << '\t'
        << fit.optimizerWork.mathematicalOperations << '\t'
        << fit.optimizerWork.linearAlgebraOperations << '\t'
        << selectionRows << '\t' << selectionOps << '\t'
        << std::setprecision(17) << rankTolerance << '\t';
    for (std::size_t i = 0; i < selectedNodes.size(); ++i) {
        if (i) out << ',';
        out << selectedNodes[i];
    }
    out << '\t'
        << std::setprecision(9) << selectionSeconds << '\t' << cpuSeconds << '\t'
        << fit.expression.toString() << '\n';
}
}

int main(int argc, char** argv) {
    try {
        if (argc != 4) throw std::runtime_error("usage: compare TRAIN.csv FROZEN_EXPRESSIONS.txt OUTPUT.tsv");
        const auto limits = ExpressionLimits{25, 8, 8.0};
        const auto training = rows(argv[1]);
        const auto sourceExpressions = expressions(argv[2]);
        std::ofstream output(argv[3], std::ios::trunc);
        if (!output) throw std::runtime_error("cannot open output file");
        output << "seed\tmethod\tselected_parameters\tstarting_training_mse\tfinal_training_mse\tstop\tnative_stop\tobjective_calls\tproposals\tsweeps\tlm_objective_evaluations\tlm_jacobian_evaluations\tlm_forward_evaluations\tlm_derivative_evaluations\tlm_math_ops\tlm_linear_algebra_ops\tselection_derivative_rows\tselection_arithmetic_ops\trank_tolerance\tselected_constant_nodes\tselection_cpu_seconds\tfit_cpu_seconds\texpression\n";
        for (std::size_t i = 0; i < sourceExpressions.size(); ++i) {
            const auto seed = i;
            auto expression = parseExpression(sourceExpressions[i], 1, {}, limits);
            auto rankStart = std::clock();
            const auto rank = selectIndependent(expression, training);
            const double rankCpu = static_cast<double>(std::clock() - rankStart) / CLOCKS_PER_SEC;
            for (const auto [name, step, method, subset] : {
                     std::tuple{"coordinate_step_1", 1.0, ConstantFitMethod::Coordinate, false},
                     std::tuple{"coordinate_step_1e-4", 1e-4, ConstantFitMethod::Coordinate, false},
                     std::tuple{"lm_all_occurrences", 1.0, ConstantFitMethod::LevenbergMarquardt, false},
                     std::tuple{"lm_rank_subset", 1.0, ConstantFitMethod::LevenbergMarquardt, true}}) {
                auto problem = RegressionProblem::create(training, limits);
                TrainingObjective objective(problem, 64);
                ConstantFitConfig config;
                config.lower = -8; config.upper = 8; config.initialStep = step;
                config.minimumStep = 1e-6; config.maxLossAttempts = 64;
                config.maxSweeps = 32; config.maxProposals = 2048;
                config.method = method;
                config.levenbergMarquardt.maxObjectiveEvaluations = 64;
                config.levenbergMarquardt.maxJacobianEvaluations = 64;
                const auto fitStart = std::clock();
                ConstantFitResult fit = subset
                    ? fitSelectedConstants(expression, objective, rank.nodes, config)
                    : fitConstants(expression, objective, config);
                const double fitCpu = static_cast<double>(std::clock() - fitStart) / CLOCKS_PER_SEC;
                const auto expressionNodes = expression.nodes();
                const auto selected = subset ? rank.nodes.size() : static_cast<std::size_t>(std::count_if(
                    expressionNodes.begin(), expressionNodes.end(), [](const auto& node) { return node.kind() == ExpressionKind::Constant; }));
                const auto selectionRows = subset ? rank.derivative_rows : 0;
                const auto selectionOps = subset ? rank.primal_ops + rank.derivative_ops : 0;
                std::vector<std::size_t> selectedNodes;
                if (subset) selectedNodes = rank.nodes;
                else for (std::size_t node = 0; node < expressionNodes.size(); ++node)
                    if (expressionNodes[node].kind() == ExpressionKind::Constant) selectedNodes.push_back(node);
                emit(output, seed, name, fit, selected, selectedNodes,
                     subset ? rank.tolerance : 0.0, selectionRows, selectionOps,
                     subset ? rankCpu : 0.0, fitCpu, objective.work());
            }
        }
        output.flush();
        if (!output) throw std::runtime_error("failed writing frozen fit output");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "refinement_error=" << error.what() << '\n';
        return 2;
    }
}
