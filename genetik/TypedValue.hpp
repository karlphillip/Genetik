#pragma once
#include "Expression.hpp" // reuse the existing strict-FP/native binary64 contract
#include <charconv>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Local mathematical storage/work limits are separate from invalid mathematics.
class TypedLimitError : public std::length_error {
public:
    using std::length_error::length_error;
};
namespace genetik_typed_detail {
inline std::size_t add(std::size_t a, std::size_t b, const char* what) {
    if (b > std::numeric_limits<std::size_t>::max() - a) throw TypedLimitError(what);
    return a + b;
}
inline std::size_t multiply(std::size_t a, std::size_t b, const char* what) {
    if (a && b > std::numeric_limits<std::size_t>::max() / a) throw TypedLimitError(what);
    return a * b;
}
inline void bound(std::size_t count, std::size_t maximum, const char* what) {
    if (count > maximum) throw TypedLimitError(what);
}
inline std::string integerText(std::size_t n) {
    char buffer[std::numeric_limits<std::size_t>::digits10 + 2];
    const auto r = std::to_chars(buffer, buffer + sizeof buffer, n);
    if (r.ec != std::errc{}) throw std::runtime_error("typed integer export failed");
    return {buffer, r.ptr};
}
inline std::string coefficientText(double v) {
    char buffer[128];
    const auto r = std::to_chars(buffer, buffer + sizeof buffer, v,
                                std::chars_format::general, std::numeric_limits<double>::max_digits10);
    if (r.ec != std::errc{}) throw std::runtime_error("typed coefficient export failed");
    std::string out(buffer, r.ptr);
    if (out.find_first_of(".eE") == std::string::npos) out += ".0";
    return std::signbit(v) ? '(' + out + ')' : out;
}
}

enum class TypedValueKind { Scalar, Vector, Matrix };

// Shape identity includes kind: scalar != vector[1] != matrix[1,1].
class TypedShape {
public:
    static TypedShape scalar() { return {TypedValueKind::Scalar, 1, 1}; }
    static TypedShape vector(std::size_t length) { return {TypedValueKind::Vector, length, 1}; }
    static TypedShape matrix(std::size_t rows, std::size_t columns) { return {TypedValueKind::Matrix, rows, columns}; }
    TypedValueKind kind() const noexcept { return kind_; }
    std::size_t rows() const noexcept { return rows_; }
    std::size_t columns() const noexcept { return columns_; }
    std::size_t coefficients() const noexcept { return count_; }
    std::size_t length() const {
        if (kind_ != TypedValueKind::Vector) throw std::logic_error("length requires a vector shape");
        return rows_;
    }
    bool operator==(const TypedShape&) const = default;
    std::string toString() const {
        using genetik_typed_detail::integerText;
        if (kind_ == TypedValueKind::Scalar) return "scalar";
        if (kind_ == TypedValueKind::Vector) return "vector[" + integerText(rows_) + ']';
        return "matrix[" + integerText(rows_) + ',' + integerText(columns_) + ']';
    }
private:
    TypedShape(TypedValueKind kind, std::size_t rows, std::size_t columns)
        : kind_(kind), rows_(rows), columns_(columns), count_(0) {
        if (!rows || !columns) throw std::invalid_argument("typed dimensions must be positive");
        count_ = genetik_typed_detail::multiply(rows, columns, "typed dimension product overflow");
    }
    TypedValueKind kind_;
    std::size_t rows_, columns_, count_;
};

struct TypedValueLimits {
    std::size_t maxVectorLength = 4096;
    std::size_t maxMatrixRows = 256;
    std::size_t maxMatrixColumns = 256;
    std::size_t maxCoefficients = 65536;
};
namespace genetik_typed_detail {
inline void validate(TypedValueLimits limits) {
    if (!limits.maxVectorLength || !limits.maxMatrixRows || !limits.maxMatrixColumns || !limits.maxCoefficients)
        throw std::invalid_argument("typed value limits must be positive");
}
inline void validateShape(TypedShape shape, TypedValueLimits limits) {
    validate(limits);
    if (shape.kind() == TypedValueKind::Vector)
        bound(shape.length(), limits.maxVectorLength, "typed vector length limit");
    if (shape.kind() == TypedValueKind::Matrix) {
        bound(shape.rows(), limits.maxMatrixRows, "typed matrix row limit");
        bound(shape.columns(), limits.maxMatrixColumns, "typed matrix column limit");
    }
    bound(shape.coefficients(), limits.maxCoefficients, "typed value coefficient limit");
    bound(shape.coefficients(), std::vector<double>().max_size(), "typed storage capacity limit");
}
}

class TypedExpression;

// Owned finite coefficients. Public factories copy, including from rvalue buffers.
class TypedValue {
public:
    static TypedValue scalar(double value) {
        if (!std::isfinite(value)) throw std::invalid_argument("typed coefficients must be finite");
        return {TypedShape::scalar(), std::vector<double>{value}};
    }
    static TypedValue vector(std::span<const double> values, TypedValueLimits limits = {}) {
        return copy(TypedShape::vector(values.size()), values, limits);
    }
    static TypedValue matrix(std::size_t rows, std::size_t columns, std::span<const double> values,
                             TypedValueLimits limits = {}) {
        return copy(TypedShape::matrix(rows, columns), values, limits);
    }
    TypedValue(const TypedValue& other) : shape_(other.shape()), data_(other.data_) {}
    TypedValue(TypedValue&&) noexcept = default;
    TypedValue& operator=(TypedValue other) noexcept { swap(other); return *this; }
    void swap(TypedValue& other) noexcept { std::swap(shape_, other.shape_); data_.swap(other.data_); }
    TypedShape shape() const { requireUsable(); return shape_; }
    std::span<const double> coefficients() const { requireUsable(); return data_; }
    double scalarValue() const {
        if (shape().kind() != TypedValueKind::Scalar) throw std::logic_error("value is not scalar");
        return data_[0];
    }
    double at(std::size_t index) const {
        if (shape().kind() != TypedValueKind::Vector) throw std::logic_error("indexed value is not a vector");
        return data_.at(index);
    }
    double at(std::size_t row, std::size_t column) const {
        if (shape().kind() != TypedValueKind::Matrix) throw std::logic_error("indexed value is not a matrix");
        if (row >= shape_.rows() || column >= shape_.columns()) throw std::out_of_range("typed matrix index");
        return data_[row * shape_.columns() + column];
    }
    std::string toString() const {
        std::string out = shape().toString() + '(';
        for (std::size_t i = 0; i < data_.size(); ++i) {
            if (i) out += ',';
            out += genetik_typed_detail::coefficientText(data_[i]);
        }
        return out + ')';
    }
private:
    friend class TypedExpression;
    // Only the checked interpreter may adopt its exclusively owned verified output.
    TypedValue(TypedShape shape, std::vector<double>&& verified) : shape_(shape), data_(std::move(verified)) {}
    static TypedValue copy(TypedShape shape, std::span<const double> values, TypedValueLimits limits) {
        genetik_typed_detail::validateShape(shape, limits);
        if (values.size() != shape.coefficients()) throw std::invalid_argument("typed coefficient count differs from shape");
        for (double v : values)
            if (!std::isfinite(v)) throw std::invalid_argument("typed coefficients must be finite");
        return {shape, std::vector<double>(values.begin(), values.end())};
    }
    void requireUsable() const {
        if (data_.empty()) throw std::logic_error("moved-from typed value");
    }
    TypedShape shape_;
    std::vector<double> data_;
};
