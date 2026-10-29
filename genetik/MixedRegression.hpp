#pragma once
#include "MixedExpression.hpp"
#include "EvaluatedCandidate.hpp"
#include "RandomContext.hpp"
#include "Population.hpp"
#include "GeneticAlgorithm.hpp"
#include <random>

struct MixedIntegerRow { std::int64_t input=0,target=0; };
struct MixedIntegerMetrics { std::size_t matches=0,mismatches=0,invalid=0,resourceExhausted=0; long double absoluteError=0,normalizedError=0; std::uint64_t operations=0,nodeVisits=0; double score=0; };

class MixedIntegerProblem {
public:
    MixedIntegerProblem(std::span<const MixedIntegerRow> rows,std::size_t perEvaluationWork=1000000,std::uint64_t perCandidateWork=10000000):rows_(rows.begin(),rows.end()),work_(perEvaluationWork),candidateWork_(perCandidateWork){if(rows_.empty()||!work_||!candidateWork_)throw std::invalid_argument("mixed integer problem requires rows and work budgets");}
    MixedIntegerMetrics measure(const MixedExpression& expression)const{
        if(expression.inputCount()!=1||expression.rootType()!=MixedType::Int64)throw std::invalid_argument("mixed integer model schema/output mismatch");
        MixedIntegerMetrics m;long double error=0;
        for(std::size_t i=0;i<rows_.size();++i){if(m.operations>=candidateWork_){m.resourceExhausted+=rows_.size()-i;break;}const auto& row=rows_[i];const auto remaining=static_cast<std::size_t>(std::min<std::uint64_t>(candidateWork_-m.operations,std::numeric_limits<std::size_t>::max()));auto r=expression.evaluate(std::span(&row.input,1),std::min(work_,remaining));if(r.operations>std::numeric_limits<std::uint64_t>::max()-m.operations||r.nodeVisits>std::numeric_limits<std::uint64_t>::max()-m.nodeVisits)throw std::overflow_error("mixed work counter overflow");m.operations+=r.operations;m.nodeVisits+=r.nodeVisits;if(r.status==MixedStatus::ResourceLimit){++m.resourceExhausted;continue;}if(!r.valid()){++m.invalid;continue;}const auto prediction=r.value->integer();if(prediction==row.target){++m.matches;continue;}++m.mismatches;const std::uint64_t d=prediction>=row.target?static_cast<std::uint64_t>(prediction)-static_cast<std::uint64_t>(row.target):static_cast<std::uint64_t>(row.target)-static_cast<std::uint64_t>(prediction);const auto magnitude=[](std::int64_t x){return x<0?std::uint64_t(-(x+1))+1:std::uint64_t(x);};const long double absolute=static_cast<long double>(d),target=static_cast<long double>(magnitude(row.target))+1.0L;m.absoluteError+=absolute;error+=absolute/(absolute+target);}
        if(m.invalid||m.resourceExhausted){m.score=static_cast<double>(rows_.size())+1.0;return m;}
        m.normalizedError=m.mismatches?error/static_cast<long double>(rows_.size()):0;
        // Exact mismatch count dominates: the tie-break term is always < 0.5.
        m.score=static_cast<double>(m.mismatches)+static_cast<double>(.5L*m.normalizedError);return m;
    }
    const std::vector<MixedIntegerRow>& rows()const noexcept{return rows_;}std::size_t perEvaluationWork()const noexcept{return work_;}std::uint64_t perCandidateWork()const noexcept{return candidateWork_;}
private:std::vector<MixedIntegerRow>rows_;std::size_t work_;std::uint64_t candidateWork_;
};

class MixedIntegerCandidate final:public EvaluatedCandidate {
    template<class C>friend struct CandidateCheckpointCodec;
public:
    MixedIntegerCandidate(MixedExpression e,std::shared_ptr<const MixedIntegerProblem> p):expression_(std::move(e)),problem_(std::move(p)){if(!problem_)throw std::invalid_argument("mixed integer candidate requires fixed problem");if(expression_.inputCount()!=1||expression_.rootType()!=MixedType::Int64)throw std::invalid_argument("mixed candidate requires one Int64 input and Int64 output");text_=expression_.canonical();}
    MixedIntegerCandidate(const MixedIntegerCandidate&)=default;MixedIntegerCandidate(MixedIntegerCandidate&&)noexcept=default;
    MixedIntegerCandidate& operator=(MixedIntegerCandidate x)noexcept{swap(x);return *this;}
    void swap(MixedIntegerCandidate&x)noexcept{swapEvaluation(x);expression_.swap(x.expression_);problem_.swap(x.problem_);text_.swap(x.text_);std::swap(metrics_,x.metrics_);}
    const MixedExpression& expression()const noexcept{return expression_;}const std::string& text()const noexcept{return text_;}
    MixedComplexity complexity()const{return expression_.complexity();}
    std::shared_ptr<const void> problemIdentity()const noexcept override{return problem_;}
    bool parallelEvaluationSafe()const noexcept override{return true;}
    std::string stats()override{return text_;}
    const MixedIntegerMetrics& metrics()const noexcept{return metrics_;}
    void replace(MixedExpression e){if(e.inputCount()!=1||e.rootType()!=MixedType::Int64)throw std::invalid_argument("mixed candidate requires one Int64 input and Int64 output");auto t=e.canonical();expression_.swap(e);text_.swap(t);metrics_={};invalidateFitness();}
protected:Fitness evaluate()const override{metrics_=problem_->measure(expression_);return metrics_.invalid||metrics_.resourceExhausted?Fitness::invalid():Fitness::valid(metrics_.score);}
private:MixedExpression expression_;std::shared_ptr<const MixedIntegerProblem>problem_;std::string text_;mutable MixedIntegerMetrics metrics_{};
};

struct MixedIntegerOrdering {
    void validateDirection(ObjectiveDirection d)const{if(d!=ObjectiveDirection::Minimize)throw std::invalid_argument("mixed integer objective minimizes mismatch-first loss");}
    bool better(const MixedIntegerCandidate&a,const MixedIntegerCandidate&b,ObjectiveDirection d)const{validateDirection(d);if(a.fitnessState()!=b.fitnessState())return a.fitnessState()==FitnessState::Valid;if(a.fitnessState()==FitnessState::Valid&&a.fitness()!=b.fitness())return a.fitness()<b.fitness();if(a.expression().nodeCount()!=b.expression().nodeCount())return a.expression().nodeCount()<b.expression().nodeCount();if(a.expression().depth()!=b.expression().depth())return a.expression().depth()<b.expression().depth();if(a.expression().constantDescriptionLength()!=b.expression().constantDescriptionLength())return a.expression().constantDescriptionLength()<b.expression().constantDescriptionLength();return a.text()<b.text();}
};

struct DirectIntegerFormulaGrammar {static constexpr std::string_view profileName="direct-integer-formula/1";std::vector<std::int64_t> integers{-2,-1,0,1,2};std::vector<double> reals{-2,-1,0,1,2};double continuousProbability=.35;double continuousLower=-4,continuousUpper=4;std::size_t maxDepth=6,proposalLimit=8;};

class MixedIntegerVariation {
public:
    MixedIntegerVariation(std::shared_ptr<const MixedIntegerProblem> problem,DirectIntegerFormulaGrammar grammar={},MixedLimits limits={}):problem_(std::move(problem)),grammar_(std::move(grammar)),limits_(limits){if(!problem_||grammar_.integers.empty()||grammar_.reals.empty()||!grammar_.maxDepth||grammar_.maxDepth>64||!grammar_.proposalLimit||grammar_.proposalLimit>4096||!limits_.maxNodes||!limits_.maxDepth||!limits_.maxOperations||!std::isfinite(limits_.maxAbsReal)||limits_.maxAbsReal<0||!std::isfinite(grammar_.continuousProbability)||grammar_.continuousProbability<0||grammar_.continuousProbability>1||!std::isfinite(grammar_.continuousLower)||!std::isfinite(grammar_.continuousUpper)||grammar_.continuousLower>grammar_.continuousUpper||std::abs(grammar_.continuousLower)>limits_.maxAbsReal||std::abs(grammar_.continuousUpper)>limits_.maxAbsReal)throw std::invalid_argument("invalid direct integer formula grammar");for(double c:grammar_.reals)if(!std::isfinite(c)||std::abs(c)>limits_.maxAbsReal)throw std::invalid_argument("mixed grammar real constant outside limits");}
    MixedIntegerCandidate initialize(std::mt19937&rng){return MixedIntegerCandidate(generate(MixedType::Int64,grammar_.maxDepth,rng),problem_);}
    MixedIntegerCandidate mutation(const MixedIntegerCandidate&p,float rate,std::mt19937&rng){requireProblem(p);auto child=p;if(!std::isfinite(rate)||rate<0||rate>1)throw std::invalid_argument("mixed mutation rate must be in [0,1]");if(std::uniform_real_distribution<float>(0,1)(rng)>=rate)return child;for(std::size_t attempt=0;attempt<grammar_.proposalLimit;++attempt){const auto root=pick(p.expression().nodeCount(),rng);auto replacement=generate(p.expression().nodeType(root),std::min(grammar_.maxDepth,limits_.maxDepth),rng);auto next=p.expression().replaceSubtree(root,replacement);if(next){child.replace(std::move(*next));return child;}}return child;}
    void crossover(const MixedIntegerCandidate&f,const MixedIntegerCandidate&m,MixedIntegerCandidate&a,MixedIntegerCandidate&b,float rate,std::mt19937&rng){requireProblem(f);requireProblem(m);if(!std::isfinite(rate)||rate<0||rate>1)throw std::invalid_argument("mixed crossover rate must be in [0,1]");auto left=f,right=m;if(std::uniform_real_distribution<float>(0,1)(rng)<rate){auto fn=f.expression().nodes(),mn=m.expression().nodes();for(std::size_t attempt=0;attempt<grammar_.proposalLimit;++attempt){auto i=pick(fn.size(),rng),j=pick(mn.size(),rng);if(f.expression().nodeType(i)!=m.expression().nodeType(j))continue;auto x=f.expression().replaceSubtree(i,subtree(m.expression(),j));auto y=m.expression().replaceSubtree(j,subtree(f.expression(),i));if(x&&y){left.replace(std::move(*x));right.replace(std::move(*y));break;}}}a=std::move(left);b=std::move(right);}
private:
    void requireProblem(const MixedIntegerCandidate& c)const{if(c.problemIdentity()!=std::shared_ptr<const void>(problem_))throw std::invalid_argument("mixed candidate belongs to another immutable problem");}
    static std::size_t pick(std::size_t n,std::mt19937&r){return std::uniform_int_distribution<std::size_t>(0,n-1)(r);}
    static MixedExpression subtree(const MixedExpression&e,std::size_t root){auto[first,last]=e.subtree(root);auto n=e.nodes();return MixedExpression(std::span(n.data()+first,last-first),e.inputCount(),e.limits(),e.nodeType(root));}
    std::int64_t integer(std::mt19937&r){return grammar_.integers[pick(grammar_.integers.size(),r)];}
    double real(std::mt19937&r){if(std::bernoulli_distribution(grammar_.continuousProbability)(r))return std::uniform_real_distribution<double>(grammar_.continuousLower,grammar_.continuousUpper)(r);return grammar_.reals[pick(grammar_.reals.size(),r)];}
    std::vector<MixedNode> build(MixedType type,std::size_t depth,std::mt19937&r){using K=MixedKind;std::vector<MixedNode> out;auto append=[&](std::vector<MixedNode>& part){if(part.size()>limits_.maxNodes-out.size())throw std::length_error("mixed grammar candidate node limit");out.insert(out.end(),part.begin(),part.end());};auto terminal=[&](MixedNode n){if(out.size()==limits_.maxNodes)throw std::length_error("mixed grammar candidate node limit");out.push_back(std::move(n));};auto operation=[&](MixedKind k){terminal(MixedNode::operation(k));};auto child=[&](MixedType t){return build(t,depth>1?depth-1:1,r);};
        if(type==MixedType::Int64){const int choice=depth>1?std::uniform_int_distribution<int>(0,9)(r):0;if(choice==0){if(std::bernoulli_distribution(.5)(r))terminal(MixedNode::input(0));else terminal(MixedNode::integer(integer(r)));}else if(choice<=4){auto a=child(MixedType::Int64),b=child(MixedType::Int64);append(a);append(b);const K ops[]={K::IAdd,K::ISubtract,K::IMultiply,K::IModulo};operation(ops[choice-1]);}else if(choice<=8){auto a=child(MixedType::Real64);append(a);const K ops[]={K::FloorToInt,K::CeilToInt,K::TruncateToInt,K::NearestToInt};operation(ops[choice-5]);}else{auto c=child(MixedType::Bool),a=child(MixedType::Int64),b=child(MixedType::Int64);append(c);append(a);append(b);operation(K::IfThenElse);}}
        else if(type==MixedType::Real64){const int choice=depth>1?std::uniform_int_distribution<int>(0,18)(r):0;if(choice==0)terminal(MixedNode::real(real(r)));else if(choice==1){auto a=child(MixedType::Int64);append(a);operation(K::IntToReal);}else if(choice<=6){auto a=child(MixedType::Real64),b=child(MixedType::Real64);append(a);append(b);const K ops[]={K::RAdd,K::RSubtract,K::RMultiply,K::RDivide,K::RPow};operation(ops[choice-2]);}else{auto a=child(MixedType::Real64);append(a);const K ops[]={K::RLog,K::RExp,K::RSin,K::RCos,K::RTan,K::RAsin,K::RAcos,K::RAtan,K::RSinh,K::RCosh,K::RTanh,K::RSqrt};operation(ops[choice-7]);}}
        else {auto a=child(MixedType::Int64),b=child(MixedType::Int64);append(a);append(b);operation(std::bernoulli_distribution(.5)(r)?K::LessInt:K::EqualInt);}
        return out;}
    MixedExpression generate(MixedType type,std::size_t depth,std::mt19937&r){for(std::size_t attempt=0;attempt<grammar_.proposalLimit;++attempt){try{auto n=build(type,std::max<std::size_t>(1,depth),r);auto e=MixedExpression(n,1,limits_,type);if(e.rootType()!=type)throw std::logic_error("mixed grammar generated wrong requested root type");return e;}catch(const std::length_error&){}}std::vector<MixedNode> fallback;if(type==MixedType::Int64)fallback.push_back(MixedNode::input(0));else if(type==MixedType::Real64)fallback.push_back(MixedNode::real(grammar_.reals.front()));else{fallback={MixedNode::input(0),MixedNode::integer(0),MixedNode::operation(MixedKind::LessInt)};}return MixedExpression(fallback,1,limits_,type);}
    std::shared_ptr<const MixedIntegerProblem>problem_;DirectIntegerFormulaGrammar grammar_;MixedLimits limits_;
};
