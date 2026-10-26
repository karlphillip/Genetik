#pragma once
#include "ExpressionDerivative.hpp"
#include "ScientificWork.hpp"

struct SecondPartialValue {
  double primal = 0, firstI = 0, firstJ = 0, second = 0;
};
struct SecondPartialResult {
  ScientificStatus status = ScientificStatus::Unavailable;
  std::optional<double> primal;
  std::optional<SecondPartialValue> derivatives;
  ScientificWork work;
  std::string reason;
};
// Hyperdual propagation: d_i,d_j,d_i d_j. No finite differences or
// reassociation.
inline SecondPartialResult
evaluateSecondPartial(const Expression &e, std::span<const double> x,
                      std::size_t i, std::size_t j,
                      ScientificLimits limits = {},
                      const std::atomic<bool> *cancelled = nullptr) {
  if (x.size() != e.variableCount() || i >= x.size() || j >= x.size())
    throw std::invalid_argument("second partial schema/target");
  // Binding validation is charged below, before any interpreter allocation.
  genetik_science::Budget b(limits, cancelled);
  SecondPartialResult result;
  struct Jet {
    double v, di, dj, dij;
    bool active;
  };
  try {
    b.storage(e.nodeCount(), sizeof(Jet) + sizeof(ExpressionNode));
    if (x.size() > limits.maxElements)
      throw genetik_science::Stop{ScientificStatus::ResourceLimit,
                                  "binding count"};
    b.charge(x.size());
    genetik_science::inputs(x);
    // Complete primal first. Invalid later children outrank derivative
    // unavailability.
    if (e.arithmeticCost() > limits.maxOperations)
      throw genetik_science::Stop{ScientificStatus::ResourceLimit,
                                  "primal admission"};
    auto p = e.evaluate(x);
    b.charge(p.arithmeticOperations());
    if (!p.valid()) {
      result.status = ScientificStatus::Invalid;
      result.reason = "invalid primal";
      result.work = b.work;
      return result;
    }
    result.primal = p.value();
    auto nodes = e.nodes();
    std::vector<Jet> stack;
    stack.reserve(nodes.size());
    auto unary = [&](Jet a, double v, double slope, double curvature) {
      return Jet{
          v, b.mul(slope, a.di), b.mul(slope, a.dj),
          b.add(b.mul(curvature, b.mul(a.di, a.dj)), b.mul(slope, a.dij)),
          a.active};
    };
    auto product = [&](Jet a, Jet c) {
      return Jet{b.mul(a.v, c.v), b.add(b.mul(a.di, c.v), b.mul(a.v, c.di)),
                 b.add(b.mul(a.dj, c.v), b.mul(a.v, c.dj)),
                 b.add(b.add(b.mul(a.dij, c.v), b.mul(a.di, c.dj)),
                       b.add(b.mul(a.dj, c.di), b.mul(a.v, c.dij))),
                 a.active || c.active};
    };
    for (auto n : nodes) {
      b.charge();
      using K = ExpressionKind;
      auto k = n.kind();
      if (k == K::Variable) {
        auto t = n.variableIndex();
        stack.push_back(
            {x[t], t == i ? 1. : 0., t == j ? 1. : 0., 0, t == i || t == j});
        continue;
      }
      if (k == K::Constant) {
        stack.push_back({n.constantValue(), 0, 0, 0, false});
        continue;
      }
      auto c = stack.back();
      stack.pop_back();
      if (expressionArity(k) == 2) {
        auto a = stack.back();
        stack.pop_back();
        if (!a.active && !c.active) {
          auto value =
              genetik_detail::scalarBinary(k, a.v, c.v, [&] { b.charge(); });
          if (!value)
            throw genetik_science::Stop{ScientificStatus::Invalid,
                                        "inactive binary primal"};
          stack.push_back({*value, 0, 0, 0, false});
          continue;
        }
        if (k == K::Add || k == K::Subtract) {
          auto op = [&](double u, double v) {
            return k == K::Add ? b.add(u, v) : b.sub(u, v);
          };
          stack.push_back({op(a.v, c.v), op(a.di, c.di), op(a.dj, c.dj),
                           op(a.dij, c.dij), a.active || c.active});
        } else if (k == K::Multiply)
          stack.push_back(product(a, c));
        else if (k == K::Divide) {
          if (!c.active) {
            stack.push_back({b.div(a.v, c.v), b.div(a.di, c.v),
                             b.div(a.dj, c.v), b.div(a.dij, c.v), a.active});
            continue;
          }
          double inv = b.div(1, c.v);
          auto reciprocal = unary(c, inv, -b.mul(inv, inv),
                                  b.mul(2, b.mul(b.mul(inv, inv), inv)));
          stack.push_back(product(a, reciprocal));
        } else if (k == K::Min || k == K::Max) {
          if (a.v == c.v && (a.active || c.active))
            throw genetik_science::Stop{ScientificStatus::Unavailable,
                                        "second partial at branch tie"};
          stack.push_back((k == K::Min ? a.v < c.v : a.v > c.v) ? a : c);
        } else
          throw genetik_science::Stop{ScientificStatus::Unavailable,
                                      "second partial binary rule unsupported"};
      } else if (expressionArity(k) == 3) {
        auto middle = stack.back();
        stack.pop_back();
        auto left = stack.back();
        stack.pop_back();
        if (left.active || middle.active || c.active)
          throw genetik_science::Stop{
              ScientificStatus::Unavailable,
              "second partial ternary rule unsupported"};
        auto value = genetik_detail::scalarTernary(k, left.v, middle.v, c.v,
                                                   [&] { b.charge(); });
        if (!value)
          throw genetik_science::Stop{ScientificStatus::Invalid,
                                      "inactive ternary primal"};
        stack.push_back({*value, 0, 0, 0, false});
      } else {
        auto v = genetik_detail::scalarUnary(
            k, c.v, expressionPowerMetadata(k) ? n.exponent() : 0,
            [&] { b.charge(); });
        if (!v)
          throw genetik_science::Stop{ScientificStatus::Invalid,
                                      "invalid local primal"};
        if (!c.active) {
          stack.push_back({*v, 0, 0, 0, false});
          continue;
        }
        double s = 0, h = 0;
        b.charge();
        switch (k) {
        case K::Sin:
          s = b.finite(std::cos(c.v));
          h = -*v;
          break;
        case K::Cos:
          s = -b.finite(std::sin(c.v));
          h = -*v;
          break;
        case K::Exp:
        case K::Expm1:
          s = k == K::Exp ? *v : b.finite(std::exp(c.v));
          h = s;
          break;
        case K::Log:
        case K::Log1p:
        case K::Log2:
        case K::Log10: {
          double a = k == K::Log1p ? b.add(1, c.v) : c.v;
          double scale = k == K::Log2    ? std::log(2.)
                         : k == K::Log10 ? std::log(10.)
                                         : 1.;
          s = b.div(1, b.mul(a, scale));
          h = -b.div(s, a);
          break;
        }
        case K::Powi:
        case K::NegativePowi: {
          int power = k == K::Powi ? static_cast<int>(n.exponent())
                                   : -static_cast<int>(n.exponent());
          if (!power) {
            s = h = 0;
            break;
          }
          if (c.v == 0) {
            s = power == 1 ? 1 : 0;
            h = power == 2 ? 2 : 0;
          } else {
            s = b.div(b.mul(power, *v), c.v);
            h = b.div(b.mul(power - 1, s), c.v);
          }
          break;
        }
        case K::Sqrt:
          if (c.v == 0)
            throw genetik_science::Stop{ScientificStatus::Unavailable,
                                        "active sqrt singularity"};
          s = b.div(.5, *v);
          h = -b.div(s, b.mul(2, c.v));
          break;
        case K::Sinh:
          s = b.finite(std::cosh(c.v));
          h = *v;
          break;
        case K::Cosh:
          s = b.finite(std::sinh(c.v));
          h = *v;
          break;
        case K::Tanh:
          s = b.mul(b.sub(1, *v), b.add(1, *v));
          h = -b.mul(2, b.mul(*v, s));
          break;
        case K::Abs:
          if (c.v == 0)
            throw genetik_science::Stop{ScientificStatus::Unavailable,
                                        "active abs corner"};
          s = c.v < 0 ? -1 : 1;
          h = 0;
          break;
        default:
          throw genetik_science::Stop{ScientificStatus::Unavailable,
                                      "second partial unary rule unsupported"};
        }
        stack.push_back(unary(c, *v, s, h));
      }
    }
    auto q = stack.back();
    result.derivatives = SecondPartialValue{*result.primal, q.di, q.dj, q.dij};
    result.status = ScientificStatus::Success;
  } catch (const genetik_science::Stop &s) {
    result.status =
        result.primal && s.status == ScientificStatus::NumericalFailure
            ? ScientificStatus::Unavailable
            : s.status;
    result.reason = s.reason;
  }
  result.work = b.work;
  return result;
}
