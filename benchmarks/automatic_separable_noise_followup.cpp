#include <AutomaticSeparableRegression.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace {
using Key = std::pair<std::string, std::string>;
using Data = std::map<Key, std::vector<RegressionRow>>;

std::string csvField(const std::string& value) {
    std::string escaped = "\"";
    for (const char character : value) {
        if (character == '"')
            escaped.push_back('"');
        escaped.push_back(character);
    }
    escaped.push_back('"');
    return escaped;
}

double independentlyEvaluate(const Expression& expression, const std::vector<double>& inputs) {
    std::vector<double> stack;
    stack.reserve(expression.nodeCount());
    for (const auto& node : expression.nodes()) {
        switch (node.kind()) {
        case ExpressionKind::Variable:
            stack.push_back(inputs.at(node.variableIndex()));
            break;
        case ExpressionKind::Constant:
            stack.push_back(node.constantValue());
            break;
        case ExpressionKind::Sin:
            if (stack.empty())
                throw std::runtime_error("independent scorer stack underflow");
            stack.back() = std::sin(stack.back());
            break;
        case ExpressionKind::Powi: {
            if (stack.empty())
                throw std::runtime_error("independent scorer stack underflow");
            const auto base = stack.back();
            double value = 1.0;
            for (unsigned exponent = 0; exponent < node.exponent(); ++exponent)
                value *= base;
            stack.back() = value;
            break;
        }
        case ExpressionKind::Add:
        case ExpressionKind::Multiply: {
            if (stack.size() < 2)
                throw std::runtime_error("independent scorer stack underflow");
            const auto right = stack.back();
            stack.pop_back();
            const auto left = stack.back();
            stack.back() = node.kind() == ExpressionKind::Add ? left + right : left * right;
            break;
        }
        default:
            throw std::runtime_error("benchmark scorer received an unfrozen operator");
        }
        if (!std::isfinite(stack.back()))
            throw std::runtime_error("nonfinite independent benchmark prediction");
    }
    if (stack.size() != 1)
        throw std::runtime_error("independent scorer did not produce one result");
    return stack.back();
}

Data readData(const char* path) {
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error("cannot open frozen dataset file");
    std::string line;
    std::getline(input, line);
    Data data;
    while (std::getline(input, line)) {
        std::istringstream fields(line);
        std::string family, split, noise, x, y, target;
        if (!std::getline(fields, family, ',') || !std::getline(fields, split, ',') ||
            !std::getline(fields, noise, ',') || !std::getline(fields, x, ',') ||
            !std::getline(fields, y, ',') || !std::getline(fields, target))
            throw std::runtime_error("malformed frozen dataset row");
        data[{family, split + ":" + noise}].push_back(
            {{std::stod(x), std::stod(y)}, std::stod(target)});
    }
    return data;
}

std::optional<SeparableForm> trueForm(const std::string& family) {
    if (family == "additive")
        return SeparableForm::Additive;
    if (family == "multiplicative")
        return SeparableForm::Multiplicative;
    return {};
}

AutomaticSeparableConfig configFor() {
    AutomaticSeparableConfig config;
    config.grammars[0].operators = {ExpressionKind::Sin, ExpressionKind::Add,
                                    ExpressionKind::Multiply};
    config.grammars[0].constants = {-1.0, 0.0, 1.0};
    config.grammars[0].initialMaxDepth = config.grammars[0].mutationMaxDepth = 2;
    config.grammars[1].operators = {ExpressionKind::Add, ExpressionKind::Multiply,
                                    ExpressionKind::Powi};
    config.grammars[1].constants = {-1.0, 0.0, 1.0};
    config.grammars[1].exponents = {2};
    config.grammars[1].initialMaxDepth = config.grammars[1].mutationMaxDepth = 2;
    config.search.population = 24;
    config.search.elites = 2;
    config.search.maxGenerationAttempts = 4;
    config.search.maxEvaluations = 96;
    config.search.maxTrainingLossAttempts = 96 * 25;
    return config;
}

std::shared_ptr<const MultiComponentTemplateProblem>
manualDomain(const std::vector<RegressionRow>& rows, SeparableForm form,
             const AutomaticSeparableConfig& config) {
    const auto source = parseExpression(form == SeparableForm::Additive ? "x0+x1" : "x0*x1", 2, {},
                                        config.modelLimits);
    MultiScalarExpressionTemplate scaffold(source, 0, 1, config.componentLimits,
                                           config.modelLimits);
    return MultiComponentTemplateProblem::create(
        scaffold, RegressionProblem::create(rows, config.modelLimits), {{{0}, {1}}},
        {{config.grammars[0].operators, config.grammars[1].operators}});
}

void printFit(const char* mode, const Key& key, std::uint64_t seed,
              const std::vector<RegressionRow>& assessment, const SymbolicRegressionResult& fit,
              std::uint64_t detectorWork) {
    double squared = 0.0;
    bool valid = static_cast<bool>(fit.best);
    std::size_t nodes = 0;
    bool exactStructure = false;
    if (fit.best) {
        nodes = fit.best->expression().nodeCount();
        if (key.first == "additive" || key.first == "multiplicative") {
            const auto expected = parseExpression(key.first == "additive" ? "sin(x0)+powi(x1,2)"
                                                                          : "(1+x0)*(1+powi(x1,2))",
                                                  2, {}, fit.best->expression().limits());
            exactStructure = fit.best->expression().sameRepresentation(expected);
        }
        for (const auto& row : assessment) {
            double prediction = 0.0;
            try {
                prediction = independentlyEvaluate(fit.best->expression(), row.inputs);
            } catch (const std::exception&) {
                valid = false;
                break;
            }
            const double error = prediction - row.target;
            squared += error * error;
        }
    }
    std::cout << "fit," << mode << ',' << key.first << ',' << key.second.substr(6) << ',' << seed
              << ",,," << (fit.best ? fit.best->fitness() : -1.0) << ','
              << (valid ? std::sqrt(squared / assessment.size()) : -1.0) << ',' << nodes << ','
              << (exactStructure ? "yes" : "no") << ',' << fit.candidateEvaluations << ','
              << fit.trainingWork.rowAttempts << ',' << detectorWork << ','
              << csvField(fit.best ? fit.best->text() : "") << '\n';
}

void run(const char* path) {
    const auto data = readData(path);
    const auto config = configFor();
    std::cout << "kind,mode,family,noise,seed,additive,multiplicative,training_mse,assessment_rmse,"
                 "nodes,exact_structure,candidates,training_rows,detection_ops,expression\n";
    for (const auto& [key, training] : data) {
        if (key.second.rfind("train:", 0) != 0)
            continue;
        const auto assessment = data.at({key.first, "assessment:clean"});
        auto strictLimits = config.detection;
        const auto strict = detectSeparableGrid(training, strictLimits);
        auto noisyLimits = strictLimits;
        noisyLimits.measurementNoiseBound = 1e-4;
        const auto bounded = detectSeparableGrid(training, noisyLimits);
        for (const auto& [mode, detection] :
             std::array<std::pair<const char*, SeparabilityDetection>, 2>{
                 {{"strict", strict}, {"bounded_noise", bounded}}}) {
            std::cout << "detect," << mode << ',' << key.first << ',' << key.second.substr(6)
                      << ",0," << static_cast<int>(detection.additive) << ','
                      << static_cast<int>(detection.multiplicative) << ",,,0,n/a,0,0,"
                      << detection.work.mathematicalOperations << ','
                      << csvField(detection.selectedForm
                                      ? (*detection.selectedForm == SeparableForm::Additive
                                             ? "additive"
                                             : "multiplicative")
                                      : detection.diagnostic)
                      << '\n';
            if (!detection.selectedForm)
                continue;
            for (const auto seed : {17ULL, 29ULL}) {
                RandomContext automaticRandom(seed);
                const auto automatic = automaticSeparableRegression(
                    training,
                    [&] {
                        auto selectedConfig = config;
                        selectedConfig.detection =
                            mode == std::string("bounded_noise") ? noisyLimits : strictLimits;
                        return selectedConfig;
                    }(),
                    automaticRandom);
                if (automatic.search)
                    printFit(mode == std::string("strict") ? "automatic_strict"
                                                           : "automatic_bounded_noise",
                             key, seed, assessment, *automatic.search,
                             detection.work.mathematicalOperations);

                if (!trueForm(key.first))
                    continue;
                const auto domain = manualDomain(training, *trueForm(key.first), config);
                RandomContext manualRandom(seed);
                printFit("manual_template", key, seed, assessment,
                         multiComponentTemplateRegression(domain, config.grammars, config.search,
                                                          manualRandom),
                         0);
                auto ordinaryConfig = config.search;
                ordinaryConfig.grammar = config.grammars[0];
                ordinaryConfig.grammar.operators.insert(ordinaryConfig.grammar.operators.end(),
                                                        config.grammars[1].operators.begin(),
                                                        config.grammars[1].operators.end());
                std::sort(ordinaryConfig.grammar.operators.begin(),
                          ordinaryConfig.grammar.operators.end());
                ordinaryConfig.grammar.operators.erase(
                    std::unique(ordinaryConfig.grammar.operators.begin(),
                                ordinaryConfig.grammar.operators.end()),
                    ordinaryConfig.grammar.operators.end());
                ordinaryConfig.grammar.constants = {-1.0, 0.0, 1.0};
                RandomContext ordinaryRandom(seed);
                const auto ordinary = symbolicRegression(RegressionProblem::create(training),
                                                         ordinaryConfig, ordinaryRandom);
                printFit("ordinary", key, seed, assessment, ordinary, 0);
            }
        }
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2)
            throw std::invalid_argument("usage: benchmark DATASETS.csv");
        run(argv[1]);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
