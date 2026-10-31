#pragma once
#include "LevenbergMarquardt.hpp"
#include "ScientificEquationSystem.hpp"

inline LMResidualFunction scientificSystemLeastSquares(const ScientificEquationSystem& system,
    std::size_t maxWork=10000000) {
    const auto equations=system.equations();const auto channels=system.channels();const auto inputCount=system.inputCount();const auto parameters=system.parameters();
    if(parameters.empty())throw std::invalid_argument("scientific LM requires shared parameters");
    double totalWeight=0;for(const auto& c:channels)totalWeight+=c.weight;
    return [equations,channels,inputCount,parameters,totalWeight,maxWork](std::span<const double> theta,bool jac,std::size_t allowance){LMResidualEvaluation out;const auto p=parameters.size();std::size_t count=0;for(const auto& c:channels){if(c.observations.size()>std::numeric_limits<std::size_t>::max()-count)return out;count+=c.observations.size();}if(count>maxWork||count>std::numeric_limits<std::size_t>::max()/p||count*p>maxWork||count>std::numeric_limits<std::size_t>::max()/(p+1)/sizeof(double)||theta.size()!=p)return out;out.workspaceBytes=count*(p+1)*sizeof(double)+(inputCount+p)*sizeof(double);const auto outputWorkspace=out.workspaceBytes;const auto workLimit=std::min(maxWork,allowance);out.residuals.reserve(count);if(jac)out.jacobian.reserve(count*p);std::vector<double> bindings(inputCount+p);
        for(const auto& channel:channels){const double scale=std::sqrt((channel.weight/totalWeight)/static_cast<double>(channel.observations.size()));for(const auto& row:channel.observations){for(std::size_t j=0;j<p;++j)if(theta[j]<parameters[j].lower||theta[j]>parameters[j].upper)return out;std::copy(theta.begin(),theta.end(),bindings.begin()+inputCount);std::copy(row.inputs.begin(),row.inputs.end(),bindings.begin());const auto& e=equations[channel.equation];if(jac){std::vector<std::size_t> ids(p);for(std::size_t j=0;j<p;++j)ids[j]=inputCount+j;DerivativeLimits dl;dl.maxOperations=workLimit-out.mathematicalOperations;auto d=evaluateParameterJacobian(e,bindings,ids,dl);out.forwardEvaluations+=d.work.forwardEvaluations;out.derivativeEvaluations+=d.work.forwardEvaluations;out.mathematicalOperations+=d.work.primalOperations+d.work.derivativeOperations+d.work.derivativeMathCalls;out.workspaceBytes=std::max(out.workspaceBytes,outputWorkspace+d.work.workspaceBytes);if(d.status!=ParameterJacobianStatus::Success)return out;out.residuals.push_back((d.value-row.target)*scale);for(double v:d.derivatives)out.jacobian.push_back(v*scale);}else{if(e.arithmeticCost()>workLimit-out.mathematicalOperations)return out;auto v=e.evaluate(bindings);++out.forwardEvaluations;out.mathematicalOperations+=v.arithmeticOperations();if(!v.valid())return out;out.residuals.push_back((v.value()-row.target)*scale);}++out.rowsProcessed;if(out.mathematicalOperations>workLimit)return out;}}
        out.valid=true;return out;};
}

inline LMResult fitScientificParametersLevenbergMarquardt(
    const ScientificEquationSystem& system,LMConfig config={},
    const std::atomic<bool>* cancelled=nullptr) {
    std::vector<double> initial,lower,upper;
    initial.reserve(system.parameters().size());lower.reserve(system.parameters().size());upper.reserve(system.parameters().size());
    for(const auto& p:system.parameters()){initial.push_back(p.initial);lower.push_back(p.lower);upper.push_back(p.upper);}
    if(initial.empty())throw std::invalid_argument("scientific LM requires at least one shared parameter");
    config.maxResiduals=std::min(config.maxResiduals,system.limits().maxElements);
    config.maxMathematicalOperations=std::min(config.maxMathematicalOperations,system.limits().maxOperations);
    return levenbergMarquardt(initial,lower,upper,scientificSystemLeastSquares(system,system.limits().maxOperations),config,cancelled);
}
