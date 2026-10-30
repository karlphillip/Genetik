#pragma once
#include "RunCheckpoint.hpp"
#include "MixedRegression.hpp"

struct MixedIntegerCheckpointContext { std::shared_ptr<const MixedIntegerProblem> problem; MixedLimits limits{}; };

template<> struct CandidateCheckpointCodec<MixedIntegerCandidate> {
    static constexpr std::string_view identifier="genetik.mixed-integer-candidate/1";
    static void encode(CheckpointWriter&w,const MixedIntegerCandidate&c){
        if(c.fitnessState()==FitnessState::Unevaluated)throw CheckpointError("cannot checkpoint unevaluated mixed candidate");
        w.text(c.expression_.serialize());if(c.expression_.parameterCount()){w.integer(c.parameters_.size());for(double p:c.parameters_)w.real(p);}w.integer(c.fitnessState()==FitnessState::Valid?1:2);if(c.fitnessState()==FitnessState::Valid)w.real(c.fitness());
        w.integer(std::bit_cast<std::uint32_t>(c.percent_));w.integer(std::bit_cast<std::uint32_t>(c.start_));w.integer(std::bit_cast<std::uint32_t>(c.end_));
    }
    static MixedIntegerCandidate decode(CheckpointReader&r,const MixedIntegerCheckpointContext&context){
        if(!context.problem)throw CheckpointError("mixed checkpoint requires a fixed problem");
        auto expression=[&](){try{return MixedExpression::deserialize(r.text(),context.limits,MixedType::Int64);}catch(...){std::throw_with_nested(CheckpointError("mixed expression decode failed"));}}();
        std::vector<double> parameters;if(expression.parameterCount()){const auto count=r.count(MixedExpression::MaxSharedParameters);if(count!=expression.parameterCount())throw CheckpointError("mixed shared parameter count differs from expression");parameters.reserve(count);for(std::size_t i=0;i<count;++i){const auto value=r.real();if(!std::isfinite(value)||std::abs(value)>context.limits.maxAbsReal)throw CheckpointError("invalid mixed shared parameter value");parameters.push_back(value);}}
        MixedIntegerCandidate c=expression.parameterCount()?MixedIntegerCandidate(std::move(expression),std::move(parameters),context.problem):MixedIntegerCandidate(std::move(expression),context.problem);c.calculateFitness();
        const auto state=r.count(2);if(state==1){const auto score=r.real();if(c.fitnessState()!=FitnessState::Valid||std::bit_cast<std::uint64_t>(c.fitness())!=std::bit_cast<std::uint64_t>(score))throw CheckpointError("mixed candidate fitness differs after reconstruction");}else if(state!=2||c.fitnessState()!=FitnessState::Invalid)throw CheckpointError("mixed candidate validity differs after reconstruction");
        c.percent_=std::bit_cast<float>(static_cast<std::uint32_t>(r.count(UINT32_MAX)));c.start_=std::bit_cast<float>(static_cast<std::uint32_t>(r.count(UINT32_MAX)));c.end_=std::bit_cast<float>(static_cast<std::uint32_t>(r.count(UINT32_MAX)));
        if(!std::isfinite(c.percent_)||!std::isfinite(c.start_)||!std::isfinite(c.end_)||c.percent_<0||c.percent_>100||c.start_<0||c.start_>c.end_||c.end_>100)throw CheckpointError("invalid mixed selection state");
        return c;
    }
};
