#pragma once
#include "ExpressionDerivative.hpp"
#include <algorithm>
#include <limits>
#include <vector>

// A dense parameter Jacobian assembled from the authoritative checked scalar
// evaluator. Each column is a forward-mode partial with respect to an explicit
// parameter binding; repeated variable references therefore share one column.
enum class ParameterJacobianStatus { Success, PrimalInvalidity, Unavailable, ResourceLimit };
struct ParameterJacobianWork {
    std::size_t forwardEvaluations=0, primalOperations=0, derivativeOperations=0,
                 derivativeMathCalls=0, nodesVisited=0, workspaceBytes=0;
};
struct ParameterJacobianResult {
    ParameterJacobianStatus status=ParameterJacobianStatus::ResourceLimit;
    double value=0;
    std::vector<double> derivatives;
    std::optional<std::size_t> failedParameter;
    std::optional<DerivativeFailure> failure;
    std::optional<DerivativeResource> resource;
    ParameterJacobianWork work;
};

inline ParameterJacobianResult evaluateParameterJacobian(
    const Expression& expression, std::span<const double> bindings,
    std::span<const std::size_t> parameterBindingIndices,
    DerivativeLimits limits={}) {
    ParameterJacobianResult out;
    if(bindings.size()!=expression.variableCount())
        throw std::invalid_argument("parameter Jacobian binding count differs from expression schema");
    for(std::size_t i=0;i<parameterBindingIndices.size();++i) {
        if(parameterBindingIndices[i]>=bindings.size())
            throw std::invalid_argument("parameter binding index outside expression schema");
        if(i && parameterBindingIndices[i]<=parameterBindingIndices[i-1])
            throw std::invalid_argument("parameter binding indices must be increasing and unique");
    }
    out.derivatives.reserve(parameterBindingIndices.size());
    std::optional<DerivativeResult> first;
    std::size_t remainingOperations=limits.maxOperations;
    for(std::size_t i=0;i<parameterBindingIndices.size();++i) {
        auto columnLimits=limits;columnLimits.maxOperations=remainingOperations;
        auto d=evaluatePartial(expression,bindings,parameterBindingIndices[i],columnLimits);
        ++out.work.forwardEvaluations;
        const auto& w=d.work();
        auto add=[](std::size_t& a,std::size_t b){if(b>std::numeric_limits<std::size_t>::max()-a)throw std::overflow_error("parameter Jacobian work overflow");a+=b;};
        add(out.work.primalOperations,w.primalOperations);add(out.work.derivativeOperations,w.derivativeArithmetic);add(out.work.derivativeMathCalls,w.derivativeMathCalls);add(out.work.nodesVisited,w.nodesVisited);out.work.workspaceBytes=std::max(out.work.workspaceBytes,w.workspaceBytes);
        const auto used=w.operations();if(used>remainingOperations){out.status=ParameterJacobianStatus::ResourceLimit;out.resource=DerivativeResource::Operations;out.failedParameter=i;return out;}remainingOperations-=used;
        if(i==0)first=d;
        if(d.status()==DerivativeStatus::ResourceLimit){out.status=ParameterJacobianStatus::ResourceLimit;out.resource=d.exhaustedResource();out.failedParameter=i;return out;}
        if(d.status()==DerivativeStatus::PrimalInvalidity){out.status=ParameterJacobianStatus::PrimalInvalidity;out.failedParameter=i;return out;}
        if(d.status()==DerivativeStatus::Unavailable){out.status=ParameterJacobianStatus::Unavailable;out.failedParameter=i;out.failure=d.derivativeFailure();return out;}
        out.derivatives.push_back(d.derivative());
    }
    if(first)out.value=first->value();
    else {auto p=expression.evaluate(bindings);if(!p.valid()){out.status=ParameterJacobianStatus::PrimalInvalidity;return out;}out.value=p.value();}
    out.status=ParameterJacobianStatus::Success;
    return out;
}
