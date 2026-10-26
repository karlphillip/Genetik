#pragma once
#include "ScientificWork.hpp"
#include "TypedValue.hpp"

struct LinearSolveValue {
  TypedValue solution;
  double residualNorm;
};
struct SymmetricEigenValue {
  TypedValue eigenvalues, eigenvectors;
  double maximumResidual;
};
namespace genetik_science {
inline std::size_t square(const TypedValue &a) {
  if (a.shape().kind() != TypedValueKind::Matrix ||
      a.shape().rows() != a.shape().columns())
    throw std::invalid_argument("square matrix required");
  auto n = a.shape().rows();
  if (n > 16)
    throw std::invalid_argument("scientific matrices support dimensions1..16");
  return n;
}
inline std::vector<double> columnMajor(const TypedValue &a) {
  auto n = square(a);
  std::vector<double> v(n * n);
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = 0; j < n; ++j)
      v[j * n + i] = a.at(i, j);
  return v;
}
#ifdef GENETIK_HAS_LAPACK
extern "C" {
const char *openblas_get_config();
int openblas_get_num_threads();
void dgesv_(const int *, const int *, double *, const int *, int *, double *,
            const int *, int *);
void dsyev_(const char *, const char *, const int *, double *, const int *,
            double *, double *, const int *, int *);
}
inline void requireBackend() {
  auto config = openblas_get_config();
  if (!config ||
      std::string_view(config).find("USE64BITINT") != std::string_view::npos)
    throw Stop{ScientificStatus::Unavailable, "LP64 OpenBLAS required"};
  if (openblas_get_num_threads() != 1)
    throw Stop{ScientificStatus::Unavailable,
               "set OPENBLAS_NUM_THREADS=1 before launching"};
}
#endif
} // namespace genetik_science
inline ScientificResult<double>
vectorNorm(const TypedValue &a, bool squared = false,
           ScientificLimits limits = {},
           const std::atomic<bool> *cancelled = nullptr) {
  if (a.shape().kind() != TypedValueKind::Vector)
    throw std::invalid_argument("norm requires vector");
  return scientificCalculation<double>(limits, cancelled, [&](auto &b) {
    b.storage(a.shape().length(), sizeof(double));
    double norm = 0;
    for (double x : a.coefficients()) {
      b.charge();
      norm = b.finite(std::hypot(norm, x));
    }
    return squared ? b.mul(norm, norm) : norm;
  });
}
inline ScientificResult<TypedValue>
vectorNormalize(const TypedValue &a, ScientificLimits limits = {},
                const std::atomic<bool> *cancelled = nullptr) {
  if (a.shape().kind() != TypedValueKind::Vector)
    throw std::invalid_argument("normalize requires vector");
  return scientificCalculation<TypedValue>(limits, cancelled, [&](auto &b) {
    b.storage(a.shape().length(), 2 * sizeof(double));
    double norm = 0;
    for (double x : a.coefficients()) {
      b.charge();
      norm = b.finite(std::hypot(norm, x));
    }
    if (norm == 0)
      throw genetik_science::Stop{ScientificStatus::Invalid,
                                  "zero vector normalization"};
    std::vector<double> out;
    for (double x : a.coefficients())
      out.push_back(b.div(x, norm));
    return TypedValue::vector(out);
  });
}
inline ScientificResult<TypedValue>
vectorCross(const TypedValue &a, const TypedValue &c,
            ScientificLimits limits = {},
            const std::atomic<bool> *cancelled = nullptr) {
  if (a.shape() != TypedShape::vector(3) || c.shape() != a.shape())
    throw std::invalid_argument("cross requires two3-vectors");
  return scientificCalculation<TypedValue>(limits, cancelled, [&](auto &b) {
    b.storage(9, sizeof(double));
    std::array<double, 3> v;
    for (std::size_t i = 0; i < 3; ++i)
      v[i] = b.sub(b.mul(a.at((i + 1) % 3), c.at((i + 2) % 3)),
                   b.mul(a.at((i + 2) % 3), c.at((i + 1) % 3)));
    return TypedValue::vector(v);
  });
}
inline ScientificResult<double>
matrixTrace(const TypedValue &a, ScientificLimits limits = {},
            const std::atomic<bool> *cancelled = nullptr) {
  auto n = genetik_science::square(a);
  return scientificCalculation<double>(limits, cancelled, [&](auto &b) {
    b.storage(n * n, sizeof(double));
    double v = a.at(0, 0);
    for (std::size_t i = 1; i < n; ++i)
      v = b.add(v, a.at(i, i));
    return v;
  });
}
inline ScientificResult<TypedValue>
matrixPower(const TypedValue &a, unsigned exponent,
            ScientificLimits limits = {},
            const std::atomic<bool> *cancelled = nullptr) {
  auto n = genetik_science::square(a);
  if (exponent > 32)
    throw std::invalid_argument("matrix power exponent0..32");
  return scientificCalculation<TypedValue>(limits, cancelled, [&](auto &b) {
    b.storage(n * n, 3 * sizeof(double));
    std::vector<double> out(n * n), next(n * n);
    for (std::size_t i = 0; i < n; ++i)
      out[i * n + i] = 1;
    for (unsigned p = 0; p < exponent; ++p) {
      b.iteration();
      for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j) {
          double s = b.mul(out[i * n], a.at(0, j));
          for (std::size_t k = 1; k < n; ++k)
            s = b.add(s, b.mul(out[i * n + k], a.at(k, j)));
          next[i * n + j] = s;
        }
      out.swap(next);
    }
    return TypedValue::matrix(n, n, out);
  });
}
inline ScientificResult<double>
matrixDeterminant(const TypedValue &a, ScientificLimits limits = {},
                  const std::atomic<bool> *cancelled = nullptr) {
  auto n = genetik_science::square(a);
  return scientificCalculation<double>(limits, cancelled, [&](auto &b) {
    b.storage(n * n, 2 * sizeof(double));
    auto v =
        std::vector<double>(a.coefficients().begin(), a.coefficients().end());
    double determinant = 1;
    for (std::size_t k = 0; k < n; ++k) {
      b.iteration();
      std::size_t pivot = k;
      for (std::size_t i = k + 1; i < n; ++i) {
        b.charge();
        if (std::abs(v[i * n + k]) > std::abs(v[pivot * n + k]))
          pivot = i;
      }
      if (v[pivot * n + k] == 0)
        return 0.;
      if (pivot != k) {
        for (std::size_t j = 0; j < n; ++j)
          std::swap(v[k * n + j], v[pivot * n + j]);
        determinant = -determinant;
      }
      double d = v[k * n + k];
      determinant = b.mul(determinant, d);
      for (std::size_t i = k + 1; i < n; ++i) {
        double ratio = b.div(v[i * n + k], d);
        for (std::size_t j = k + 1; j < n; ++j)
          v[i * n + j] = b.sub(v[i * n + j], b.mul(ratio, v[k * n + j]));
      }
    }
    return determinant;
  });
}
inline ScientificResult<LinearSolveValue>
solveLinearSystem(const TypedValue &a, const TypedValue &rhs,
                  ScientificLimits limits = {},
                  const std::atomic<bool> *cancelled = nullptr) {
  auto n = genetik_science::square(a);
  if (rhs.shape() != TypedShape::vector(n))
    throw std::invalid_argument("linear rhs shape");
  return scientificCalculation<LinearSolveValue>(
      limits, cancelled, [&](auto &b) -> LinearSolveValue {
#ifndef GENETIK_HAS_LAPACK
        (void)b;
        throw genetik_science::Stop{ScientificStatus::Unavailable,
                                    "optional LAPACK backend not enabled"};
#else
    genetik_science::requireBackend();
    b.storage(n*n+4*n,4*sizeof(double));b.charge(4*n*n*n+4*n*n);auto data=genetik_science::columnMajor(a);std::vector<double> x(rhs.coefficients().begin(),rhs.coefficients().end());std::vector<int> pivots(n);int dim=static_cast<int>(n),nrhs=1,info=0;genetik_science::dgesv_(&dim,&nrhs,data.data(),&dim,pivots.data(),x.data(),&dim,&info);if(info>0)throw genetik_science::Stop{ScientificStatus::Invalid,"singular linear system"};if(info<0)throw std::runtime_error("LAPACK dgesv argument failure");double residual=0,scale=0;for(std::size_t i=0;i<n;++i){double dot=0,row=0;for(std::size_t j=0;j<n;++j){dot=b.add(dot,b.mul(a.at(i,j),x[j]));row=b.add(row,std::abs(b.mul(a.at(i,j),x[j])));}b.charge();residual=b.finite(std::hypot(residual,b.sub(dot,rhs.at(i))));scale=std::max(scale,b.add(row,std::abs(rhs.at(i))));}if(residual>limits.tolerance*std::max(1.,scale))throw genetik_science::Stop{ScientificStatus::Nonconvergence,"linear backward residual criterion"};return {TypedValue::vector(x),residual};
#endif
      });
}
inline ScientificResult<SymmetricEigenValue>
symmetricEigen(const TypedValue &a, ScientificLimits limits = {},
               const std::atomic<bool> *cancelled = nullptr) {
  auto n = genetik_science::square(a);
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = 0; j < i; ++j)
      if (a.at(i, j) != a.at(j, i))
        throw std::invalid_argument("exact symmetric matrix required");
  return scientificCalculation<SymmetricEigenValue>(
      limits, cancelled, [&](auto &b) -> SymmetricEigenValue {
#ifndef GENETIK_HAS_LAPACK
        (void)b;
        throw genetik_science::Stop{ScientificStatus::Unavailable,
                                    "optional LAPACK backend not enabled"};
#else
    genetik_science::requireBackend();
    b.storage(4*n*n+8*n,2*sizeof(double));b.charge(32*n*n*n);auto data=genetik_science::columnMajor(a);std::vector<double> values(n),workspace(8*n+1);int dim=static_cast<int>(n),size=static_cast<int>(workspace.size()),info=0;const char job='V',triangle='U';genetik_science::dsyev_(&job,&triangle,&dim,data.data(),&dim,values.data(),workspace.data(),&size,&info);if(info>0)throw genetik_science::Stop{ScientificStatus::Nonconvergence,"LAPACK eigen nonconvergence"};if(info<0)throw std::runtime_error("LAPACK dsyev argument failure");std::vector<double> vectors(n*n);double residual=0;for(std::size_t k=0;k<n;++k)for(std::size_t i=0;i<n;++i){vectors[i*n+k]=b.finite(data[k*n+i]);double dot=0;for(std::size_t j=0;j<n;++j)dot=b.add(dot,b.mul(a.at(i,j),data[k*n+j]));residual=std::max(residual,std::abs(b.sub(dot,b.mul(values[k],data[k*n+i]))));}double scale=1;for(double v:a.coefficients())scale=std::max(scale,std::abs(v));if(residual/scale>limits.tolerance*static_cast<double>(n))throw genetik_science::Stop{ScientificStatus::Nonconvergence,"eigen residual criterion"};return {TypedValue::vector(values),TypedValue::matrix(n,n,vectors),residual};
#endif
      });
}
// A deliberately small domain: exponential of a real symmetric matrix through
// LAPACK orthonormal eigenvectors. General nonsymmetric exp is not claimed.
inline ScientificResult<TypedValue>
symmetricMatrixExponential(const TypedValue &a, double time = 1,
                           ScientificLimits limits = {},
                           const std::atomic<bool> *cancelled = nullptr) {
  if (!std::isfinite(time))
    throw std::invalid_argument("matrix time finite");
  auto n = genetik_science::square(a);
  auto eig = symmetricEigen(a, limits, cancelled);
  if (!eig.success())
    return {eig.status, {}, eig.work, eig.reason};
  ScientificLimits remaining = limits;
  if (eig.work.operations >= limits.maxOperations)
    return {ScientificStatus::ResourceLimit,
            {},
            eig.work,
            "matrix exponential remaining work"};
  remaining.maxOperations -= eig.work.operations;
  auto result =
      scientificCalculation<TypedValue>(remaining, cancelled, [&](auto &b) {
        b.storage(4 * n * n, 2 * sizeof(double));
        std::vector<double> exponent(n), out(n * n);
        for (std::size_t k = 0; k < n; ++k) {
          double arg = b.mul(time, eig.value->eigenvalues.at(k));
          b.charge();
          exponent[k] = b.finite(std::exp(arg));
        }
        for (std::size_t i = 0; i < n; ++i)
          for (std::size_t j = 0; j < n; ++j) {
            double s = 0;
            for (std::size_t k = 0; k < n; ++k)
              s = b.add(
                  s, b.mul(b.mul(eig.value->eigenvectors.at(i, k), exponent[k]),
                           eig.value->eigenvectors.at(j, k)));
            out[i * n + j] = s;
          }
        return TypedValue::matrix(n, n, out);
      });
  result.work.operations += eig.work.operations;
  result.work.workspaceBytes =
      std::max(result.work.workspaceBytes, eig.work.workspaceBytes);
  return result;
}
