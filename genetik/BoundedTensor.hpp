#pragma once
#include "RunCheckpoint.hpp"
#include "ScientificWork.hpp"

class TensorShape {
public:
  explicit TensorShape(std::span<const std::size_t> dimensions,
                       ScientificLimits limits = {}) {
    genetik_science::Budget b(limits);
    if (dimensions.size() > 3)
      throw std::invalid_argument("tensor rank exceeds three");
    std::size_t n = 1;
    for (auto d : dimensions) {
      if (!d)
        throw std::invalid_argument("zero tensor extent");
      if (d > limits.maxElements / n)
        throw ScientificResourceExhausted("tensor shape element limit");
      n *= d;
    }
    if (n > limits.maxWorkspaceBytes / sizeof(double))
      throw ScientificResourceExhausted("tensor bytes");
    dimensions_.assign(dimensions.begin(), dimensions.end());
    count_ = n;
  }
  std::size_t rank() const noexcept { return dimensions_.size(); }
  std::size_t size() const noexcept { return count_; }
  const auto &dimensions() const noexcept { return dimensions_; }
  bool operator==(const TensorShape &) const = default;
  std::size_t index(std::span<const std::size_t> indices) const {
    if (indices.size() != rank())
      throw std::invalid_argument("tensor index rank");
    std::size_t n = 0;
    for (std::size_t k = 0; k < rank(); ++k) {
      if (indices[k] >= dimensions_[k])
        throw std::out_of_range("tensor index");
      n = n * dimensions_[k] + indices[k];
    }
    return n;
  }
  std::vector<std::size_t> coordinates(std::size_t n) const {
    if (n >= count_)
      throw std::out_of_range("tensor flat index");
    std::vector<std::size_t> result(rank());
    for (std::size_t k = rank(); k-- > 0;) {
      result[k] = n % dimensions_[k];
      n /= dimensions_[k];
    }
    return result;
  }

private:
  std::vector<std::size_t> dimensions_;
  std::size_t count_;
};
class BoundedTensor {
public:
  BoundedTensor(const TensorShape &shape, std::span<const double> values,
                ScientificLimits limits = {})
      : shape_(shape) {
    TensorShape checked(shape.dimensions(), limits);
    if (values.size() != shape.size())
      throw std::invalid_argument("tensor value count");
    genetik_science::inputs(values);
    data_.assign(values.begin(), values.end());
  }
  const TensorShape &shape() const noexcept { return shape_; }
  std::span<const double> values() const noexcept { return data_; }
  double at(std::span<const std::size_t> indices) const {
    return data_.at(shape_.index(indices));
  }

private:
  TensorShape shape_;
  std::vector<double> data_;
};
inline ScientificResult<BoundedTensor>
tensorAdd(const BoundedTensor &a, const BoundedTensor &c, bool subtract = false,
          ScientificLimits limits = {},
          const std::atomic<bool> *cancelled = nullptr) {
  if (a.shape() != c.shape())
    throw std::invalid_argument("tensor addition shapes");
  return scientificCalculation<BoundedTensor>(limits, cancelled, [&](auto &b) {
    b.storage(a.shape().size(), 3 * sizeof(double));
    std::vector<double> v;
    v.reserve(a.shape().size());
    for (std::size_t i = 0; i < a.shape().size(); ++i)
      v.push_back(subtract ? b.sub(a.values()[i], c.values()[i])
                           : b.add(a.values()[i], c.values()[i]));
    return BoundedTensor(a.shape(), v, limits);
  });
}
inline ScientificResult<BoundedTensor>
tensorScale(const BoundedTensor &a, double factor, ScientificLimits limits = {},
            const std::atomic<bool> *cancelled = nullptr) {
  if (!std::isfinite(factor))
    throw std::invalid_argument("tensor scale finite");
  return scientificCalculation<BoundedTensor>(limits, cancelled, [&](auto &b) {
    b.storage(a.shape().size(), 2 * sizeof(double));
    std::vector<double> v;
    v.reserve(a.shape().size());
    for (auto x : a.values())
      v.push_back(b.mul(x, factor));
    return BoundedTensor(a.shape(), v, limits);
  });
}
// One explicit contracted axis in each operand; surviving axes of A then B.
// With no axes, this is an outer product. No implicit Einstein summation.
inline ScientificResult<BoundedTensor>
tensorContract(const BoundedTensor &a, const BoundedTensor &c,
               std::optional<std::size_t> axisA,
               std::optional<std::size_t> axisB, ScientificLimits limits = {},
               const std::atomic<bool> *cancelled = nullptr) {
  if (axisA.has_value() != axisB.has_value())
    throw std::invalid_argument("both contraction axes required");
  if (axisA &&
      (*axisA >= a.shape().rank() || *axisB >= c.shape().rank() ||
       a.shape().dimensions()[*axisA] != c.shape().dimensions()[*axisB]))
    throw std::invalid_argument("contraction extent");
  std::vector<std::size_t> dims;
  for (std::size_t i = 0; i < a.shape().rank(); ++i)
    if (!axisA || i != *axisA)
      dims.push_back(a.shape().dimensions()[i]);
  for (std::size_t i = 0; i < c.shape().rank(); ++i)
    if (!axisB || i != *axisB)
      dims.push_back(c.shape().dimensions()[i]);
  TensorShape shape(dims, limits);
  return scientificCalculation<BoundedTensor>(limits, cancelled, [&](auto &b) {
    b.storage(
        genetik_science::checkedCount(
            genetik_science::checkedCount(32, shape.size()),
            genetik_science::checkedCount(a.shape().size(), c.shape().size())),
        2 * sizeof(double));
    std::vector<double> v;
    v.reserve(shape.size());
    for (std::size_t i = 0; i < shape.size(); ++i) {
      auto out = shape.coordinates(i);
      std::vector<std::size_t> ai(a.shape().rank()), ci(c.shape().rank());
      std::size_t p = 0;
      for (std::size_t k = 0; k < ai.size(); ++k)
        if (!axisA || k != *axisA)
          ai[k] = out[p++];
      for (std::size_t k = 0; k < ci.size(); ++k)
        if (!axisB || k != *axisB)
          ci[k] = out[p++];
      double sum = 0;
      auto count = axisA ? a.shape().dimensions()[*axisA] : 1;
      for (std::size_t k = 0; k < count; ++k) {
        if (axisA) {
          ai[*axisA] = ci[*axisB] = k;
        }
        auto term = b.mul(a.at(ai), c.at(ci));
        sum = k ? b.add(sum, term) : term;
      }
      v.push_back(sum);
    }
    return BoundedTensor(shape, v, limits);
  });
}
inline ScientificResult<BoundedTensor>
tensorReduce(const BoundedTensor &a, std::size_t axis,
             ScientificLimits limits = {},
             const std::atomic<bool> *cancelled = nullptr) {
  if (axis >= a.shape().rank())
    throw std::invalid_argument("reduction axis");
  auto dims = a.shape().dimensions();
  auto count = dims[axis];
  dims.erase(dims.begin() + axis);
  TensorShape shape(dims, limits);
  return scientificCalculation<BoundedTensor>(limits, cancelled, [&](auto &b) {
    b.storage(
        genetik_science::checkedCount(
            a.shape().size(), genetik_science::checkedCount(32, shape.size())),
        2 * sizeof(double));
    std::vector<double> v;
    v.reserve(shape.size());
    for (std::size_t i = 0; i < shape.size(); ++i) {
      auto indices = shape.coordinates(i);
      indices.insert(indices.begin() + axis, 0);
      double s = 0;
      for (std::size_t k = 0; k < count; ++k) {
        indices[axis] = k;
        s = k ? b.add(s, a.at(indices)) : a.at(indices);
        b.charge();
      }
      v.push_back(s);
    }
    return BoundedTensor(shape, v, limits);
  });
}
inline CheckpointDocument checkpointTensor(const BoundedTensor &a) {
  CheckpointWriter w;
  w.integer(a.shape().rank());
  for (auto d : a.shape().dimensions())
    w.integer(d);
  for (auto v : a.values())
    w.real(v);
  CheckpointDocument d;
  d.codec = "genetik.bounded-tensor/1";
  d.identity = "rank<=3;binary64;row-major";
  d.payload = w.data();
  return d;
}
inline BoundedTensor restoreTensor(const CheckpointDocument &d,
                                   ScientificLimits limits = {}) {
  if (d.codec != "genetik.bounded-tensor/1" ||
      d.identity != "rank<=3;binary64;row-major")
    throw CheckpointError("tensor codec");
  CheckpointReader r(d.payload);
  std::vector<std::size_t> dims;
  auto rank = r.count(3);
  for (std::size_t i = 0; i < rank; ++i)
    dims.push_back(r.count(limits.maxElements));
  TensorShape shape(dims, limits);
  std::vector<double> v;
  v.reserve(shape.size());
  for (std::size_t i = 0; i < shape.size(); ++i)
    v.push_back(r.real());
  r.finish();
  return {shape, v, limits};
}
