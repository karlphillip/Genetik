#include <DiscreteRunCheckpoint.hpp>
#include <DiscreteExpressionParsing.hpp>
#include <DiscreteExpressionFormatting.hpp>
#include <fstream>
#include <iostream>
#include <barrier>
namespace {
void check(bool v, const char *s) {
    if (!v)
        throw std::runtime_error(s);
}
void references(const char *path) {
    std::ifstream in(path);
    std::string line;
    std::size_t count = 0;
    while (std::getline(in, line)) {
        auto tab = line.find('\t');
        auto e = parseDiscreteExpression(line.substr(0, tab), 0);
        auto r = e.evaluate({}, DiscreteEvaluationLimits{100000, 100000, 1048576});
        const auto answer = line.substr(tab + 1);
        if (answer == "invalid")
            check(r.status == DiscreteEvaluationStatus::MathematicalInvalid,
                  "independent invalid integer fixture");
        else
            check(r.valid() && r.value->integer() == std::stoll(answer),
                  "independent Python integer reference");
        check(parseDiscreteExpression(e.toString(), 0).sameRepresentation(e),
              "integer canonical operator identities");
        for (auto f : {DiscreteFormat::Unicode, DiscreteFormat::LaTeX})
            check(!formatDiscreteExpression(e, f).empty(), "integer presentation");
        ++count;
    }
    check(in.eof() && count > 3000, "all independent integer rows");
    std::cout << count << " independent integer references passed\n";
}
void search() {
    const auto evaluate = [](std::string_view text, DiscreteEvaluationLimits l = DiscreteEvaluationLimits{}) {
        return parseDiscreteExpression(text, 0).evaluate({}, l);
    };
    auto r = evaluate("first(k0,-7,10,k0%3==2)");
    check(r.valid() && r.value->integer() == -7, "negative Euclidean first witness");
    r = evaluate("first(k0,1,3,k0==2)");
    check(r.valid() && r.value->integer() == 2 && r.iterations == 2,
          "first witness short-circuit and actual iterations");
    for (auto text : {"first(k0,1,0,1%0==0)", "first(k0,1,3,false)"})
        check(evaluate(text).status == DiscreteEvaluationStatus::NoWitness,
              "no witness is explicit, no fabricated integer");
    check(evaluate("first(k0,1,3,if(k0==1,true,1%0==0))").valid(),
          "later predicates unvisited after first witness");
    check(evaluate("first(k0,1,3,if(k0==1,false,1%0==0))").status ==
              DiscreteEvaluationStatus::MathematicalInvalid,
          "invalid required predicate remains invalid");
    check(evaluate("first(k0,-9223372036854775808,9223372036854775807,true)").status ==
              DiscreteEvaluationStatus::ResourceExhausted,
          "range admission before first predicate");
}
void work() {
    auto e = parseDiscreteExpression(
        "modular_power(9223372036854775806,9223372036854775807,9223372036854775807)", 0);
    auto l = DiscreteEvaluationLimits{100000, 100000, 1048576};
    auto r = e.evaluate({}, l);
    check(r.valid(), "large modular power representable");
    l.maxOperations = r.operations - 1;
    check(e.evaluate({}, l).status == DiscreteEvaluationStatus::ResourceExhausted,
          "internal math work admission");
    l.maxOperations = r.operations;
    check(e.evaluate({}, l).valid(), "exact internal work boundary");
    int polls = 0;
    check(e.evaluateWithCancellation(
               {}, DiscreteEvaluationLimits{100000, 100000, 1048576},
               [&] {
                   return ++polls == 15;
               }).status == DiscreteEvaluationStatus::Cancelled,
          "bounded math-loop cancellation");
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc != 2)
            throw std::invalid_argument("integer references required");
        references(argv[1]);
        bool rejected = false;
        try {
            (void)parseDiscreteExpression("first(k0,1,1,0)", 0);
        } catch (const DiscreteParseError &) {
            rejected = true;
        }
        check(rejected, "first predicate type");
        auto relationship = parseDiscreteExpression("gcd(x0,x1)*lcm(x0,x1)", 2);
        for (std::int64_t a = 1; a <= 12; ++a)
            for (std::int64_t b = 1; b <= 12; ++b)
                check(relationship.evaluate(std::array{a, b}).value->integer() == a * b,
                      "independent positive gcd/lcm relationship");
        work();
        search();
        for (auto [text, status] : std::array<std::pair<const char *, DiscreteEvaluationStatus>, 3>{
                 {{"first(k0,1,3,k0==2)", DiscreteEvaluationStatus::Valid},
                  {"first(k0,1,3,false)", DiscreteEvaluationStatus::NoWitness},
                  {"first(k0,1,0,1%0==0)", DiscreteEvaluationStatus::NoWitness}}})
            check(parseDiscreteExpression(text, 0).evaluate({}).status == status, "bounded first outcome");
        std::cout << "D5 advanced integer contracts passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
