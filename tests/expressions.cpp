#include <Expression.hpp>
#include <array>
#include <bit>
#include <cfenv>
#include <cstdint>
#include <iostream>
#include <locale>
#include <string_view>

static void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class E, class F> void throws(F action) {
    bool caught = false;
    try { action(); } catch (const E&) { caught = true; }
    check(caught, "expected public API error");
}
using K = ExpressionKind;
using N = ExpressionNode;
static Expression binary(K kind, const Expression& a, const Expression& b) { return Expression::binary(kind, a, b); }
static double scalar(const Expression& e) { return e.evaluate({}).value(); }
static void leavesAndBindings() {
    check(std::fegetround() == FE_TONEAREST, "qualification requires round-to-nearest");
    const auto zero = Expression::constant(0);
    check(zero.evaluate({}).valid() && scalar(zero) == 0 && zero.nodeCount() == 1 && zero.depth() == 1, "constant zero/structure");
    check(scalar(Expression::constant(-7.5)) == -7.5, "negative constant");
    const auto second = Expression::variable(1, 2);
    check(second.evaluate(std::array{3., -5.}).value() == -5, "variable identity and binding order");
    check(second.toString() == "x1", "variable export identity");
    throws<std::invalid_argument>([&] { second.evaluate(std::array{3.}); });
    throws<std::invalid_argument>([&] { second.evaluate(std::array{3., 4., 5.}); });
    throws<std::invalid_argument>([] { Expression::variable(0, 0); });
    throws<std::invalid_argument>([] { Expression::variable(2, 2); });
    const auto unused = Expression::constant(1, 2);
    for (double bad : {std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN()}) {
        throws<std::invalid_argument>([&] { Expression::constant(bad); });
        throws<std::invalid_argument>([&] { second.evaluate(std::array{0., bad}); });
        throws<std::invalid_argument>([&] { unused.evaluate(std::array{bad, 0.}); });
    }
    throws<std::invalid_argument>([&] { binary(K::Add, zero, second); });
    throws<std::invalid_argument>([&] { binary(K::Variable, zero, zero); });
    throws<std::invalid_argument>([] { N::binary(static_cast<K>(999)); });
    throws<std::logic_error>([] { N::constant(1).variableIndex(); });
    throws<std::logic_error>([] { N::variable(0).constantValue(); });
}
static void arithmetic() {
    const auto x = Expression::variable(0);
    const auto polynomial = binary(K::Add, binary(K::Add, binary(K::Multiply, x, x), x), Expression::constant(1, 1));
    const std::array inputs{-2., -1., 0., 1., 2.};
    const std::array expected{3., 1., 1., 3., 7.};
    for (std::size_t i = 0; i < inputs.size(); ++i)
        check(polynomial.evaluate(std::array{inputs[i]}).value() == expected[i], "independent polynomial table");
    check(polynomial.nodeCount() == 7 && polynomial.depth() == 4, "polynomial counting");
    check(polynomial.toString() == "(((x0 * x0) + x0) + 1.0)", "polynomial grouping export");
    const auto a = Expression::constant(10), b = Expression::constant(3), c = Expression::constant(2);
    check(scalar(binary(K::Subtract, a, b)) == 7, "left minus right");
    check(scalar(binary(K::Subtract, b, a)) == -7, "subtraction operand order");
    const auto left = binary(K::Subtract, binary(K::Subtract, a, b), c);
    const auto right = binary(K::Subtract, a, binary(K::Subtract, b, c));
    check(scalar(left) == 5 && scalar(right) == 9, "subtraction grouping");
    check(left.toString() == "((10.0 - 3.0) - 2.0)" && right.toString() == "(10.0 - (3.0 - 2.0))", "distinct grouping exported");
    check(scalar(binary(K::Multiply, Expression::constant(-2), b)) == -6, "negative product");
    // Separate binary64 nodes must round the product to 1 before subtracting 1.
    const auto product = binary(K::Multiply, Expression::variable(0, 3), Expression::variable(1, 3));
    const auto unfused = binary(K::Add, product, Expression::variable(2, 3));
    const std::array bindings{1 + std::ldexp(1., -27), 1 - std::ldexp(1., -27), -1.};
    check(unfused.evaluate(bindings).value() == 0, "multiplication/addition nodes must not fuse");
    check(std::fma(bindings[0], bindings[1], bindings[2]) == -std::ldexp(1., -54), "independent fused counterexample");
    // Grouping also matters without overflow: binary64 loses the small addend here.
    const auto large = Expression::constant(0x1p54), one = Expression::constant(1);
    check(scalar(binary(K::Subtract, binary(K::Add, large, one), large)) == 0, "node rounding is preserved");
}
static void numericalOutcomes() {
    const auto big = Expression::constant(std::numeric_limits<double>::max());
    const auto overflow = binary(K::Multiply, big, Expression::constant(2));
    auto result = overflow.evaluate({});
    check(!result.valid() && result.status() == ExpressionStatus::MathematicalInvalidity && result.failedNode() == 2, "overflow is explicit with node diagnostic");
    throws<std::logic_error>([&] { result.value(); });
    const auto masked = binary(K::Multiply, Expression::constant(0), binary(K::Multiply, big, big));
    result = masked.evaluate({});
    check(!result.valid() && result.failedNode() == 3, "zero cannot mask inner overflow");
    check(!binary(K::Add, big, big).evaluate({}).valid(), "addition overflow");
    check(!binary(K::Subtract, Expression::constant(-std::numeric_limits<double>::max()), big).evaluate({}).valid(), "subtraction overflow");
    // Left subtree is evaluated before right; first failure has a stable postfix position.
    const auto twoFailures = binary(K::Add, overflow, overflow);
    check(twoFailures.evaluate({}).failedNode() == 2, "left subtree failure precedes right");
    const auto negativeZero = Expression::constant(-0.);
    check(std::signbit(scalar(negativeZero)) && negativeZero.toString() == "(-0.0)", "signed zero constant");
    check(!std::signbit(scalar(Expression::constant(0))), "positive zero constant");
    check(std::signbit(scalar(binary(K::Multiply, negativeZero, Expression::constant(3)))), "negative zero arithmetic");
    check(std::signbit(Expression::variable(0).evaluate(std::array{-0.}).value()), "signed variable zero");
    const double subnormal = std::numeric_limits<double>::min() / 2;
    check(subnormal > 0 && subnormal < std::numeric_limits<double>::min(), "platform gradual underflow qualification");
    check(scalar(binary(K::Multiply, Expression::constant(std::numeric_limits<double>::min()), Expression::constant(.5))) == subnormal,
          "finite subnormal accepted");
    check(scalar(Expression::constant(std::numeric_limits<double>::denorm_min())) > 0, "smallest subnormal constant");
    const auto underflow = binary(K::Multiply, Expression::constant(-std::numeric_limits<double>::denorm_min()), Expression::constant(.5));
    const auto tiny = underflow.evaluate({});
    check(tiny.valid() && tiny.value() == 0 && std::signbit(tiny.value()), "finite underflow zero accepted with sign");
}
static void validationAndLimits() {
    const std::vector<N> valid{N::constant(1), N::constant(2), N::binary(K::Add)};
    Expression exact(valid, 0, {3, 2, 2});
    check(exact.nodeCount() == 3 && exact.depth() == 2 && scalar(exact) == 3, "node/depth/constant bounds inclusive");
    throws<std::length_error>([&] { Expression beyond(valid, 0, {2, 2, 2}); });
    throws<std::length_error>([&] { Expression beyond(valid, 0, {3, 1, 2}); });
    throws<std::invalid_argument>([&] { Expression beyond(valid, 0, {3, 2, 1}); });
    throws<std::invalid_argument>([&] { Expression::constant(0, 0, {0, 1, 1}); });
    throws<std::invalid_argument>([&] { Expression::constant(0, 0, {1, 0, 1}); });
    for (double bad : {-1., std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        throws<std::invalid_argument>([&] { Expression::constant(0, 0, {1, 1, bad}); });
    check(scalar(Expression::constant(-0., 0, {1, 1, 0})) == 0, "zero constant bound allowed");
    throws<std::invalid_argument>([] { Expression::constant(std::numeric_limits<double>::denorm_min(), 0, {1, 1, 0}); });
    throws<std::invalid_argument>([] { Expression empty({}, 0); });
    for (const std::vector<N>& malformed : {std::vector<N>{N::binary(K::Add)},
         {N::constant(1), N::binary(K::Multiply)}, {N::constant(1), N::constant(2)},
         {N::constant(1), N::constant(2), N::binary(K::Add), N::constant(3)}})
        throws<std::invalid_argument>([&] { Expression e(malformed, 0); });
    const std::vector<N> badVariable{N::variable(1)};
    throws<std::invalid_argument>([&] { Expression e(badVariable, 1); });
    const auto one = Expression::constant(1);
    check(Expression::binary(K::Add, one, one, {3, 2}).nodeCount() == 3, "composed boundary inclusive");
    throws<std::length_error>([&] { Expression::binary(K::Add, one, one, {2, 2}); });
    throws<std::length_error>([&] { Expression::binary(K::Add, one, one, {3, 1}); });
    const auto maximum = std::numeric_limits<std::size_t>::max();
    check(Expression::binary(K::Add, one, one, {maximum, maximum}).depth() == 2, "SIZE_MAX limits do not wrap checks");
    throws<std::length_error>([&] { Expression::binary(K::Add, one, one, {1, maximum}); });
    const auto lastVariable = Expression::variable(maximum - 1, maximum);
    check(lastVariable.variableCount() == maximum, "large variable schema does not allocate by index");
    throws<std::invalid_argument>([&] { lastVariable.evaluate({}); });
}
struct CommaPunct : std::numpunct<char> { char do_decimal_point() const override { return ','; } };
static void ownershipAndExport() {
    std::vector<N> tokens{N::constant(1), N::constant(-2), N::binary(K::Multiply)};
    auto& retained = tokens[0];
    Expression e(std::move(tokens), 0);
    retained = N::constant(99);
    check(scalar(e) == -2, "retained input token cannot mutate expression");
    auto copy = e;
    auto detached = copy.nodes(); detached[0] = N::constant(5);
    check(scalar(e) == -2 && scalar(copy) == -2, "copies and inspection cannot alias mutable storage");
    const auto changed = e.withConstant(0, 3);
    check(scalar(changed) == -6 && scalar(e) == -2, "derived value leaves original intact");
    auto detachedLimits = e.limits(); detachedLimits.maxNodes = 1;
    check(e.limits().maxNodes != detachedLimits.maxNodes, "limit inspection is an independent value");
    const auto before = e.toString();
    throws<std::invalid_argument>([&] { e = e.withConstant(0, std::numeric_limits<double>::infinity()); });
    check(e.toString() == before && scalar(e) == -2, "failed replacement preserves old valid value");
    throws<std::invalid_argument>([&] { e.withConstant(2, 3); });
    throws<std::out_of_range>([&] { e.withConstant(3, 3); });
    throws<std::length_error>([&] { e = Expression::binary(K::Multiply, e, e, {3, 2}); });
    check(e.toString() == before, "failed composition preserves assignment destination");
    Expression moved(std::move(copy));
    check(scalar(moved) == -2, "move transfers a valid tree");
    throws<std::logic_error>([&] { copy.evaluate({}); });
    throws<std::logic_error>([&] { copy.toString(); });
    copy = changed;
    check(scalar(copy) == -6, "moved-from value can be reassigned");
    check(Expression::constant(.1).toString() == "0.10000000000000001", "max_digits10 spelling");
    check(Expression::constant(-2).toString() == "(-2.0)", "negative constant parentheses");
    const std::array constants{0., -0., .1, -.1, std::nextafter(1., 2.),
        std::numeric_limits<double>::denorm_min(), std::numeric_limits<double>::min(), std::numeric_limits<double>::max()};
    const auto original = std::locale();
    std::locale::global(std::locale(original, new CommaPunct));
    try {
        check(Expression::constant(.1).toString() == "0.10000000000000001", "export ignores process C++ decimal locale");
        for (double value : constants) {
            std::string spelling = Expression::constant(value).toString();
            if (spelling.front() == '(') spelling = spelling.substr(1, spelling.size() - 2);
            double recovered = 42;
            const auto parsed = std::from_chars(spelling.data(), spelling.data() + spelling.size(), recovered);
            check(parsed.ec == std::errc{} && parsed.ptr == spelling.data() + spelling.size(), "constant spelling independently readable");
            check(std::bit_cast<std::uint64_t>(value) == std::bit_cast<std::uint64_t>(recovered), "constant export preserves exact binary64 bits");
        }
    } catch (...) { std::locale::global(original); throw; }
    std::locale::global(original);
}
static void deepIterativeTree() {
    constexpr std::size_t levels = 20000;
    const ExpressionLimits limits{2 * levels + 1, levels + 1};
    std::vector<N> left{N::constant(1)};
    for (std::size_t i = 0; i < levels; ++i) { left.push_back(N::constant(1)); left.push_back(N::binary(K::Add)); }
    const Expression e(left, 0, limits);
    check(e.depth() == levels + 1 && e.nodeCount() == 2 * levels + 1, "deep iterative construction");
    check(scalar(e) == levels + 1, "deep iterative evaluation");
    const auto text = e.toString();
    check(text.size() == 8 * levels + 3, "deep iterative export length");
    throws<std::length_error>([&] { Expression beyond(left, 0, {limits.maxNodes, levels}); });
    std::vector<N> right(levels + 1, N::constant(1));
    for (std::size_t i = 0; i < levels; ++i) right.push_back(N::binary(K::Add));
    const Expression other(right, 0, limits);
    check(scalar(other) == levels + 1 && other.depth() == levels + 1, "right-deep evaluation operand stack");
    check(other.toString().size() == text.size(), "right-deep iterative export");
    const auto copied = other;
    check(copied.nodeCount() == other.nodeCount(), "deep iterative copy/destruction");
}
int main() {
    try {
        leavesAndBindings(); arithmetic(); numericalOutcomes(); validationAndLimits(); ownershipAndExport(); deepIterativeTree();
        std::cout << "Expression construction, arithmetic, ownership, export and depth contracts passed\n";
    } catch (const std::exception& e) { std::cerr << "FAILED: " << e.what() << '\n'; return 1; }
}
