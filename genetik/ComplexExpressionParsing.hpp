#pragma once
#include "ComplexExpression.hpp"
#include <cctype>

// Canonical prefix grammar only. Parser recursion <=64; expression evaluator is
// iterative.
inline ComplexExpression parseComplexExpression(std::string_view text,
                                                std::size_t variables,
                                                ExpressionLimits limits = {}) {
  if (text.size() > 1048576)
    throw ScientificResourceExhausted("complex text bytes");
  std::size_t pos = 0;
  std::vector<ComplexNode> nodes;
  auto whitespace = [&] {
    while (pos < text.size() &&
           std::isspace(static_cast<unsigned char>(text[pos])))
      ++pos;
  };
  auto token = [&](char c) {
    whitespace();
    if (pos == text.size() || text[pos] != c)
      throw std::invalid_argument("complex parse delimiter");
    ++pos;
  };
  auto real = [&] {
    whitespace();
    double value;
    auto r = std::from_chars(text.data() + pos, text.data() + text.size(),
                             value, std::chars_format::general);
    if (r.ec != std::errc{} || r.ptr == text.data() + pos ||
        !std::isfinite(value))
      throw std::invalid_argument("complex parse finite coefficient");
    pos = static_cast<std::size_t>(r.ptr - text.data());
    return value;
  };
  auto integer = [&] {
    whitespace();
    std::size_t value;
    auto r =
        std::from_chars(text.data() + pos, text.data() + text.size(), value);
    if (r.ec != std::errc{} || r.ptr == text.data() + pos)
      throw std::invalid_argument("complex parse integer");
    pos = static_cast<std::size_t>(r.ptr - text.data());
    return value;
  };
  auto append = [&](ComplexNode n) {
    if (nodes.size() == limits.maxNodes)
      throw ScientificResourceExhausted("complex parser nodes");
    nodes.push_back(n);
  };
  std::function<void(unsigned)> expression = [&](unsigned depth) {
    if (depth > 64 || depth > limits.maxDepth)
      throw ScientificResourceExhausted("complex parser depth");
    whitespace();
    if (pos == text.size())
      throw std::invalid_argument("complex parse EOF");
    if (text[pos] == 'x') {
      ++pos;
      append(ComplexNode::variable(integer()));
      return;
    }
    auto start = pos;
    while (pos < text.size() &&
           (std::isalpha(static_cast<unsigned char>(text[pos])) ||
            text[pos] == '_'))
      ++pos;
    auto name = text.substr(start, pos - start);
    token('(');
    if (name == "c") {
      double re = real();
      token(',');
      double im = real();
      token(')');
      append(ComplexNode::constant({re, im}));
      return;
    }
    constexpr std::array<std::string_view, 12> names{
        "add", "sub", "mul",  "div", "conj", "magnitude",
        "exp", "log", "sqrt", "sin", "cos",  "powi"};
    auto found = std::find(names.begin(), names.end(), name);
    if (found == names.end())
      throw std::invalid_argument("unknown complex operator");
    auto k = static_cast<ComplexKind>(2 + std::distance(names.begin(), found));
    expression(depth + 1);
    unsigned exponent = 0;
    if (complexArity(k) == 2) {
      token(',');
      expression(depth + 1);
    } else if (k == ComplexKind::Powi) {
      token(',');
      auto n = integer();
      if (n > 8)
        throw std::invalid_argument("complex power exponent");
      exponent = static_cast<unsigned>(n);
    }
    token(')');
    append(ComplexNode::operation(k, exponent));
  };
  expression(1);
  whitespace();
  if (pos != text.size())
    throw std::invalid_argument("complex trailing text");
  return {nodes, variables, limits};
}
