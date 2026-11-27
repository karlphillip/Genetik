// Post-search D7 coefficient-refinement diagnostic. Training rows only.
#include <ConstantFitting.hpp>
#include <ExpressionParsing.hpp>
#include <charconv>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
double number(std::string_view text) {
    double value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
        throw std::runtime_error("invalid training number");
    return value;
}
std::vector<RegressionRow> rows(const char* path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open training CSV");
    std::string line;
    if (!std::getline(input, line) || line != "x0,y")
        throw std::runtime_error("unexpected training header");
    std::vector<RegressionRow> data;
    while (std::getline(input, line)) {
        std::istringstream fields(line);
        std::string x, y, extra;
        if (!std::getline(fields, x, ',') || !std::getline(fields, y, ',') || std::getline(fields, extra, ','))
            throw std::runtime_error("malformed training row");
        data.push_back({{number(x)}, number(y)});
    }
    return data;
}
}

int main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::runtime_error("usage: refine TRAIN.csv EXPRESSION");
        const auto limits = ExpressionLimits{25, 8, 8.0};
        auto problem = RegressionProblem::create(rows(argv[1]), limits);
        auto expression = parseExpression(argv[2], 1, {}, limits);
        TrainingObjective objective(problem, 64);
        ConstantFitConfig fit;
        fit.lower = -8.0;
        fit.upper = 8.0;
        fit.initialStep = 1.0;
        fit.minimumStep = 1e-6;
        fit.maxLossAttempts = 64;
        fit.maxSweeps = 32;
        fit.maxProposals = 2048;
        const auto cpuStart = std::clock();
        const auto result = fitConstants(expression, objective, fit);
        const double cpuSeconds = static_cast<double>(std::clock() - cpuStart) / CLOCKS_PER_SEC;
        std::cout << "initial_training_mse=" << result.startingLoss.score() << '\n'
                  << "final_training_mse=" << result.loss.score() << '\n'
                  << "expression=" << result.expression.toString() << '\n'
                  << "improved=" << (result.improved ? "true" : "false") << '\n'
                  << "stop=" << constantFitStopName(result.stop) << '\n'
                  << "fit_cpu_seconds=" << cpuSeconds << '\n'
                  << "loss_attempts=" << objective.work().lossAttempts << '\n'
                  << "proposals=" << result.proposals << '\n'
                  << "sweeps=" << result.sweeps << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "refinement_error=" << error.what() << '\n';
        return 2;
    }
}
