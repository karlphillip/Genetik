#pragma once
#include "ParameterDerivatives.hpp"
#include <functional>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
#include <type_traits>
#include <vector>

enum class LMStop { GradientTolerance, StepTolerance, LossTolerance, IterationLimit,
                    EvaluationLimit, DerivativeLimit, WorkspaceLimit, SingularSystem, InvalidStart,
                    DampingLimit, Cancelled };
struct LMConfig {
    std::size_t maxIterations=100;
    std::size_t maxObjectiveEvaluations=1000;
    std::size_t maxJacobianEvaluations=500;
    std::size_t maxParameters=128;
    std::size_t maxResiduals=65536;
    std::size_t maxMathematicalOperations=100000000;
    std::size_t maxWorkspaceBytes=64*1024*1024;
    double initialDamping=1e-3;
    double gradientTolerance=1e-10, stepTolerance=1e-12, lossTolerance=0;
    double minimumDamping=1e-15, maximumDamping=1e15;
    // Off by default to preserve existing callers. When enabled, each trial
    // coordinate is projected independently onto its feasible interval.
    bool projectStepsToBounds = false;
};
struct LMWork {
    std::size_t objectiveEvaluations=0, jacobianEvaluations=0,
        forwardEvaluations=0, derivativeEvaluations=0, rowsProcessed=0,
        parameterUpdates=0, linearSolves=0, acceptedSteps=0, rejectedSteps=0,
        mathematicalOperations=0, linearAlgebraOperations=0, peakWorkspaceBytes=0;
    double seconds=0;
    bool operator==(const LMWork&) const = default;
};
struct LMResidualEvaluation {
    bool valid=false;
    std::vector<double> residuals;
    // Row-major m by p. Empty when only objective residuals are requested.
    std::vector<double> jacobian;
    std::size_t forwardEvaluations=0, derivativeEvaluations=0,
        rowsProcessed=0, mathematicalOperations=0, workspaceBytes=0;
    // Lets an evaluator report exhausted mathematical work without inventing
    // an extra operation merely to make the counter exceed its allowance.
    bool operationLimitReached = false;
    bool workspaceLimitReached = false;
};
struct LMResult {
    std::vector<double> parameters;
    std::optional<double> sumSquaredResiduals;
    LMStop stop=LMStop::InvalidStart;
    LMWork work;
};
using LMResidualFunction = std::function<LMResidualEvaluation(std::span<const double>,bool,std::size_t)>;

namespace genetik_detail {
inline void validateLevenbergMarquardtConfig(const LMConfig& c) {
    if (!c.maxIterations || !c.maxObjectiveEvaluations || !c.maxJacobianEvaluations ||
        !c.maxParameters || !c.maxResiduals || !c.maxMathematicalOperations ||
        !c.maxWorkspaceBytes || !std::isfinite(c.initialDamping) || c.initialDamping <= 0 ||
        !std::isfinite(c.minimumDamping) || !std::isfinite(c.maximumDamping) ||
        c.minimumDamping <= 0 || c.minimumDamping > c.initialDamping ||
        c.initialDamping > c.maximumDamping || !std::isfinite(c.gradientTolerance) ||
        c.gradientTolerance <= 0 || !std::isfinite(c.stepTolerance) || c.stepTolerance <= 0 ||
        !std::isfinite(c.lossTolerance) || c.lossTolerance < 0)
        throw std::invalid_argument("invalid Levenberg-Marquardt configuration");
}
inline bool solveDampedSystem(std::vector<double> a,std::vector<double> b,
                              std::vector<double>& x,std::size_t n) {
    // Partial-pivot Gaussian elimination on the small symmetric damped normal
    // system. Scaled pivot rejection prevents pretending an ill-conditioned
    // system has a trustworthy step.
    double scale=0;for(double v:a)scale=std::max(scale,std::abs(v));
    if(!std::isfinite(scale)||scale==0)return false;
    for(std::size_t k=0;k<n;++k){std::size_t pivot=k;for(std::size_t i=k+1;i<n;++i)if(std::abs(a[i*n+k])>std::abs(a[pivot*n+k]))pivot=i;
        if(std::abs(a[pivot*n+k])<=scale*64*std::numeric_limits<double>::epsilon())return false;
        if(pivot!=k){for(std::size_t j=k;j<n;++j)std::swap(a[k*n+j],a[pivot*n+j]);std::swap(b[k],b[pivot]);}
        for(std::size_t i=k+1;i<n;++i){double q=a[i*n+k]/a[k*n+k];if(!std::isfinite(q))return false;for(std::size_t j=k+1;j<n;++j)a[i*n+j]-=q*a[k*n+j];b[i]-=q*b[k];}
    }
    x.assign(n,0);for(std::size_t ii=n;ii-->0;){double v=b[ii];for(std::size_t j=ii+1;j<n;++j)v-=a[ii*n+j]*x[j];x[ii]=v/a[ii*n+ii];if(!std::isfinite(x[ii]))return false;}return true;
}
// Inspect the undamped normal matrix before LM regularization. Damping can
// make a deficient matrix solvable, but cannot make its parameters identifiable.
inline bool fullRankAndConditioned(std::vector<double> a,std::size_t n,
                                   std::size_t& operations) {
    double scale=0;for(double v:a)scale=std::max(scale,std::abs(v));
    if(!std::isfinite(scale)||scale==0)return false;
    double smallest=scale,largest=0;
    const double threshold=scale*std::numeric_limits<double>::epsilon()*
        128.0*static_cast<double>(std::max<std::size_t>(1,n));
    for(std::size_t k=0;k<n;++k){std::size_t pivot=k;for(std::size_t i=k+1;i<n;++i){++operations;if(std::abs(a[i*n+k])>std::abs(a[pivot*n+k]))pivot=i;}
        const double magnitude=std::abs(a[pivot*n+k]);
        if(!std::isfinite(magnitude)||magnitude<=threshold)return false;
        smallest=std::min(smallest,magnitude);largest=std::max(largest,magnitude);
        if(pivot!=k)for(std::size_t j=k;j<n;++j)std::swap(a[k*n+j],a[pivot*n+j]);
        for(std::size_t i=k+1;i<n;++i){++operations;const double q=a[i*n+k]/a[k*n+k];for(std::size_t j=k+1;j<n;++j){operations+=2;a[i*n+j]-=q*a[k*n+j];}}
    }
    // A deliberately conservative pivot-ratio test; it is a guard, not a
    // high-quality condition-number estimate.
    return smallest>largest*std::numeric_limits<double>::epsilon()*128.0;
}
inline double checkedSSE(std::span<const double> r){double s=0;for(double v:r){if(!std::isfinite(v))return std::numeric_limits<double>::infinity();volatile double q=v*v;volatile double t=s+q;if(!std::isfinite(t))return std::numeric_limits<double>::infinity();s=t;}return s;}
inline bool checkedWorkspaceAdd(std::size_t& total, std::size_t count, std::size_t elementBytes) {
    if (count && elementBytes > (std::numeric_limits<std::size_t>::max() - total) / count)
        return false;
    total += count * elementBytes;
    return true;
}
inline bool checkedWorkspaceProduct(std::size_t a, std::size_t b, std::size_t elementBytes,
                                    std::size_t& bytes) {
    if (a && b > std::numeric_limits<std::size_t>::max() / a)
        return false;
    const auto count = a * b;
    if (count && elementBytes > std::numeric_limits<std::size_t>::max() / count)
        return false;
    bytes = count * elementBytes;
    return true;
}
}
inline LMResult levenbergMarquardt(std::span<const double> initial,
    std::span<const double> lower,std::span<const double> upper,
    const LMResidualFunction& evaluate,const LMConfig& c={},
    const std::atomic<bool>* cancelled=nullptr,
    const std::function<void(std::span<const double>)>& accepted={}) {
    using clock=std::chrono::steady_clock;const auto started=clock::now();
    genetik_detail::validateLevenbergMarquardtConfig(c);
    if (initial.empty() || initial.size() > c.maxParameters || lower.size() != initial.size() ||
        upper.size() != initial.size() || !evaluate)
        throw std::invalid_argument("invalid Levenberg-Marquardt configuration");
    for(std::size_t i=0;i<initial.size();++i)if(!std::isfinite(initial[i])||!std::isfinite(lower[i])||!std::isfinite(upper[i])||lower[i]>initial[i]||initial[i]>upper[i])throw std::invalid_argument("invalid bounded LM start");
    LMResult out;out.parameters.assign(initial.begin(),initial.end());double lambda=c.initialDamping;
    auto vectorBytes = [](const auto& values) {
        std::size_t bytes = 0;
        using Vector = std::decay_t<decltype(values)>;
        if (!genetik_detail::checkedWorkspaceProduct(values.capacity(), 1,
                                                     sizeof(typename Vector::value_type), bytes))
            return std::numeric_limits<std::size_t>::max();
        return bytes;
    };
    std::size_t callbackWorkspaceBound = 0;
    auto call = [&](std::span<const double> parameters, bool jac, std::size_t liveBytes) {
        if (cancelled && cancelled->load(std::memory_order_relaxed)) {
            out.stop = LMStop::Cancelled;
            return LMResidualEvaluation{};
        }
        if (out.work.objectiveEvaluations >= c.maxObjectiveEvaluations) {
            out.stop = LMStop::EvaluationLimit;
            return LMResidualEvaluation{};
        }
        if (jac && out.work.jacobianEvaluations >= c.maxJacobianEvaluations) {
            out.stop = LMStop::DerivativeLimit;
            return LMResidualEvaluation{};
        }
        std::size_t expectedWorkspace = liveBytes;
        if (!genetik_detail::checkedWorkspaceAdd(expectedWorkspace, callbackWorkspaceBound, 1)) {
            out.work.peakWorkspaceBytes = std::numeric_limits<std::size_t>::max();
            out.stop = LMStop::WorkspaceLimit;
            return LMResidualEvaluation{};
        }
        out.work.peakWorkspaceBytes = std::max(out.work.peakWorkspaceBytes, expectedWorkspace);
        if (expectedWorkspace > c.maxWorkspaceBytes) {
            out.stop = LMStop::WorkspaceLimit;
            return LMResidualEvaluation{};
        }
        const auto allowance = c.maxMathematicalOperations - out.work.mathematicalOperations;
        auto e = evaluate(parameters, jac, allowance);
        ++out.work.objectiveEvaluations;
        if (jac)
            ++out.work.jacobianEvaluations;
        out.work.forwardEvaluations += std::max<std::size_t>(1, e.forwardEvaluations);
        out.work.derivativeEvaluations += e.derivativeEvaluations;
        out.work.rowsProcessed += e.rowsProcessed;
        std::size_t callbackBytes = vectorBytes(e.residuals);
        if (!genetik_detail::checkedWorkspaceAdd(callbackBytes, vectorBytes(e.jacobian), 1))
            callbackBytes = std::numeric_limits<std::size_t>::max();
        callbackBytes = std::max(callbackBytes, e.workspaceBytes);
        callbackWorkspaceBound = std::max(callbackWorkspaceBound, callbackBytes);
        std::size_t totalWorkspace = liveBytes;
        if (!genetik_detail::checkedWorkspaceAdd(totalWorkspace, callbackBytes, 1))
            totalWorkspace = std::numeric_limits<std::size_t>::max();
        out.work.peakWorkspaceBytes = std::max(out.work.peakWorkspaceBytes, totalWorkspace);
        out.work.mathematicalOperations =
            e.mathematicalOperations >
                    std::numeric_limits<std::size_t>::max() - out.work.mathematicalOperations
                ? std::numeric_limits<std::size_t>::max()
                : out.work.mathematicalOperations + e.mathematicalOperations;
        if (e.workspaceLimitReached || totalWorkspace > c.maxWorkspaceBytes) {
            out.stop = LMStop::WorkspaceLimit;
            return e;
        }
        if (e.operationLimitReached || e.mathematicalOperations > allowance)
            out.stop = LMStop::EvaluationLimit;
        return e;
    };
    auto current = call(out.parameters, true, vectorBytes(out.parameters));
    if (out.stop == LMStop::Cancelled || out.stop == LMStop::DerivativeLimit ||
        out.stop == LMStop::EvaluationLimit || out.stop == LMStop::WorkspaceLimit) {
        out.work.seconds = std::chrono::duration<double>(clock::now() - started).count();
        return out;
    }
    auto validShape=[&](const LMResidualEvaluation&e,bool jac){if(!e.valid||e.residuals.empty()||e.residuals.size()>c.maxResiduals)return false;if(jac&&e.jacobian.size()!=e.residuals.size()*initial.size())return false;for(double v:e.residuals)if(!std::isfinite(v))return false;for(double v:e.jacobian)if(!std::isfinite(v))return false;return true;};
    if(out.stop==LMStop::WorkspaceLimit){out.work.seconds=std::chrono::duration<double>(clock::now()-started).count();return out;}if(!validShape(current,true)){out.stop=LMStop::InvalidStart;out.work.seconds=std::chrono::duration<double>(clock::now()-started).count();return out;}
    double loss=genetik_detail::checkedSSE(current.residuals);if(!std::isfinite(loss)){out.stop=LMStop::InvalidStart;out.work.seconds=std::chrono::duration<double>(clock::now()-started).count();return out;}
    if (accepted)
        accepted(out.parameters);
    out.sumSquaredResiduals=loss;const std::size_t p=initial.size(),m=current.residuals.size();
    std::size_t matrixBytes = 0;
    std::size_t parameterBytes = 0;
    if (!genetik_detail::checkedWorkspaceProduct(p, p, sizeof(double), matrixBytes) ||
        !genetik_detail::checkedWorkspaceProduct(p, 1, sizeof(double), parameterBytes)) {
        out.stop = LMStop::WorkspaceLimit;
        out.work.seconds = std::chrono::duration<double>(clock::now() - started).count();
        return out;
    }
    const auto parameterCapacityBytes = vectorBytes(out.parameters);
    std::size_t persistentWorkspace = parameterCapacityBytes;
    out.stop=LMStop::IterationLimit;
    for(std::size_t iter=0;iter<c.maxIterations;++iter){if(cancelled&&cancelled->load(std::memory_order_relaxed)){out.stop=LMStop::Cancelled;break;}if(loss<=c.lossTolerance){out.stop=LMStop::LossTolerance;break;}
        // The callback may return larger capacities on later iterations.
        // Account for its current outputs, parameter storage, normal matrix,
        // gradient, and rank-check copy before allocating any normal-system
        // buffers. Rank validation accepts the matrix by value.
        std::size_t outputCapacityBytes = vectorBytes(current.residuals);
        bool rankWorkspaceRepresentable = genetik_detail::checkedWorkspaceAdd(
            outputCapacityBytes, vectorBytes(current.jacobian), 1);
        std::size_t rankWorkspace = std::max(current.workspaceBytes, outputCapacityBytes);
        rankWorkspaceRepresentable =
            rankWorkspaceRepresentable &&
            genetik_detail::checkedWorkspaceAdd(rankWorkspace, parameterCapacityBytes, 1) &&
            genetik_detail::checkedWorkspaceAdd(rankWorkspace, matrixBytes, 1) &&
            genetik_detail::checkedWorkspaceAdd(rankWorkspace, p, sizeof(double)) &&
            genetik_detail::checkedWorkspaceAdd(rankWorkspace, matrixBytes, 1);
        if (!rankWorkspaceRepresentable) {
            out.stop = LMStop::WorkspaceLimit;
            out.work.peakWorkspaceBytes = std::numeric_limits<std::size_t>::max();
            break;
        }
        out.work.peakWorkspaceBytes = std::max(out.work.peakWorkspaceBytes, rankWorkspace);
        if (rankWorkspace > c.maxWorkspaceBytes) {
            out.stop = LMStop::WorkspaceLimit;
            break;
        }
        std::vector<double> a(p*p,0),g(p,0);for(std::size_t i=0;i<m;++i)for(std::size_t j=0;j<p;++j){double ji=current.jacobian[i*p+j];g[j]+=ji*current.residuals[i];out.work.linearAlgebraOperations+=2;for(std::size_t k=0;k<p;++k){a[j*p+k]+=ji*current.jacobian[i*p+k];out.work.linearAlgebraOperations+=2;}}
        if(!genetik_detail::fullRankAndConditioned(a,p,out.work.linearAlgebraOperations)){out.stop=LMStop::SingularSystem;break;}
        current = LMResidualEvaluation{};
        persistentWorkspace = parameterCapacityBytes;
        double norm=0;for(double x:g)norm=std::max(norm,std::abs(x));if(norm<=c.gradientTolerance){out.stop=LMStop::GradientTolerance;break;}
        bool madeStep = false;
        for (unsigned retry = 0; retry < 16; ++retry) {
            std::size_t freeCount = 0;
            if (c.projectStepsToBounds) {
                for (std::size_t j = 0; j < p; ++j) {
                    const bool blocksDescent = (out.parameters[j] <= lower[j] && g[j] > 0) ||
                                               (out.parameters[j] >= upper[j] && g[j] < 0);
                    if (!blocksDescent)
                        ++freeCount;
                }
            }
            const bool useReduced = c.projectStepsToBounds && freeCount > 0 && freeCount < p;
            std::size_t solveWorkspace = persistentWorkspace;
            std::size_t normalWorkspace = vectorBytes(a);
            bool solveWorkspaceRepresentable =
                genetik_detail::checkedWorkspaceAdd(normalWorkspace, vectorBytes(g), 1) &&
                genetik_detail::checkedWorkspaceAdd(solveWorkspace, normalWorkspace, 1);
            std::size_t trialBytes = 0;
            solveWorkspaceRepresentable =
                solveWorkspaceRepresentable &&
                genetik_detail::checkedWorkspaceProduct(p, 1, sizeof(double), trialBytes);
            if (solveWorkspaceRepresentable && useReduced) {
                std::size_t reducedMatrixBytes = 0;
                solveWorkspaceRepresentable =
                    genetik_detail::checkedWorkspaceProduct(freeCount, freeCount, sizeof(double),
                                                            reducedMatrixBytes) &&
                    genetik_detail::checkedWorkspaceAdd(solveWorkspace, freeCount,
                                                        sizeof(std::size_t)) &&
                    genetik_detail::checkedWorkspaceAdd(solveWorkspace, trialBytes, 1) &&
                    genetik_detail::checkedWorkspaceAdd(solveWorkspace, freeCount,
                                                        sizeof(std::size_t)) &&
                    genetik_detail::checkedWorkspaceAdd(solveWorkspace, reducedMatrixBytes, 1) &&
                    genetik_detail::checkedWorkspaceAdd(solveWorkspace, freeCount,
                                                        3 * sizeof(double));
            } else if (solveWorkspaceRepresentable) {
                // Normal system plus damped matrix, copied RHS, and solution.
                solveWorkspaceRepresentable =
                    genetik_detail::checkedWorkspaceAdd(solveWorkspace, matrixBytes, 1) &&
                    genetik_detail::checkedWorkspaceAdd(solveWorkspace, p, 2 * sizeof(double));
                auto postSolveWorkspace = persistentWorkspace;
                solveWorkspaceRepresentable =
                    solveWorkspaceRepresentable &&
                    genetik_detail::checkedWorkspaceAdd(postSolveWorkspace, normalWorkspace, 1) &&
                    genetik_detail::checkedWorkspaceAdd(postSolveWorkspace, p,
                                                        2 * sizeof(double)) &&
                    genetik_detail::checkedWorkspaceAdd(postSolveWorkspace, trialBytes, 1);
                if (solveWorkspaceRepresentable)
                    solveWorkspace = std::max(solveWorkspace, postSolveWorkspace);
            }
            if (!solveWorkspaceRepresentable) {
                out.work.peakWorkspaceBytes = std::numeric_limits<std::size_t>::max();
                out.stop = LMStop::WorkspaceLimit;
                break;
            }
            out.work.peakWorkspaceBytes = std::max(out.work.peakWorkspaceBytes, solveWorkspace);
            if (solveWorkspace > c.maxWorkspaceBytes) {
                out.stop = LMStop::WorkspaceLimit;
                break;
            }
            std::vector<double> delta;
            ++out.work.linearSolves;
            if (!useReduced) {
                auto damped = a;
                for (std::size_t j = 0; j < p; ++j) {
                    damped[j * p + j] += lambda * std::max(1.0, a[j * p + j]);
                    out.work.linearAlgebraOperations += 2;
                }
                if (!genetik_detail::solveDampedSystem(std::move(damped), g, delta, p)) {
                    out.work.linearAlgebraOperations += p * p * p;
                    lambda *= 10;
                    if (lambda > c.maximumDamping) {
                        out.stop = LMStop::SingularSystem;
                        break;
                    }
                    continue;
                }
                out.work.linearAlgebraOperations += p * p * p;
            }
            std::vector<double> trial = out.parameters;
            double stepNorm = 0, parameterNorm = 0;
            bool inside = true;
            for (std::size_t j = 0; j < p; ++j) {
                const double proposed =
                    useReduced ? out.parameters[j] : out.parameters[j] - delta[j];
                parameterNorm = std::max(parameterNorm, std::abs(out.parameters[j]));
                if (c.projectStepsToBounds) {
                    if (!std::isfinite(proposed)) {
                        inside = false;
                        continue;
                    }
                    trial[j] = std::clamp(proposed, lower[j], upper[j]);
                    stepNorm = std::max(stepNorm, std::abs(trial[j] - out.parameters[j]));
                } else {
                    trial[j] = proposed;
                    stepNorm = std::max(stepNorm, std::abs(delta[j]));
                    if (!std::isfinite(trial[j]) || trial[j] < lower[j] || trial[j] > upper[j])
                        inside = false;
                }
            }
            if (useReduced && inside) {
                std::vector<std::size_t> free;
                for (std::size_t j = 0; j < p; ++j) {
                    const bool blocksDescent = (out.parameters[j] <= lower[j] && g[j] > 0) ||
                                               (out.parameters[j] >= upper[j] && g[j] < 0);
                    if (!blocksDescent)
                        free.push_back(j);
                }
                if (!free.empty() && free.size() < p) {
                    std::vector<double> reduced(free.size() * free.size());
                    std::vector<double> reducedGradient(free.size());
                    for (std::size_t j = 0; j < free.size(); ++j) {
                        reducedGradient[j] = g[free[j]];
                        for (std::size_t k = 0; k < free.size(); ++k)
                            reduced[j * free.size() + k] = a[free[j] * p + free[k]];
                        reduced[j * free.size() + j] +=
                            lambda * std::max(1.0, a[free[j] * p + free[j]]);
                    }
                    std::vector<double> reducedDelta;
                    if (genetik_detail::solveDampedSystem(std::move(reduced), reducedGradient,
                                                          reducedDelta, free.size())) {
                        out.work.linearAlgebraOperations += free.size() * free.size() * free.size();
                        trial = out.parameters;
                        stepNorm = 0;
                        for (std::size_t j = 0; j < free.size(); ++j) {
                            const auto coordinate = free[j];
                            trial[coordinate] =
                                std::clamp(out.parameters[coordinate] - reducedDelta[j],
                                           lower[coordinate], upper[coordinate]);
                            stepNorm = std::max(
                                stepNorm, std::abs(trial[coordinate] - out.parameters[coordinate]));
                        }
                    } else {
                        out.work.linearAlgebraOperations += free.size() * free.size() * free.size();
                        ++out.work.rejectedSteps;
                        lambda *= 10;
                        if (lambda > c.maximumDamping) {
                            out.stop = LMStop::DampingLimit;
                            break;
                        }
                        continue;
                    }
                }
            }
            if(!inside){++out.work.rejectedSteps;lambda*=10;if(lambda>c.maximumDamping){out.stop=LMStop::DampingLimit;break;}continue;}
            if(stepNorm<=c.stepTolerance*(parameterNorm+c.stepTolerance)){out.stop=LMStop::StepTolerance;madeStep=true;break;}
            if (cancelled && cancelled->load(std::memory_order_relaxed)) {
                out.stop = LMStop::Cancelled;
                break;
            }
            if (out.work.mathematicalOperations >= c.maxMathematicalOperations) {
                out.stop = LMStop::EvaluationLimit;
                break;
            }
            std::size_t trialLiveWorkspace = persistentWorkspace;
            solveWorkspaceRepresentable =
                genetik_detail::checkedWorkspaceAdd(trialLiveWorkspace, normalWorkspace, 1) &&
                genetik_detail::checkedWorkspaceAdd(trialLiveWorkspace, vectorBytes(trial), 1) &&
                genetik_detail::checkedWorkspaceAdd(trialLiveWorkspace, vectorBytes(delta), 1);
            if (!solveWorkspaceRepresentable) {
                out.stop = LMStop::WorkspaceLimit;
                out.work.peakWorkspaceBytes = std::numeric_limits<std::size_t>::max();
                break;
            }
            auto trialEval = call(trial, false, trialLiveWorkspace);
            if (out.stop == LMStop::Cancelled || out.stop == LMStop::EvaluationLimit ||
                out.stop == LMStop::DerivativeLimit || out.stop == LMStop::WorkspaceLimit)
                break;
            if(validShape(trialEval,false)&&trialEval.residuals.size()==m){double trialLoss=genetik_detail::checkedSSE(trialEval.residuals);if(trialLoss<loss){out.parameters.swap(trial);loss=trialLoss;out.sumSquaredResiduals=loss;++out.work.acceptedSteps;++out.work.parameterUpdates;if (accepted) accepted(out.parameters);lambda=std::max(c.minimumDamping,lambda*.3);madeStep=true;if(loss<=c.lossTolerance)out.stop=LMStop::LossTolerance;break;}}
            ++out.work.rejectedSteps;lambda*=10;if(lambda>c.maximumDamping){out.stop=LMStop::DampingLimit;break;}
        }
        if(out.stop==LMStop::LossTolerance||out.stop==LMStop::SingularSystem||out.stop==LMStop::DampingLimit||out.stop==LMStop::EvaluationLimit||out.stop==LMStop::DerivativeLimit||out.stop==LMStop::WorkspaceLimit||out.stop==LMStop::Cancelled)break;
        if(!madeStep){out.stop=LMStop::DampingLimit;break;}if(out.stop==LMStop::StepTolerance)break;
        std::vector<double>().swap(a);
        std::vector<double>().swap(g);
        current = call(out.parameters, true, parameterCapacityBytes);
        if (out.stop == LMStop::Cancelled || out.stop == LMStop::EvaluationLimit ||
            out.stop == LMStop::DerivativeLimit || out.stop == LMStop::WorkspaceLimit)
            break;
        if (!validShape(current, true) || current.residuals.size() != m) {
            out.stop = LMStop::InvalidStart;
            break;
        }
    }
    out.work.seconds=std::chrono::duration<double>(clock::now()-started).count();
    return out;
}

struct LMExpressionObservation { std::vector<double> inputs; double target=0; };
inline LMResidualFunction expressionLeastSquares(Expression expression,
    std::vector<LMExpressionObservation> rows,std::vector<std::size_t> parameterBindings,
    std::size_t maxWork=10000000, std::vector<double> observationWeights = {},
    std::optional<double> huberDelta = {}) {
    if(rows.empty()||parameterBindings.empty())throw std::invalid_argument("LM expression observations and parameters required");
    for(const auto& row:rows)if(row.inputs.size()!=expression.variableCount()||!std::isfinite(row.target))throw std::invalid_argument("LM expression row schema");
    for(std::size_t i=0;i<parameterBindings.size();++i)if(parameterBindings[i]>=expression.variableCount()||(i&&parameterBindings[i]<=parameterBindings[i-1]))throw std::invalid_argument("LM parameter binding schema");
    if(!observationWeights.empty()){
        if(observationWeights.size()!=rows.size())throw std::invalid_argument("LM observation weight schema");
        double total=0;
        for(double weight:observationWeights){if(!std::isfinite(weight)||weight<0)throw std::invalid_argument("LM observation weights must be finite and nonnegative");volatile double next=total+weight;if(!std::isfinite(next))throw std::invalid_argument("LM observation weight sum overflow");total=next;}
        if(!(total>0))throw std::invalid_argument("LM observation weights require positive total");
    }
    if (huberDelta && (!std::isfinite(*huberDelta) || *huberDelta <= 0.0 ||
                       !observationWeights.empty()))
        throw std::invalid_argument("LM Huber delta must be finite and positive, without weights");
    return [expression=std::move(expression),rows=std::move(rows),parameterBindings=std::move(parameterBindings),maxWork,observationWeights=std::move(observationWeights),huberDelta](std::span<const double> theta,bool jac,std::size_t allowance){LMResidualEvaluation out;const auto p=parameterBindings.size();if(rows.size()>std::numeric_limits<std::size_t>::max()/p||rows.size()*p>maxWork)return out;if(rows.size()>std::numeric_limits<std::size_t>::max()/(p+1)/sizeof(double))return out;out.workspaceBytes=rows.size()*(p+1)*sizeof(double)+expression.variableCount()*sizeof(double);const auto outputWorkspace=out.workspaceBytes;const auto workLimit=std::min(maxWork,allowance);out.residuals.reserve(rows.size());if(jac)out.jacobian.reserve(rows.size()*p);std::vector<double> bindings(expression.variableCount());double totalWeight=0;if(!observationWeights.empty())for(double weight:observationWeights)totalWeight+=weight;
        auto transformHuber = [&](double residual, double& derivative) {
            derivative = 1.0;
            if (!huberDelta) return residual;
            const double magnitude = std::abs(residual);
            if (magnitude <= *huberDelta) return residual;
            // q^2/2 equals rho_delta(r). The algebra avoids forming r^2 in
            // the linear tail and keeps q and dq/dr consistent with scoring.
            const double qMagnitude = std::sqrt(*huberDelta) * std::sqrt(magnitude) *
                                      std::sqrt(2.0 - *huberDelta / magnitude);
            derivative = *huberDelta / qMagnitude;
            return std::copysign(qMagnitude, residual);
        };
        for(const auto& row:rows){std::copy(row.inputs.begin(),row.inputs.end(),bindings.begin());if(theta.size()!=p)return out;for(std::size_t j=0;j<p;++j)bindings[parameterBindings[j]]=theta[j];
            const auto rowIndex=out.rowsProcessed;
            const double weight=observationWeights.empty()?1.0:observationWeights[rowIndex];
            const double scale=observationWeights.empty()?1.0:std::sqrt(weight)/std::sqrt(totalWeight);
            // Even a zero-weight observation must satisfy the normal scoring
            // contract. Validate its primal prediction, then give it no influence
            // on the least-squares system (and avoid requiring a derivative).
            if(!observationWeights.empty()&&weight==0){
                if(expression.arithmeticCost()>workLimit-out.mathematicalOperations)return out;
                auto v=expression.evaluate(bindings);++out.forwardEvaluations;out.mathematicalOperations+=v.arithmeticOperations();
                if(!v.valid())return out;
                out.residuals.push_back(0.0);
                if(jac)for(std::size_t j=0;j<p;++j)out.jacobian.push_back(0.0);
                ++out.rowsProcessed;if(out.mathematicalOperations>workLimit)return out;continue;
            }
            if(jac){DerivativeLimits dl;dl.maxOperations=workLimit-out.mathematicalOperations;auto d=evaluateParameterJacobian(expression,bindings,parameterBindings,dl);out.forwardEvaluations+=d.work.forwardEvaluations;out.derivativeEvaluations+=d.work.forwardEvaluations;out.mathematicalOperations+=d.work.primalOperations+d.work.derivativeOperations+d.work.derivativeMathCalls;out.workspaceBytes=std::max(out.workspaceBytes,outputWorkspace+d.work.workspaceBytes);if(d.status!=ParameterJacobianStatus::Success)return out;const double residual=d.value-row.target;double chainScale=1.0;const double fittedResidual=transformHuber(residual,chainScale);const double objectiveScale=huberDelta?1.0/std::sqrt(2.0*static_cast<double>(rows.size())):scale;out.residuals.push_back(fittedResidual*objectiveScale);for(double derivative:d.derivatives)out.jacobian.push_back(derivative*chainScale*objectiveScale);}
            else {if(expression.arithmeticCost()>workLimit-out.mathematicalOperations)return out;auto v=expression.evaluate(bindings);++out.forwardEvaluations;out.mathematicalOperations+=v.arithmeticOperations();if(!v.valid())return out;const double residual=v.value()-row.target;double chainScale=1.0;const double fittedResidual=transformHuber(residual,chainScale);const double objectiveScale=huberDelta?1.0/std::sqrt(2.0*static_cast<double>(rows.size())):scale;out.residuals.push_back(fittedResidual*objectiveScale);}++out.rowsProcessed;if(out.mathematicalOperations>workLimit)return out;}
        out.valid=true;return out;};
}
