#pragma once
#include "ExpressionFormatting.hpp"
#include "ScalarExpressionCheckpoint.hpp"
#include "ScientificWork.hpp"
#include <complex>

enum class ComplexKind {
  Variable,
  Constant,
  Add,
  Subtract,
  Multiply,
  Divide,
  Conjugate,
  Magnitude,
  Exp,
  Log,
  Sqrt,
  Sin,
  Cos,
  Powi
};
struct ComplexNode {
  ComplexKind kind;
  std::size_t index = 0;
  std::complex<double> value{};
  static ComplexNode variable(std::size_t i) {
    return {ComplexKind::Variable, i, {}};
  }
  static ComplexNode constant(std::complex<double> v) {
    return {ComplexKind::Constant, 0, v};
  }
  static ComplexNode operation(ComplexKind k, unsigned exponent = 0) {
    return {k, exponent, {}};
  }
};
inline unsigned complexArity(ComplexKind k) {
  auto i = static_cast<unsigned>(k);
  if (i > 13)
    throw std::invalid_argument("complex operator ID");
  return i < 2 ? 0 : i < 6 ? 2 : 1;
}
inline bool finiteComplex(std::complex<double> z) {
  return std::isfinite(z.real()) && std::isfinite(z.imag());
}
inline std::string complexCoefficientText(double x) {
  char b[128];
  auto r = std::to_chars(b, b + 128, x, std::chars_format::general, 17);
  if (r.ec != std::errc{})
    throw std::runtime_error("complex coefficient export");
  return {b, r.ptr};
}
class ComplexExpression {
public:
  ComplexExpression(std::span<const ComplexNode> nodes, std::size_t variables,
                    ExpressionLimits limits = {})
      : variables_(variables), limits_(limits) {
    if (!limits.maxNodes || !limits.maxDepth ||
        !std::isfinite(limits.maxAbsConstant) || limits.maxAbsConstant < 0 ||
        nodes.empty())
      throw std::invalid_argument("complex limits/nodes");
    if (nodes.size() > limits.maxNodes || variables > 65536)
      throw ScientificResourceExhausted("complex schema limit");
    std::vector<std::size_t> depths;
    depths.reserve(nodes.size());
    std::size_t cost = 0;
    for (auto n : nodes) {
      auto arity = complexArity(n.kind);
      if (n.kind == ComplexKind::Variable && n.index >= variables)
        throw std::invalid_argument("complex variable");
      if (n.kind == ComplexKind::Constant &&
          (!finiteComplex(n.value) ||
           std::abs(n.value.real()) > limits.maxAbsConstant ||
           std::abs(n.value.imag()) > limits.maxAbsConstant))
        throw std::invalid_argument("complex constant");
      if (n.kind == ComplexKind::Powi && n.index > 8)
        throw std::invalid_argument("complex exponent0..8");
      if (depths.size() < arity)
        throw std::invalid_argument("complex postfix arity");
      std::size_t depth = 1;
      for (unsigned i = 0; i < arity; ++i) {
        depth = std::max(depth, depths.back() + 1);
        depths.pop_back();
      }
      if (depth > limits.maxDepth)
        throw ScientificResourceExhausted("complex depth");
      depths.push_back(depth);
      auto work = n.kind == ComplexKind::Powi ? (n.index ? n.index - 1 : 0)
                  : arity                     ? 1
                                              : 0;
      if (work > limits.maxArithmeticOperations - cost)
        throw ScientificResourceExhausted("complex arithmetic bound");
      cost += work;
    }
    if (depths.size() != 1)
      throw std::invalid_argument("complex postfix root");
    nodes_.assign(nodes.begin(), nodes.end());
    depth_ = depths.back();
  }
  const auto &nodes() const noexcept { return nodes_; }
  std::size_t variableCount() const noexcept { return variables_; }
  std::size_t depth() const noexcept { return depth_; }
  ExpressionLimits limits() const noexcept { return limits_; }
  ScientificResult<std::complex<double>>
  evaluate(std::span<const std::complex<double>> inputs,
           ScientificLimits limits = {},
           const std::atomic<bool> *cancelled = nullptr) const {
    if (inputs.size() != variables_)
      throw std::invalid_argument("complex binding count");
    for (auto z : inputs)
      if (!finiteComplex(z))
        throw std::invalid_argument("complex input nonfinite");
    return scientificCalculation<std::complex<double>>(
        limits, cancelled, [&](auto &b) {
          b.storage(nodes_.size(), sizeof(std::complex<double>));
          if (inputs.size() > limits.maxElements)
            throw genetik_science::Stop{ScientificStatus::ResourceLimit,
                                        "complex bindings"};
          b.charge(inputs.size());
          std::vector<std::complex<double>> stack;
          stack.reserve(nodes_.size());
          for (auto n : nodes_) {
            b.charge();
            if (n.kind == ComplexKind::Variable) {
              stack.push_back(inputs[n.index]);
              continue;
            }
            if (n.kind == ComplexKind::Constant) {
              stack.push_back(n.value);
              continue;
            }
            auto right = stack.back();
            stack.pop_back();
            std::complex<double> v;
            if (complexArity(n.kind) == 2) {
              auto left = stack.back();
              stack.pop_back();
              b.charge();
              switch (n.kind) {
              case ComplexKind::Add:
                v = left + right;
                break;
              case ComplexKind::Subtract:
                v = left - right;
                break;
              case ComplexKind::Multiply:
                v = left * right;
                break;
              case ComplexKind::Divide:
                if (right == std::complex<double>{})
                  throw genetik_science::Stop{ScientificStatus::Invalid,
                                              "complex division by zero"};
                v = left / right;
                break;
              default:
                throw std::logic_error("complex binary dispatch");
              }
            } else {
              b.charge();
              switch (n.kind) {
              case ComplexKind::Conjugate:
                v = std::conj(right);
                break;
              case ComplexKind::Magnitude:
                v = {std::abs(right), 0};
                break;
              case ComplexKind::Exp:
                v = std::exp(right);
                break;
              case ComplexKind::Log:
                if (right == std::complex<double>{})
                  throw genetik_science::Stop{ScientificStatus::Invalid,
                                              "complex logarithm zero"};
                v = std::log(right);
                break;
              case ComplexKind::Sqrt:
                v = std::sqrt(right);
                break;
              case ComplexKind::Sin:
                v = std::sin(right);
                break;
              case ComplexKind::Cos:
                v = std::cos(right);
                break;
              case ComplexKind::Powi:
                v = n.index ? right : std::complex<double>{1, 0};
                for (std::size_t i = 1; i < n.index; ++i) {
                  b.charge();
                  v *= right;
                  if (!finiteComplex(v))
                    throw genetik_science::Stop{
                        ScientificStatus::NumericalFailure,
                        "complex power overflow"};
                }
                break;
              default:
                throw std::logic_error("complex unary dispatch");
              }
            }
            if (!finiteComplex(v))
              throw genetik_science::Stop{ScientificStatus::NumericalFailure,
                                          "nonfinite complex arithmetic"};
            stack.push_back(v);
          }
          return stack.back();
        });
  }
  std::string
  format(ExpressionTextFormat format = ExpressionTextFormat::Canonical,
         std::size_t maxBytes = 1048576) const {
    if (format != ExpressionTextFormat::Canonical &&
        format != ExpressionTextFormat::Unicode &&
        format != ExpressionTextFormat::LaTeX)
      throw std::invalid_argument("complex format");
    std::vector<std::string> stack;
    std::size_t payload = 0;
    static constexpr std::array names{"x",    "c",    "add",       "sub", "mul",
                                      "div",  "conj", "magnitude", "exp", "log",
                                      "sqrt", "sin",  "cos",       "powi"};
    for (auto n : nodes_) {
      auto k = static_cast<std::size_t>(n.kind);
      std::string text;
      if (k == 0)
        text = format == ExpressionTextFormat::LaTeX
                   ? "x_{" + std::to_string(n.index) + "}"
                   : "x" + std::to_string(n.index);
      else if (k == 1) {
        auto real = complexCoefficientText(n.value.real()),
             imag = complexCoefficientText(n.value.imag());
        text = format == ExpressionTextFormat::Canonical
                   ? "c(" + real + "," + imag + ")"
               : format == ExpressionTextFormat::LaTeX
                   ? "\\left(" + real + "+(" + imag + ")\\,\\mathrm{i}\\right)"
                   : "(" + real + "+(" + imag + ")i)";
      } else {
        auto right = std::move(stack.back());
        payload -= right.size();
        stack.pop_back();
        if (k < 6) {
          auto left = std::move(stack.back());
          payload -= left.size();
          stack.pop_back();
          text = format == ExpressionTextFormat::Canonical
                     ? std::string(names[k]) + "(" + left + "," + right + ")"
                 : k == 5 && format == ExpressionTextFormat::LaTeX
                     ? "\\frac{" + left + "}{" + right + "}"
                     : "(" + left +
                           (k == 2   ? " + "
                            : k == 3 ? " - "
                            : k == 4 ? (format == ExpressionTextFormat::LaTeX
                                            ? " \\cdot "
                                            : " · ")
                                     : " / ") +
                           right + ")";
        } else if (format == ExpressionTextFormat::LaTeX) {
          if (k == 6)
            text = "\\overline{" + right + "}";
          else if (k == 7)
            text = "\\left|" + right + "\\right|";
          else if (k == 10)
            text = "\\sqrt{" + right + "}";
          else if (k == 13)
            text = "\\left(" + right + "\\right)^{" + std::to_string(n.index) +
                   "}";
          else
            text = "\\" + std::string(k == 9 ? "ln" : names[k]) + "\\left(" +
                   right + "\\right)";
        } else
          text = std::string(names[k]) + "(" + right +
                 (k == 13 ? "," + std::to_string(n.index) : "") + ")";
      }
      if (text.size() > maxBytes || payload > maxBytes - text.size())
        throw ScientificResourceExhausted("complex format bytes");
      payload += text.size();
      stack.push_back(std::move(text));
    }
    return stack.back();
  }
  // An ordinary scalar AST is used only as an explicitly complex-interpreted
  // topology/real-coefficient genome. Its ordinary scalar value is never
  // scored.
  static ComplexExpression fromScalarGenome(const Expression &e) {
    std::vector<ComplexNode> nodes;
    for (auto n : e.nodes()) {
      using S = ExpressionKind;
      ComplexKind k;
      switch (n.kind()) {
      case S::Variable:
        nodes.push_back(ComplexNode::variable(n.variableIndex()));
        continue;
      case S::Constant:
        nodes.push_back(ComplexNode::constant({n.constantValue(), 0}));
        continue;
      case S::Add:
        k = ComplexKind::Add;
        break;
      case S::Subtract:
        k = ComplexKind::Subtract;
        break;
      case S::Multiply:
        k = ComplexKind::Multiply;
        break;
      case S::Divide:
        k = ComplexKind::Divide;
        break;
      case S::Exp:
        k = ComplexKind::Exp;
        break;
      case S::Log:
        k = ComplexKind::Log;
        break;
      case S::Sqrt:
        k = ComplexKind::Sqrt;
        break;
      case S::Sin:
        k = ComplexKind::Sin;
        break;
      case S::Cos:
        k = ComplexKind::Cos;
        break;
      case S::Powi:
        k = ComplexKind::Powi;
        break;
      default:
        throw std::invalid_argument(
            "scalar genome operator outside complex profile");
      }
      nodes.push_back(
          ComplexNode::operation(k, n.kind() == S::Powi ? n.exponent() : 0));
    }
    return {nodes, e.variableCount(), e.limits()};
  }

private:
  std::vector<ComplexNode> nodes_;
  std::size_t variables_, depth_;
  ExpressionLimits limits_;
};
inline CheckpointDocument
checkpointComplexExpression(const ComplexExpression &e) {
  CheckpointWriter w;
  w.integer(e.variableCount());
  auto l = e.limits();
  w.integer(l.maxNodes);
  w.integer(l.maxDepth);
  w.real(l.maxAbsConstant);
  w.integer(l.maxArithmeticOperations);
  w.integer(e.nodes().size());
  for (auto n : e.nodes()) {
    w.integer(static_cast<unsigned>(n.kind));
    if (n.kind == ComplexKind::Variable || n.kind == ComplexKind::Powi)
      w.integer(n.index);
    else if (n.kind == ComplexKind::Constant) {
      w.real(n.value.real());
      w.real(n.value.imag());
    }
  }
  CheckpointDocument d;
  d.codec = "genetik.complex-expression/1";
  d.identity =
      "binary64;std-complex;principal-branches;magnitude-real-embedding";
  d.payload = w.data();
  return d;
}
inline ComplexExpression restoreComplexExpression(const CheckpointDocument &d) {
  if (d.codec != "genetik.complex-expression/1" ||
      d.identity !=
          "binary64;std-complex;principal-branches;magnitude-real-embedding")
    throw CheckpointError("complex codec/branch identity");
  CheckpointReader r(d.payload);
  auto vars = r.count(65536);
  ExpressionLimits l;
  l.maxNodes = r.count(65536);
  l.maxDepth = r.count(65536);
  l.maxAbsConstant = r.real();
  l.maxArithmeticOperations = r.integer();
  auto count = r.count(l.maxNodes);
  std::vector<ComplexNode> nodes;
  nodes.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    auto k = static_cast<ComplexKind>(r.count(13));
    if (k == ComplexKind::Variable)
      nodes.push_back(ComplexNode::variable(r.count(vars ? vars - 1 : 0)));
    else if (k == ComplexKind::Constant) {
      double real = r.real(), imaginary = r.real();
      nodes.push_back(ComplexNode::constant({real, imaginary}));
    } else
      nodes.push_back(ComplexNode::operation(
          k, k == ComplexKind::Powi ? static_cast<unsigned>(r.count(8)) : 0));
  }
  r.finish();
  return {nodes, vars, l};
}
