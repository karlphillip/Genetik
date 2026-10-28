#pragma once
#include "RunCheckpoint.hpp"
#include "DiscreteRegression.hpp"
#include <charconv>
#include <cmath>

struct DiscreteCheckpointContext {
    std::shared_ptr<const DiscreteRegressionProblem> problem;
    std::shared_ptr<DiscreteTrainingObjective> objective;
};
// Candidate codec only. Generic run-state restoration additionally requires the
// caller's complete immutable configuration identity and objective work adapter.
template<> struct CandidateCheckpointCodec<DiscreteExpressionCandidate> {
    static constexpr std::string_view identifier="genetik.discrete-expression-candidate/1";
    static void encode(CheckpointWriter& w,const DiscreteExpressionCandidate& c){
        if(typeid(c)!=typeid(DiscreteExpressionCandidate)||c.parent_||c.fitnessState()==FitnessState::Unevaluated)throw CheckpointError("unsupported discrete candidate state");
        w.integer(c.expression().dimensions());w.integer(c.expression().nodeCount());
        for(const auto& n:c.expression().nodes()){
            w.integer(static_cast<unsigned>(n.kind()));
            if(n.kind()==DiscreteKind::Integer)w.text(std::to_string(n.integer()));
            if(n.kind()==DiscreteKind::Boolean)w.integer(n.boolean()?1:0);
            if(n.kind()==DiscreteKind::Variable)w.integer(n.variable());
            if(n.kind()==DiscreteKind::Iterator||discreteIteration(n.kind()))w.integer(n.binder());
        }
        w.integer(c.fitnessState()==FitnessState::Valid?1:2);if(c.fitnessState()==FitnessState::Valid)w.real(c.fitness());
        w.integer(std::bit_cast<std::uint32_t>(c.percent_));w.integer(std::bit_cast<std::uint32_t>(c.start_));w.integer(std::bit_cast<std::uint32_t>(c.end_));
    }
    static DiscreteExpressionCandidate decode(CheckpointReader& r,const DiscreteCheckpointContext& context){
        if(!context.problem)throw CheckpointError("discrete checkpoint requires validated context");
        const auto dimensions=r.count(context.problem->limits().maxVariables);
        if(dimensions!=context.problem->dimensions())throw CheckpointError("discrete checkpoint schema mismatch");
        const auto cap=std::min({context.problem->limits().maxNodes,context.problem->limits().maxStorageBytes/DiscreteExpression::storageBytesPerNode(),r.limits().maxBytes/8});
        const auto size=r.count(cap);if(!size)throw CheckpointError("empty discrete checkpoint");
        std::vector<DiscreteNode> nodes;nodes.reserve(size);
        for(std::size_t i=0;i<size;++i){const auto k=static_cast<DiscreteKind>(r.count(25));
            if(k==DiscreteKind::Integer){auto text=r.text(20);if(text.size()>20)throw CheckpointError("discrete integer token too large");std::int64_t n=0;auto parsed=std::from_chars(text.data(),text.data()+text.size(),n);if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size())throw CheckpointError("invalid discrete integer");nodes.push_back(DiscreteNode::integer(n));}
            else if(k==DiscreteKind::Boolean)nodes.push_back(DiscreteNode::boolean(r.count(1)!=0));
            else if(k==DiscreteKind::Variable)nodes.push_back(DiscreteNode::variable(r.count(dimensions?dimensions-1:0)));
            else if(k==DiscreteKind::Iterator)nodes.push_back(DiscreteNode::iterator(r.count(std::numeric_limits<std::size_t>::max())));
            else if(discreteIteration(k))nodes.push_back(DiscreteNode::iteration(k,r.count(std::numeric_limits<std::size_t>::max())));
            else nodes.push_back(DiscreteNode::operation(k));
        }
        DiscreteExpressionCandidate c(DiscreteExpression(nodes,dimensions,context.problem->limits()),context.problem,context.objective);
        const auto state=r.count(2);if(state==1){const auto loss=r.real();if(loss<0||loss>static_cast<double>(context.problem->sampleCount())||std::floor(loss)!=loss)throw CheckpointError("invalid discrete mismatch score");c.fitness_=Fitness::valid(loss);}else if(state==2)c.fitness_=Fitness::invalid();else throw CheckpointError("unevaluated discrete checkpoint");
        c.percent_=std::bit_cast<float>(static_cast<std::uint32_t>(r.count(std::numeric_limits<std::uint32_t>::max())));c.start_=std::bit_cast<float>(static_cast<std::uint32_t>(r.count(std::numeric_limits<std::uint32_t>::max())));c.end_=std::bit_cast<float>(static_cast<std::uint32_t>(r.count(std::numeric_limits<std::uint32_t>::max())));
        if(!std::isfinite(c.percent_)||!std::isfinite(c.start_)||!std::isfinite(c.end_)||c.percent_<0||c.percent_>100||c.start_<0||c.start_>c.end_||c.end_>100)throw CheckpointError("invalid discrete selection state");
        return c;
    }
};
