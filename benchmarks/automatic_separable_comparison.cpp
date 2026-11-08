#include <AutomaticSeparableRegression.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {
using Function = std::function<double(double, double)>;
struct Case {
    const char* name;
    Function reference;
    std::optional<SeparableForm> form;
    const char* expectedExpression;
};

std::uint64_t mix(std::uint64_t value) {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

std::vector<RegressionRow> grid(const Function& function,
                                std::optional<std::uint64_t> noiseSeed = {}) {
    std::vector<RegressionRow> rows;
    std::size_t i = 0;
    for (const double x : {-1.0, -0.5, 0.0, 0.5, 1.0}) {
        std::size_t j = 0;
        for (const double y : {-1.0, -0.25, 0.5, 1.0, 1.25}) {
            const auto observation = static_cast<std::uint64_t>(i * 5 + j);
            double perturbation = 0.0;
            if (noiseSeed) {
                const double unit =
                    static_cast<double>(mix(*noiseSeed + observation) >> 11U) * 0x1.0p-53;
                perturbation = (2.0 * unit - 1.0) * 1e-4;
            }
            rows.push_back({{x, y}, function(x, y) + perturbation});
            ++j;
        }
        ++i;
    }
    return rows;
}

std::vector<RegressionRow> heldOutGrid(const Function& function) {
    std::vector<RegressionRow> rows;
    for (const double x : {-0.8, -0.2, 0.2, 0.8})
        for (const double y : {-0.6, 0.3, 0.8, 1.1})
            rows.push_back({{x, y}, function(x, y)});
    return rows;
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
    const auto scaffoldSource = parseExpression(form == SeparableForm::Additive ? "x0+x1" : "x0*x1",
                                                2, {}, config.modelLimits);
    MultiScalarExpressionTemplate scaffold(scaffoldSource, 0, 1, config.componentLimits,
                                           config.modelLimits);
    return MultiComponentTemplateProblem::create(
        scaffold, RegressionProblem::create(rows, config.modelLimits), {{{0}, {1}}},
        {{config.grammars[0].operators, config.grammars[1].operators}});
}

void report(const char* mode, const char* caseName, const Function& reference,
            const char* expectedExpression, const std::vector<RegressionRow>& heldOut,
            const SymbolicRegressionResult& fit, std::uint64_t detectionOps) {
    double squared = 0.0;
    std::size_t nodes = 0;
    bool valid = static_cast<bool>(fit.best);
    bool exactStructure = false;
    if (fit.best) {
        nodes = fit.best->expression().nodeCount();
        if (expectedExpression)
            exactStructure = fit.best->expression().sameRepresentation(
                parseExpression(expectedExpression, 2, {}, fit.best->expression().limits()));
        for (const auto& row : heldOut) {
            const auto prediction = fit.best->expression().evaluate(row.inputs);
            if (!prediction.valid()) {
                valid = false;
                break;
            }
            const double residual = prediction.value() - reference(row.inputs[0], row.inputs[1]);
            squared += residual * residual;
        }
    }
    std::cout << mode << ',' << caseName << ',' << (fit.best ? fit.best->fitness() : -1.0) << ','
              << (valid ? std::sqrt(squared / heldOut.size()) : -1.0) << ',' << nodes << ','
              << (fit.best ? (exactStructure ? "yes" : "no") : "n/a") << ','
              << fit.candidateEvaluations << ',' << fit.trainingWork.rowAttempts << ','
              << detectionOps << ',' << (fit.best ? fit.best->text() : "") << '\n';
}
} // namespace

int main() {
    const std::vector<Case> cases{
        {"additive_sin_plus_square", [](double x, double y) { return std::sin(x) + y * y; },
         SeparableForm::Additive, "sin(x0)+powi(x1,2)"},
        {"multiplicative_product", [](double x, double y) { return (1.0 + x) * (1.0 + y * y); },
         SeparableForm::Multiplicative, "(1+x0)*(1+powi(x1,2))"},
        {"nonseparable_sin_sum",
         [](double x, double y) { return std::sin(x + y); },
         {},
         "sin(x0+x1)"},
        {"nonseparable_cross_term",
         [](double x, double y) { return x + y + x * y; },
         {},
         "x0+x1+x0*x1"},
        {"degenerate_zero", [](double, double) { return 0.0; }, {}, "0"}};
    const auto config = configFor();
    std::cout << "mode,case,training_mse,heldout_rmse,nodes,exact_postfix_structure,candidates,"
                 "training_rows,detection_ops,expression\n";
    for (const auto& testCase : cases) {
        const auto heldOut = heldOutGrid(testCase.reference);
        const std::array<std::optional<std::uint64_t>, 3> noiseSeeds{std::nullopt, 1103, 2309};
        for (const auto noiseSeed : noiseSeeds) {
            const auto training = grid(testCase.reference, noiseSeed);
            const std::string caseName =
                std::string(testCase.name) +
                (noiseSeed ? (*noiseSeed == 1103 ? "_noise_a" : "_noise_b") : "_clean");
            for (const std::uint64_t seed : {17, 29}) {
                const auto automatic = [&] {
                    RandomContext random(seed);
                    return automaticSeparableRegression(training, config, random);
                }();
                if (automatic.search)
                    report("automatic", caseName.c_str(), testCase.reference,
                           testCase.expectedExpression, heldOut, *automatic.search,
                           automatic.detection.work.mathematicalOperations);
                else
                    std::cout << "automatic," << caseName << ",-1,-1,0,n/a,0,0,"
                              << automatic.detection.work.mathematicalOperations << ','
                              << automatic.detection.diagnostic << '\n';

                if (testCase.form) {
                    const auto domain = manualDomain(training, *testCase.form, config);
                    RandomContext templateRandom(seed);
                    const auto manual = multiComponentTemplateRegression(
                        domain, config.grammars, config.search, templateRandom);
                    report("manual_template", caseName.c_str(), testCase.reference,
                           testCase.expectedExpression, heldOut, manual, 0);
                }
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
                report("ordinary", caseName.c_str(), testCase.reference,
                       testCase.expectedExpression, heldOut, ordinary, 0);
            }
        }
    }
}
