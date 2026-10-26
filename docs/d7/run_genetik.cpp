// D7 benchmark adapter: consumes training rows only and emits a canonical model.
#include <ConstantFitting.hpp>
#include <ExpressionFormatting.hpp>
#include <SymbolicRegression.hpp>

#include <charconv>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

namespace {
double number(std::string_view text) {
    double value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
        throw std::runtime_error("invalid numeric training data");
    return value;
}
std::uint32_t seedValue(std::string_view text) {
    std::uint32_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
        throw std::runtime_error("invalid seed");
    return value;
}
std::vector<RegressionRow> readRows(const char* path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open training CSV");
    std::string line;
    if (!std::getline(input, line) || (line != "x0,y" && line != "x0,x1,y"))
        throw std::runtime_error("unexpected training header");
    const std::size_t dimensions = line == "x0,y" ? 1 : 2;
    std::vector<RegressionRow> rows;
    while (std::getline(input, line)) {
        std::istringstream fields(line);
        std::string field;
        RegressionRow row;
        for (std::size_t i = 0; i < dimensions + 1; ++i) {
            if (!std::getline(fields, field, ',')) throw std::runtime_error("short training row");
            const double value = number(field);
            if (i < dimensions) row.inputs.push_back(value);
            else row.target = value;
        }
        if (std::getline(fields, field, ',')) throw std::runtime_error("long training row");
        rows.push_back(std::move(row));
    }
    return rows;
}
}

int main(int argc, char** argv) {
    try {
        if (argc != 4) throw std::runtime_error("usage: run_genetik TRAIN.csv SEED WALL_SECONDS");
        const auto seed = seedValue(argv[2]);
        const auto wallSeconds = number(argv[3]);
        if (!(wallSeconds > 0) || wallSeconds > 3600) throw std::runtime_error("invalid wall allowance");
        auto rows = readRows(argv[1]);
        const auto dimensions = rows.front().inputs.size();
        const auto problem = RegressionProblem::create(rows, ExpressionLimits{25, 8, 8.0});
        SymbolicRegressionConfig config;
        config.population = 128;
        config.maxGenerationAttempts = std::numeric_limits<std::uint32_t>::max();
        config.maxEvaluations = std::numeric_limits<std::uint64_t>::max();
        config.maxTrainingLossAttempts = std::numeric_limits<std::uint64_t>::max();
        config.grammar.operators = {ExpressionKind::Add, ExpressionKind::Subtract,
                                    ExpressionKind::Multiply, ExpressionKind::Divide,
                                    ExpressionKind::Sin, ExpressionKind::Exp,
                                    ExpressionKind::Log};
        config.grammar.continuousProbability = 1.0;
        config.grammar.continuousLower = -8.0;
        config.grammar.continuousUpper = 8.0;
        config.grammar.initialMaxDepth = 8;
        config.grammar.mutationMaxDepth = 8;
        ConstantFitConfig fitting;
        fitting.lower = -8.0;
        fitting.upper = 8.0;
        fitting.initialStep = 1.0;
        fitting.minimumStep = 1e-6;
        fitting.maxLossAttempts = 64;
        fitting.maxSweeps = 32;
        fitting.maxProposals = 2048;
        config.fitting = fitting;

        ExecutionContext execution(ExecutionConfig{1});
        RandomContext random(seed, execution);
        std::mutex mutex;
        std::condition_variable cv;
        bool finished = false;
        std::thread timer([&] {
            std::unique_lock lock(mutex);
            if (!cv.wait_for(lock, std::chrono::duration<double>(wallSeconds), [&] { return finished; }))
                execution.requestStop();
        });
        const auto start = std::chrono::steady_clock::now();
        auto result = symbolicRegression(problem, config, random);
        const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        {
            std::lock_guard lock(mutex);
            finished = true;
        }
        cv.notify_all();
        timer.join();

        std::cout << "seed=" << seed << '\n'
                  << "dimensions=" << dimensions << '\n'
                  << "wall_seconds=" << elapsed << '\n'
                  << "candidate_evaluations=" << result.candidateEvaluations << '\n'
                  << "training_loss_attempts=" << result.trainingWork.lossAttempts << '\n'
                  << "training_row_attempts=" << result.trainingWork.rowAttempts << '\n'
                  << "initial_best_mse=" << (result.initialBestMSE ? std::to_string(*result.initialBestMSE) : "none") << '\n'
                  << "stop=" << searchStopName(result.stoppingReason) << '\n';
        if (result.best) {
            std::cout << "expression=" << formatExpression(result.best->expression(), ExpressionTextFormat::Canonical) << '\n'
                      << "training_mse=" << result.best->fitness() << '\n'
                      << "best_generation=" << (result.bestGeneration ? std::to_string(*result.bestGeneration) : "none") << '\n';
        } else {
            std::cout << "expression=\ntraining_mse=none\nbest_generation=none\n";
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "adapter_error=" << error.what() << '\n';
        return 2;
    }
}
