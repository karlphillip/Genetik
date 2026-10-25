#include <ExpressionParsing.hpp>
#include <ExpressionDerivative.hpp>
#include <ExpressionFormatting.hpp>
#include <ScalarExpressionCheckpoint.hpp>
#include <ScalarExpressionTemplate.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <barrier>
namespace {
void check(bool v, const char *s) {
    if (!v)
        throw std::runtime_error(s);
}
auto parse(std::string_view text) { return parseExpression(text, 1); }
void references(const char *path) {
    std::ifstream input(path);
    std::string line;
    std::size_t count = 0;
    while (std::getline(input, line)) {
        std::istringstream row(line);
        std::string formula, xs, vs, ds;
        std::getline(row, formula, '\t');
        std::getline(row, xs, '\t');
        std::getline(row, vs, '\t');
        std::getline(row, ds);
        const double x = std::stod(xs), v = std::stod(vs), d = std::stod(ds);
        auto e = parse(formula);
        auto p = e.evaluate(std::array{x});
        auto partial = evaluatePartial(e, std::array{x}, 0);
        check(p.valid() && partial.derivativeAvailable(), "scientific value/derivative available");
        check(std::bit_cast<std::uint64_t>(p.value()) == std::bit_cast<std::uint64_t>(partial.value()),
              "checked primal bits agree");
        check(std::abs(p.value() - v) <= 1e-14 + 1e-13 * std::abs(v) &&
                  std::abs(partial.derivative() - d) <= 1e-14 + 1e-13 * std::abs(d),
              "independent100digit analytic reference");
        check(parse(e.toString()).sameRepresentation(e), "canonical scientific operator/metadata roundtrip");
        check(!formatExpression(e).empty() && !formatExpression(e, ExpressionTextFormat::LaTeX).empty(),
              "scientific Unicode/LaTeX");
        ++count;
    }
    check(input.eof() && count >= 60, "all scientific references read");
    std::cout << count << " independent high-precision references passed\n";
}
void domains() {
    for (auto text :
         {"pow(0,0)", "pow(0,-1)", "pow(-2,.5)", "powi(0,-1)", "log1p(-1)", "log2(0)", "log10(-1)", "asin(2)",
          "acos(-2)", "atan2(0,0)", "clamp(0,2,1)", "expm1(1000)", "cosh(1000)"})
        check(!parse(text).evaluate(std::array{0.}).valid(), "strict new domain/overflow invalidity");
    for (auto [text, x] : std::array<std::pair<const char *, double>, 7>{{{"abs(x0)", 0},
                                                                          {"min(x0,0)", 0},
                                                                          {"max(x0,0)", 0},
                                                                          {"clamp(x0,0,1)", 0},
                                                                          {"cbrt(x0)", 0},
                                                                          {"asin(x0)", 1},
                                                                          {"atan2(x0,-1)", 0}}}) {
        auto d = evaluatePartial(parse(text), std::array{x}, 0);
        check(d.valueAvailable() && !d.derivativeAvailable(),
              "finite primal separate from nondifferentiable/singular rule");
    }
    check(evaluatePartial(parse("pow(-2,x0)"), std::array{2.}, 0).status() == DerivativeStatus::Unavailable,
          "negative base active real exponent unsupported");
    check(evaluatePartial(parse("pow(x0,3)"), std::array{-2.}, 0).derivative() == 12,
          "negative base fixed integer power derivative");
    check(evaluatePartial(parse("atan(x0)"), std::array{1e200}, 0).derivativeAvailable(),
          "stable large atan derivative");
    check(evaluatePartial(parse("atan2(x0,1e300)"), std::array{1e300}, 0).derivativeAvailable(),
          "scaled atan2 derivative avoids overflowing square");
    check(parse("powi(x0,-2)").evaluate(std::array{1e200}).value() == 0,
          "negative power inverse-first underflow remains valid");
    check(std::signbit(parse("min(x0,0)").evaluate(std::array{-0.}).value()),
          "left-biased min signed-zero tie");
    auto e = parse("clamp(sin(x0),0,1)");
    auto d = evaluatePartial(e, std::array{.5}, 0);
    DerivativeLimits l;
    l.maxOperations = d.work().operations();
    check(evaluatePartial(e, std::array{.5}, 0, l).derivativeAvailable(),
          "exact advanced derivative work boundary");
    --l.maxOperations;
    check(evaluatePartial(e, std::array{.5}, 0, l).status() == DerivativeStatus::ResourceLimit,
          "derivative work exhaustion");
    auto scaffold = ScalarExpressionTemplate(e, 0, {1, 1, 2}, {6, 4, 2});
    check(scaffold.materialize(Expression::variable(0, 1)).sameRepresentation(e),
          "ternary template preserves grouping");
}
void grammar() {
    auto p = RegressionProblem::create({{{.5}, 1}, {{2}, 2}}, {31, 6, 10, 1000});
    ExpressionGrammar g;
    g.operators = {ExpressionKind::Clamp, ExpressionKind::Pow, ExpressionKind::NegativePowi,
                   ExpressionKind::Tanh};
    ExpressionVariation variation(p, g);
    std::mt19937 rng(19);
    for (int i = 0; i < 100; ++i) {
        auto a = variation.initialize(rng), b = variation.initialize(rng);
        auto x = variation.mutation(a, 1, rng), y = b;
        variation.crossover(a, b, x, y, 1, rng);
        for (const auto *c : {&x, &y}) {
            check(genetik_detail::extendedGrammarAccepts(c->expression(), g),
                  "constant exponent grammar survives variation");
            check(parse(c->text()).sameRepresentation(c->expression()), "new arity-aware varied roundtrip");
        }
    }
    auto expression = parse("clamp(pow(x0,1.7),0,4)");
    ExpressionCandidate candidate(expression, p);
    candidate.calculateFitness();
    CheckpointWriter w;
    CandidateCheckpointCodec<ExpressionCandidate>::encode(w, candidate);
    CheckpointReader r(w.data());
    auto restored = CandidateCheckpointCodec<ExpressionCandidate>::decode(r, {p, {}});
    r.finish();
    check(restored.expression().sameRepresentation(expression) &&
              restored.evaluation() == candidate.evaluation(),
          "append-only scalar node codec");
}
void overlap() {
    auto expression = parse("tanh(x0)+cbrt(x0)");
    std::barrier gate(4);
    ExecutionContext execution({4});
    execution.run(4, [&](std::size_t i) {
        gate.arrive_and_wait();
        for (int n = 0; n < 50; ++n) {
            auto d = evaluatePartial(expression, std::array{static_cast<double>(i)}, 0);
            check(d.valueAvailable(), "shared new scientific expression");
            check(i == 0 ? !d.derivativeAvailable() : d.derivativeAvailable(),
                  "private success/unavailable outcomes");
        }
    });
    check(execution.peakTasks() == 4, "forced scientific overlap");
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc > 2) {
            overlap();
        } else {
            if (argc != 2)
                throw std::invalid_argument("reference fixture path required");
            references(argv[1]);
            domains();
            grammar();
            overlap();
        }
        std::cout << "D5 scientific contracts passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
