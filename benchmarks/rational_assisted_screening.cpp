#include <RationalSymbolicRegression.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/resource.h>

namespace {
using Clock = std::chrono::steady_clock;
struct Case {
    std::string name;
    unsigned m, n;
    double (*f)(double);
};
double rat11(double x) {
    return (1.25 - .4 * x) / (1 + .3 * x);
}
double rat21(double x) {
    return (.7 + .35 * x - .2 * x * x) / (1 - .25 * x);
}
double cubic(double x) {
    return .4 - .7 * x + .3 * x * x + .12 * x * x * x;
}
double sine(double x) {
    return std::sin(1.3 * x) + .2 * x;
}
std::vector<RegressionRow> grid(double (*f)(double), int count, double start, double step) {
    std::vector<RegressionRow> out;
    for (int i = 0; i < count; ++i) {
        double x = start + i * step;
        out.push_back({{x}, f(x)});
    }
    return out;
}
std::vector<RegressionRow> csv(const std::string& path) {
    std::ifstream in(path);
    if (!in)
        throw std::runtime_error("cannot open historical data: " + path);
    std::string line;
    std::getline(in, line);
    std::vector<RegressionRow> out;
    while (std::getline(in, line)) {
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream row(line);
        double x, y;
        if (row >> x >> y)
            out.push_back({{x}, y});
    }
    return out;
}
struct Splits {
    std::vector<RegressionRow> train, validation, assessment;
};
Splits split(const Case& c, const std::string& dataRoot) {
    if (c.name == "d7_rational_x2") {
        auto all = csv(dataRoot + "/docs/d7/data/rational_x2-train-clean.csv");
        Splits s;
        s.assessment = csv(dataRoot + "/docs/d7/data/rational_x2-test.csv");
        for (std::size_t i = 0; i < all.size(); ++i)
            (i % 4 == 0 ? s.validation : s.train).push_back(all[i]);
        return s;
    }
    return {grid(c.f, 41, -1, 0.05), grid(c.f, 21, -.95, .095), grid(c.f, 41, -.975, .04875)};
}
struct Scores {
    double rmse, nrmse, r2;
};
Scores score(const Expression& e, const std::vector<RegressionRow>& rows) {
    double sum = 0, mean = 0;
    for (const auto& r : rows)
        mean += r.target;
    mean /= rows.size();
    double total = 0;
    for (const auto& r : rows) {
        auto v = e.evaluate(r.inputs);
        if (!v.valid())
            return {INFINITY, INFINITY, -INFINITY};
        double d = v.value() - r.target;
        sum += d * d;
        double z = r.target - mean;
        total += z * z;
    }
    double rmse = std::sqrt(sum / rows.size()), rms = 0;
    for (const auto& r : rows)
        rms += r.target * r.target;
    rms = std::sqrt(rms / rows.size());
    return {rmse, rmse / std::max(rms, 1e-12), total ? 1 - sum / total : 1};
}
std::size_t constants(const Expression& e) {
    const auto nodes = e.nodes();
    return std::count_if(nodes.begin(), nodes.end(),
                         [](const auto& n) { return n.kind() == ExpressionKind::Constant; });
}
long peakRss() {
    rusage u{};
    getrusage(RUSAGE_SELF, &u);
    return u.ru_maxrss;
}
void emit(const Case& c, const Splits& s, const char* arm, int seed, const Expression* e,
          double initMs, double totalMs, std::uint64_t initWork, std::uint64_t candidates,
          std::uint64_t losses, std::uint64_t rows, const std::string& stop,
          const std::string& origin, const std::string& initializerStatus = "disabled",
          std::optional<double> initialMSE = {}, std::uint64_t invalid = 0,
          const RationalFitResult* fit = nullptr, int fittedParameters = -1) {
    std::cout << std::quoted(c.name) << ',' << arm << ',' << seed << ',' << stop << ',' << origin
              << ',' << initializerStatus << ',';
    if (!e) {
        std::cout << "\"\",nan,nan,nan,nan,nan,nan,0,0,0,0,0,0,0,0,0,0,0,0,not_proven,NA,\"\"\n";
        return;
    }
    auto tr = score(*e, s.train), va = score(*e, s.validation), te = score(*e, s.assessment);
    std::cout << std::quoted(e->toString()) << ',' << tr.rmse << ',' << va.rmse << ',' << te.rmse
              << ',' << te.nrmse << ',' << te.r2 << ','
              << (initialMSE ? std::sqrt(*initialMSE) : NAN) << ',' << e->nodeCount() << ','
              << e->depth() << ',' << constants(*e) << ',' << initMs << ',' << totalMs << ','
              << initWork << ',' << candidates << ',' << losses << ',' << rows << ',' << invalid
              << ',' << peakRss() << ',';
    if (fit)
        std::cout << fit->arithmeticOperations << ',' << fit->linearAlgebraOperations << ','
                  << fit->lmWork.mathematicalOperations;
    else
        std::cout << "0,0,0";
    std::cout << ",not_proven," << (fittedParameters < 0 ? "NA" : std::to_string(fittedParameters))
              << ",\"";
    for (const auto& n : e->nodes())
        if (n.kind() == ExpressionKind::Constant)
            std::cout << std::setprecision(17) << n.constantValue() << ' ';
    std::cout << "\"\n";
}
SymbolicRegressionConfig config() {
    SymbolicRegressionConfig c;
    c.population = 48;
    c.elites = 2;
    c.maxGenerationAttempts = 12;
    c.maxEvaluations = 624;
    c.maxTrainingLossAttempts = 624;
    c.grammar.operators = {ExpressionKind::Add, ExpressionKind::Subtract, ExpressionKind::Multiply,
                           ExpressionKind::Divide, ExpressionKind::Powi};
    c.grammar.constants = {-2, -1, 0, 1, 2};
    c.grammar.exponents = {0, 1, 2, 3};
    return c;
}
RationalConfig rationalConfig(const Case& c) {
    RationalConfig r;
    r.maximumNumeratorDegree = std::max(3u, c.m);
    r.maximumDenominatorDegree = std::max(3u, c.n);
    r.maximumParameters = 8;
    r.expressionLimits = {127, 16, 100, 1024};
    r.maximumAbsoluteCoefficient = 100;
    r.complexityWeight = 1;
    r.refineWithLM = false;
    r.maximumArithmeticOperations = 2'000'000;
    r.maximumLinearAlgebraOperations = 50'000'000;
    return r;
}
void run(const Case& c, const Splits& s) {
    auto problem = RegressionProblem::create(s.train, {127, 16, 100, 1024});
    auto rc = rationalConfig(c);
    const auto fitStart = Clock::now();
    std::vector<RationalObservation> obs;
    for (const auto& r : s.train)
        obs.push_back({r.inputs[0], r.target});
    auto fit = fitRationalFunction(obs, rc);
    double fitMs = std::chrono::duration<double, std::milli>(Clock::now() - fitStart).count();
    auto seedBatch = rationalInitialExpressions(fit, 2);
    if (fit.status == RationalStatus::Success && fit.selected) {
        const auto& m = fit.models[*fit.selected];
        const int p = static_cast<int>(m.numeratorDegree + m.denominatorDegree + 1);
        emit(c, s, "B_rational", -1, &m.expression, fitMs, fitMs,
             genetik_rational_detail::combinedLinearAlgebraWork(fit), 0,
             fit.trainingLossEvaluations, fit.trainingLossEvaluations * s.train.size(),
             rationalStatusName(fit.status), "constructive", rationalStatusName(fit.status),
             m.trainingSSE / s.train.size(), fit.invalidModels, &fit, p);
    } else
        emit(c, s, "B_rational", -1, nullptr, fitMs, fitMs, 0, 0, fit.trainingLossEvaluations, 0,
             rationalStatusName(fit.status), "unavailable", rationalStatusName(fit.status), {},
             fit.invalidModels, &fit);
    for (int seed : {11, 29, 47}) {
        auto cfg = config();
        RandomContext random(seed);
        auto t = Clock::now();
        auto out = symbolicRegression(problem, cfg, random);
        double elapsed = std::chrono::duration<double, std::milli>(Clock::now() - t).count();
        auto expr = out.best ? &out.best->expression() : nullptr;
        emit(c, s, "A_symbolic", seed, expr, 0, elapsed, 0, out.candidateEvaluations,
             out.trainingWork.lossAttempts, out.trainingWork.rowAttempts,
             searchStopName(out.stoppingReason), "evolution", "disabled", out.initialBestMSE,
             out.trainingWork.invalidLosses, nullptr, 0);
        cfg.initialExpressionSlots = 2;
        cfg.maximumInitializerWorkUnits = 50'000'000;
        InitialExpressionProvider cached =
            [seedBatch](const RegressionProblem&, std::size_t maxCount, std::uint64_t maxWork) {
                if (seedBatch.expressions.size() > maxCount || seedBatch.workUnits > maxWork)
                    throw std::runtime_error("cached initializer exceeds the frozen allowance");
                return seedBatch;
            };
        RandomContext hybridRandom(seed);
        t = Clock::now();
        auto hybrid = symbolicRegression(problem, cfg, hybridRandom, {}, cached);
        const double searchMs = std::chrono::duration<double, std::milli>(Clock::now() - t).count();
        elapsed = fitMs + searchMs;
        expr = hybrid.best ? &hybrid.best->expression() : nullptr;
        const auto seedEnd =
            hybrid.rawInitialExpressions.begin() +
            std::min(cfg.initialExpressionSlots, hybrid.rawInitialExpressions.size());
        const bool seeded = expr && std::find(hybrid.rawInitialExpressions.begin(), seedEnd,
                                              hybrid.best->text()) != seedEnd;
        int seededParameters = -1;
        if (seeded)
            for (const auto& model : fit.models)
                if (genetik_rational_detail::sameNodes(model.expression, *expr)) {
                    seededParameters =
                        static_cast<int>(model.numeratorDegree + model.denominatorDegree + 1);
                    break;
                }
        emit(c, s, "C_hybrid", seed, expr, fitMs, elapsed, hybrid.initializerWorkUnits,
             hybrid.candidateEvaluations, hybrid.trainingWork.lossAttempts,
             hybrid.trainingWork.rowAttempts, searchStopName(hybrid.stoppingReason),
             seeded ? "constructive_seed_retained" : "evolution", hybrid.initializerStatus,
             hybrid.initialBestMSE, hybrid.trainingWork.invalidLosses, &fit, seededParameters);
    }
}
} // namespace
int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: rational_assisted_screening <repo-root>\n";
        return 2;
    }
    std::string root = argv[1];
    std::cout
        << std::setprecision(10)
        << "case,arm,seed,stop,origin,initializer_status,expression,train_rmse,validation_rmse,"
           "assessment_rmse,assessment_nrmse,assessment_r2,initial_train_rmse,nodes,depth,constant_"
           "nodes,initializer_ms,end_to_end_ms,initializer_work,candidate_calls,loss_attempts,row_"
           "attempts,invalid_count,campaign_process_peak_rss_kib,qr_arithmetic,qr_linear_algebra,"
           "qr_lm_math,symbolic_equivalence,fitted_parameter_count,constants\n";
    Case cases[] = {{"d7_rational_x2", 2, 2, nullptr},
                    {"independent_rational_11", 1, 1, rat11},
                    {"independent_rational_21", 2, 1, rat21},
                    {"nonrational_cubic", 3, 0, cubic},
                    {"nonrational_sine", 0, 0, sine}};
    for (const auto& c : cases) {
        auto s = split(c, root);
        run(c, s);
    }
}
