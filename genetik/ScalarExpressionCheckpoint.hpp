#pragma once
#include "RunCheckpoint.hpp"
#include "ConstantFitting.hpp"
#include "ExpressionVariation.hpp"

struct ScalarCheckpointContext {
    std::shared_ptr<const RegressionProblem> problem;
    std::shared_ptr<RegressionPreparation> preparation;
};
// Explicit first-party codec; constrained/template contexts and custom subclasses
// require their own codec. Cached scores are trusted only after context validation.
template<> struct CandidateCheckpointCodec<ExpressionCandidate> {
    static constexpr std::string_view identifier="genetik.scalar-expression-candidate/1";
    static void encode(CheckpointWriter& w,const ExpressionCandidate& c){
        if(typeid(c)!=typeid(ExpressionCandidate)||c.contextOwner_||c.parentPreparation_||c.fitnessState()==FitnessState::Unevaluated)throw CheckpointError("unsupported candidate codec/context");
        const auto& e=c.expression();const auto l=e.limits();w.integer(e.variableCount());w.integer(l.maxNodes);w.integer(l.maxDepth);w.real(l.maxAbsConstant);w.integer(l.maxArithmeticOperations);
        w.integer(e.nodeCount());
        for(const auto& n:e.nodes()){
            w.integer(kindCode(n.kind()));
            if(n.kind()==ExpressionKind::Constant)w.real(n.constantValue());
            else if(n.kind()==ExpressionKind::Variable)w.integer(n.variableIndex());
            else if(expressionPowerMetadata(n.kind()))w.integer(n.exponent());
        }
        w.integer(c.fitnessState()==FitnessState::Valid?1:c.fitnessState()==FitnessState::Invalid?2:0);
        if(c.fitnessState()==FitnessState::Valid)w.real(c.fitness());
        w.integer(std::bit_cast<std::uint32_t>(c.percent_));w.integer(std::bit_cast<std::uint32_t>(c.start_));w.integer(std::bit_cast<std::uint32_t>(c.end_));
        w.integer(c.context_.generation);w.integer(static_cast<unsigned>(c.context_.origin));
    }
    static ExpressionCandidate decode(CheckpointReader& r,const ScalarCheckpointContext& context){
        if(!context.problem)throw CheckpointError("checkpoint problem context required");
        const auto vars=r.count(100000);ExpressionLimits l;
        l.maxNodes=r.count(100000);l.maxDepth=r.count(100000);l.maxAbsConstant=r.real();l.maxArithmeticOperations=r.integer();
        const auto expected=context.problem->limits();
        if(vars!=context.problem->dimensions() || l.maxNodes!=expected.maxNodes || l.maxDepth!=expected.maxDepth ||
           std::bit_cast<std::uint64_t>(l.maxAbsConstant)!=std::bit_cast<std::uint64_t>(expected.maxAbsConstant) || l.maxArithmeticOperations!=expected.maxArithmeticOperations)throw CheckpointError("candidate expression limits/schema differ");
        const auto count=r.count(l.maxNodes);if(!count)throw CheckpointError("empty expression checkpoint");std::vector<ExpressionNode> nodes;nodes.reserve(count);
        for(std::size_t i=0;i<count;++i){const auto k=codeKind(r.integer());
            if(k==ExpressionKind::Constant)nodes.push_back(ExpressionNode::constant(r.real()));
            else if(k==ExpressionKind::Variable)nodes.push_back(ExpressionNode::variable(r.count(vars?vars-1:0)));
            else if(expressionPowerMetadata(k)){auto power=static_cast<unsigned>(r.count(8));if(k==ExpressionKind::NegativePowi&&!power)throw CheckpointError("zero negative-power magnitude");nodes.push_back(k==ExpressionKind::Powi?ExpressionNode::power(power):ExpressionNode::signedPower(-static_cast<int>(power)));}
            else if(expressionArity(k)==1)nodes.push_back(ExpressionNode::unary(k));
            else if(expressionArity(k)==2)nodes.push_back(ExpressionNode::binary(k));else nodes.push_back(ExpressionNode::ternary(k));
        }
        ExpressionCandidate c(Expression(nodes,vars,l),context.problem,context.preparation);
        const auto state=r.integer();if(state==1)c.fitness_=Fitness::valid(r.real());else if(state==2)c.fitness_=Fitness::invalid();else throw CheckpointError("unevaluated/unknown committed fitness");
        c.percent_=std::bit_cast<float>(static_cast<std::uint32_t>(r.count(std::numeric_limits<std::uint32_t>::max())));
        c.start_=std::bit_cast<float>(static_cast<std::uint32_t>(r.count(std::numeric_limits<std::uint32_t>::max())));
        c.end_=std::bit_cast<float>(static_cast<std::uint32_t>(r.count(std::numeric_limits<std::uint32_t>::max())));
        if(!std::isfinite(c.percent_)||!std::isfinite(c.start_)||!std::isfinite(c.end_)||c.percent_<0||c.percent_>100||c.start_<0||c.start_>c.end_||c.end_>100)throw CheckpointError("invalid selection state");
        c.context_.generation=r.integer();c.context_.origin=static_cast<PreparationOrigin>(r.count(4));return c;
    }
private:
    static constexpr auto kinds=[] {std::array<ExpressionKind,scalarOperators.size()> a{};for(std::size_t i=0;i<a.size();++i)a[i]=scalarOperators[i].kind;return a;}();
    static std::uint64_t kindCode(ExpressionKind k){for(std::size_t i=0;i<kinds.size();++i)if(kinds[i]==k)return i;throw CheckpointError("unknown expression kind");}
    static ExpressionKind codeKind(std::uint64_t c){if(c>=kinds.size())throw CheckpointError("unsupported expression kind code");return kinds[c];}
};

#define GENETIK_TRAINING_FIELDS(X) X(lossAttempts) X(rowAttempts) X(fittingInvocations) X(fittingLossAttempts) X(proposals) X(accepted) X(invalid) X(infeasible) X(duplicates) X(invalidLosses) X(ordinaryLossAttempts) X(fittingBaselineAttempts) X(fittingProbeAttempts) X(fittingImproved)
inline void writeTrainingWork(CheckpointWriter& w,const TrainingWork& v){
#define GENETIK_WRITE(f) w.integer(v.f);
    GENETIK_TRAINING_FIELDS(GENETIK_WRITE)
#undef GENETIK_WRITE
}
inline TrainingWork readTrainingWork(CheckpointReader& r){TrainingWork v;
#define GENETIK_READ(f) v.f=r.integer();
    GENETIK_TRAINING_FIELDS(GENETIK_READ)
#undef GENETIK_READ
    return v;
}
#undef GENETIK_TRAINING_FIELDS
inline void writePreparationStatistics(CheckpointWriter& w,const PreparationStatistics& s){
    for(auto v:s.origins)for(auto n:{v.requests,v.fits,v.skipped,v.ordinary,v.baselines,v.probes,v.rows,v.improved})w.integer(n);
    for(auto n:{s.skippedBySchedule,s.completedImproved,s.completedUnchanged,s.noConstants,s.invalidStarts,s.abortedFits})w.integer(n);
    for(auto n:s.terminations)w.integer(n);
}
inline PreparationStatistics readPreparationStatistics(CheckpointReader& r){PreparationStatistics s;
    for(auto& v:s.origins){v.requests=r.integer();v.fits=r.integer();v.skipped=r.integer();v.ordinary=r.integer();v.baselines=r.integer();v.probes=r.integer();v.rows=r.integer();v.improved=r.integer();}
    s.skippedBySchedule=r.integer();s.completedImproved=r.integer();s.completedUnchanged=r.integer();s.noConstants=r.integer();s.invalidStarts=r.integer();s.abortedFits=r.integer();for(auto& n:s.terminations)n=r.integer();return s;
}
inline void writeVariationStatistics(CheckpointWriter& w,const VariationStatistics& s){for(auto n:{s.initializations,s.mutationProposals,s.crossoverProposals,s.structuralRejections,s.mutationFallbacks,s.crossoverFallbacks,s.mutationChanges,s.crossoverChanges})w.integer(n);}
inline VariationStatistics readVariationStatistics(CheckpointReader& r){VariationStatistics s;s.initializations=r.integer();s.mutationProposals=r.integer();s.crossoverProposals=r.integer();s.structuralRejections=r.integer();s.mutationFallbacks=r.integer();s.crossoverFallbacks=r.integer();s.mutationChanges=r.integer();s.crossoverChanges=r.integer();return s;}

// Reviewed forward read from published0.3.0; strict compiler/runtime/FP suffix and
// unchanged codec payload are still required. Generic loading stays exact-version.
inline std::optional<CheckpointReadCompatibility> scalarCheckpointCompatibility03(){
    if(std::string_view(genetikVersion)=="0.4.0"||std::string_view(genetikVersion)=="0.5.0"||std::string_view(genetikVersion)=="0.6.0")return CheckpointReadCompatibility{"0.3.0",std::string(CandidateCheckpointCodec<ExpressionCandidate>::identifier)};
    return {};
}
inline std::vector<CheckpointReadCompatibility> scalarCheckpointCompatibilities(){
    const auto version=std::string_view(genetikVersion);const auto codec=std::string(CandidateCheckpointCodec<ExpressionCandidate>::identifier);
    if(version=="0.6.0")return {{"0.5.0",codec},{"0.4.0",codec},{"0.3.0",codec}};
    if(version=="0.5.0")return {{"0.4.0",codec},{"0.3.0",codec}};
    if(version=="0.4.0")return {{"0.3.0",codec}};
    return {};
}
inline CheckpointDocument loadScalarCheckpoint(const std::filesystem::path& path,CheckpointLimits limits={}){
    return loadCompatibleCheckpointWithPolicies(path,limits,scalarCheckpointCompatibilities());
}
