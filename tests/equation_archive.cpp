#include <EquationArchive.hpp>
#include <stdexcept>
#include <string>

int main() {
    using namespace genetik;
    EquationArchive a({3});
    a.insert({4.0, 8, true, 1, 17, "x^2+1", "x² + 1", "x real"});
    a.insert({2.0, 12, true, 2, 34, "sin(x)+x", "sin(x) + x", ""});
    a.insert({3.0, 5, true, 0, 0, "x", "x", "x real"});
    a.insert({5.0, 10, true, 0, 0, "bad", "bad", ""});
    a.insert({0.0, 1, false, 0, 0, "invalid", "invalid", ""});
    if (a.records().size() != 2 || a.records()[0].canonicalExpression != "sin(x)+x" || a.records()[1].canonicalExpression != "x")
        throw std::runtime_error("unexpected deterministic Pareto frontier");
    auto json = a.toJsonLines();
    if (json.find("\"training_loss\":3") == std::string::npos || json.find("\"constraints\":\"x real\"") == std::string::npos)
        throw std::runtime_error("machine-readable archive fields missing");
    auto text = a.toText();
    if (text.find("sin(x) + x") == std::string::npos) throw std::runtime_error("human-readable expression missing");
    EquationArchive replay({3});
    replay.insert({3.0, 5, true, 0, 0, "x", "x", "x real"});
    replay.insert({2.0, 12, true, 2, 34, "sin(x)+x", "sin(x) + x", ""});
    replay.insert({4.0, 8, true, 1, 17, "x^2+1", "x² + 1", "x real"});
    if (replay.toJsonLines() != json) throw std::runtime_error("archive output depends on insertion order");
    EquationArchive b({2});
    b.insert({1, 2, true, 0, 0, "z", "z", ""});
    b.insert({2, 1, true, 0, 0, "a", "a", ""});
    b.insert({3, 3, true, 0, 0, "m", "m", ""});
    if (b.records().size() != 2 || b.records()[0].canonicalExpression != "z" || b.records()[1].canonicalExpression != "a")
        throw std::runtime_error("bounded archive order/capacity failed");
    bool bounded = false;
    try { EquationArchive tiny({2, 8, 8}); tiny.insert({1, 1, true, 0, 0, "too-long", "too-long", ""}); }
    catch (const std::length_error&) { bounded = true; }
    if (!bounded) throw std::runtime_error("archive per-record text limit missing");
}
