#pragma once
#include "DiscreteExpression.hpp"
#include "EvaluatedCandidate.hpp"
#include "Fitness.hpp"
#include <memory>
#include <mutex>
#include <typeinfo>

class DiscreteResourceExhausted : public std::runtime_error {public:using std::runtime_error::runtime_error;};
class DiscreteLossBudgetExceeded : public EvaluationBudgetExceeded {public:DiscreteLossBudgetExceeded():EvaluationBudgetExceeded("discrete training loss allowance exhausted") {}};
struct DiscreteRegressionRow {std::vector<std::int64_t> inputs;std::int64_t target;};
struct DiscreteDatasetLimits {std::size_t maxRows=100000,maxInputValues=10000000,maxNodeVisitsPerLoss=10000000,maxOperationsPerLoss=10000000,maxIterationsPerLoss=1000000;};
struct DiscreteRegressionMetrics {Fitness loss;std::size_t rowsAttempted=0,mismatches=0,invalidRows=0,nodeVisits=0,operations=0,iterations=0,iterationOperations=0;};
// Immutable mathematical measurement adapter. Default null retains the exact D1/D2 path.
class DiscreteMeasurement {
public:
    virtual ~DiscreteMeasurement()=default;
    virtual std::string identity() const=0;
    virtual bool parallelEvaluationSafe() const noexcept{return false;}
    virtual DiscreteEvaluationResult evaluate(const DiscreteExpression&,std::span<const std::int64_t>,DiscreteEvaluationLimits,const ExecutionContext*) const=0;
};
class DiscreteRegressionProblem final {
public:
    DiscreteRegressionProblem(std::size_t dimensions,const std::vector<DiscreteRegressionRow>& rows,DiscreteExpressionLimits e={},DiscreteEvaluationLimits evaluation={},DiscreteDatasetLimits data={},std::shared_ptr<const DiscreteMeasurement> measurement={})
        :dimensions_(dimensions),limits_(e),evaluation_(evaluation),data_(data),measurement_(std::move(measurement)) {
        genetik_discrete_detail::validateLimits(e);
        if(evaluation.maxIterationDepth>128||evaluation.maxIterationsPerOperation>1000000000||evaluation.maxAggregateIterations>1000000000)throw std::invalid_argument("invalid iteration evaluation limits");
        if(rows.empty()||!data.maxRows||!data.maxInputValues||!data.maxNodeVisitsPerLoss||!data.maxOperationsPerLoss||!evaluation.maxWorkspaceBytes)throw std::invalid_argument("invalid discrete data or limits");
        if(data.maxNodeVisitsPerLoss>1000000000||data.maxOperationsPerLoss>1000000000||data.maxIterationsPerLoss>1000000000)throw std::invalid_argument("discrete loss work cap exceeds D1 bound");
        genetik_discrete_detail::limit(dimensions,e.maxVariables,"discrete schema limit");
        genetik_discrete_detail::limit(rows.size(),std::min(data.maxRows,std::size_t{1000000}),"discrete row limit");
        if(dimensions&&rows.size()>data.maxInputValues/dimensions)throw std::length_error("discrete dataset storage limit");
        for(const auto& row:rows)if(row.inputs.size()!=dimensions)throw std::invalid_argument("discrete row dimension mismatch");
        if(measurement_){const auto id=measurement_->identity();if(id.empty()||id.size()>1024*1024)throw std::invalid_argument("measurement identity must be nonempty and bounded");}
        rows_=rows;
    }
    static std::shared_ptr<const DiscreteRegressionProblem> create(std::size_t dimensions,const std::vector<DiscreteRegressionRow>& rows,DiscreteExpressionLimits e={},DiscreteEvaluationLimits v={},DiscreteDatasetLimits d={},std::shared_ptr<const DiscreteMeasurement> measurement={}) {return std::make_shared<const DiscreteRegressionProblem>(dimensions,rows,e,v,d,std::move(measurement));}
    std::string measurementIdentity() const{return measurement_?measurement_->identity():std::string{};}
    bool parallelEvaluationSafe() const noexcept{return !measurement_||measurement_->parallelEvaluationSafe();}
    std::size_t dimensions() const noexcept{return dimensions_;}
    std::size_t sampleCount() const noexcept{return rows_.size();}
    DiscreteExpressionLimits limits() const noexcept{return limits_;}
    DiscreteDatasetLimits datasetLimits() const noexcept{return data_;}
    const std::vector<DiscreteRegressionRow>& rows() const noexcept{return rows_;}
    DiscreteEvaluationLimits evaluationLimits() const noexcept{return evaluation_;}
    DiscreteExpression checkedExpression(const DiscreteExpression& e) const {
        if(e.dimensions()!=dimensions_||e.type()!=DiscreteType::Int64||!e.outerBindings().empty())throw std::invalid_argument("discrete regression requires matching Int64 expression");
        return {e.nodes(),dimensions_,limits_};
    }
    DiscreteRegressionMetrics measure(const DiscreteExpression& e,bool inspectAll=false,const ExecutionContext* execution=nullptr,DiscreteRegressionMetrics* consumed=nullptr) const {
        if(e.dimensions()!=dimensions_||e.type()!=DiscreteType::Int64||!e.outerBindings().empty())throw std::invalid_argument("discrete problem/result mismatch");
        DiscreteRegressionMetrics result;bool invalid=false;
        // External work is updated immediately, including on resource/cancellation exits.
        for(const auto& row:rows_){
            if(execution&&execution->stopRequested())throw EvaluationCancelled();
            ++result.rowsAttempted;if(consumed)++consumed->rowsAttempted;
            auto v=evaluation_;v.maxNodeVisits=std::min(v.maxNodeVisits,data_.maxNodeVisitsPerLoss-result.nodeVisits);v.maxOperations=std::min(v.maxOperations,data_.maxOperationsPerLoss-result.operations);
            v.maxAggregateIterations=std::min(v.maxAggregateIterations,data_.maxIterationsPerLoss-result.iterations);
            const auto prediction=measurement_?measurement_->evaluate(e,row.inputs,v,execution):e.evaluateWithCancellation(row.inputs,v,[&]{return execution&&execution->stopRequested();});result.nodeVisits+=prediction.nodeVisits;result.operations+=prediction.operations;result.iterations+=prediction.iterations;result.iterationOperations+=prediction.iterationOperations;
            if(consumed){consumed->nodeVisits+=prediction.nodeVisits;consumed->operations+=prediction.operations;consumed->iterations+=prediction.iterations;consumed->iterationOperations+=prediction.iterationOperations;}
            if(prediction.status==DiscreteEvaluationStatus::ResourceExhausted)throw DiscreteResourceExhausted(prediction.reason);
            if(prediction.status==DiscreteEvaluationStatus::Cancelled)throw EvaluationCancelled();
            if(!prediction.valid()){
                invalid=true;++result.invalidRows;if(consumed)++consumed->invalidRows;
                if(!inspectAll){result.loss=Fitness::invalid();return result;}
            }else if(prediction.value->integer()!=row.target)++result.mismatches;
        }
        result.loss=invalid?Fitness::invalid():Fitness::valid(static_cast<double>(result.mismatches));return result;
    }
private:
    std::size_t dimensions_;DiscreteExpressionLimits limits_;DiscreteEvaluationLimits evaluation_;DiscreteDatasetLimits data_;std::vector<DiscreteRegressionRow> rows_;std::shared_ptr<const DiscreteMeasurement> measurement_;
};
struct DiscreteTrainingWork {
    std::uint64_t lossAttempts=0,rowAttempts=0,nodeVisits=0,operations=0,invalidLosses=0,invalidRows=0,iterations=0,iterationOperations=0;
    bool operator==(const DiscreteTrainingWork&) const = default;
};
class DiscreteTrainingObjective {
    struct Admission {
        std::mutex mutex;std::uint64_t maximum,used=0;
        explicit Admission(std::uint64_t n):maximum(n){}
    };
public:
    explicit DiscreteTrainingObjective(std::shared_ptr<const DiscreteRegressionProblem> p,std::uint64_t maximum=100000,const ExecutionContext* execution=nullptr):problem_(std::move(p)),admission_(std::make_shared<Admission>(maximum)),execution_(execution) {
        if(!problem_)throw std::invalid_argument("discrete objective problem required");
        // Guarantees supplemental counters cannot wrap even if every attempt uses its local cap.
        if(maximum>1000000000)throw std::invalid_argument("discrete objective allowance exceeds D1 cap");
    }
    DiscreteTrainingObjective(const DiscreteTrainingObjective&)=delete;
    DiscreteTrainingObjective& operator=(const DiscreteTrainingObjective&)=delete;
    const auto& problem() const noexcept{return problem_;}
    const DiscreteTrainingWork& work() const noexcept{return work_;}
    void restoreWork(const DiscreteTrainingWork& work) {
        if(work_!=DiscreteTrainingWork{}||admission_->used||work.lossAttempts>admission_->maximum||work.invalidLosses>work.lossAttempts||work.invalidRows>work.rowAttempts)throw std::invalid_argument("invalid discrete work restore");
        const auto d=problem_->datasetLimits();const auto safe=[](std::uint64_t used,std::uint64_t count,std::uint64_t cap){return !count?used==0:used/count<=cap && (used/count<cap||used%count==0);};
        if(!safe(work.rowAttempts,work.lossAttempts,problem_->sampleCount())||!safe(work.nodeVisits,work.lossAttempts,d.maxNodeVisitsPerLoss)||!safe(work.operations,work.lossAttempts,d.maxOperationsPerLoss)||!safe(work.iterations,work.lossAttempts,d.maxIterationsPerLoss)||work.iterationOperations>work.nodeVisits)throw std::invalid_argument("invalid discrete work counters");
        work_=work;admission_->used=work.lossAttempts;
    }
    std::uint64_t remaining() const {std::lock_guard lock(admission_->mutex);return admission_->maximum-admission_->used;}
    std::shared_ptr<DiscreteTrainingObjective> fork(ExecutionContext& execution) const {
        auto local=std::make_shared<DiscreteTrainingObjective>(problem_,admission_->maximum);local->admission_=admission_;local->execution_=&execution;return local;
    }
    void merge(const DiscreteTrainingObjective& local){
        if(local.admission_!=admission_||local.problem_!=problem_)throw std::invalid_argument("discrete worker context mismatch");
        const auto add=[](std::uint64_t& a,std::uint64_t b){if(b>std::numeric_limits<std::uint64_t>::max()-a)throw std::overflow_error("discrete counter overflow");a+=b;};
        add(work_.lossAttempts,local.work_.lossAttempts);add(work_.rowAttempts,local.work_.rowAttempts);add(work_.nodeVisits,local.work_.nodeVisits);add(work_.operations,local.work_.operations);add(work_.invalidLosses,local.work_.invalidLosses);add(work_.invalidRows,local.work_.invalidRows);add(work_.iterations,local.work_.iterations);add(work_.iterationOperations,local.work_.iterationOperations);
    }
    Fitness score(const DiscreteExpression& e) {
        {std::lock_guard lock(admission_->mutex);if(execution_&&execution_->stopRequested())throw EvaluationCancelled();if(admission_->used==admission_->maximum)throw DiscreteLossBudgetExceeded();++admission_->used;}
        ++work_.lossAttempts;DiscreteRegressionMetrics used;
        const auto mergeUsed=[&]{
            const auto add=[](std::uint64_t& a,std::size_t b){if(b>std::numeric_limits<std::uint64_t>::max()-a)throw std::overflow_error("discrete counter overflow");a+=b;};
            add(work_.rowAttempts,used.rowsAttempted);add(work_.nodeVisits,used.nodeVisits);add(work_.operations,used.operations);add(work_.invalidRows,used.invalidRows);add(work_.iterations,used.iterations);add(work_.iterationOperations,used.iterationOperations);
        };
        DiscreteRegressionMetrics result;
        try{result=problem_->measure(e,false,execution_,&used);}catch(...){mergeUsed();throw;}
        mergeUsed();if(result.loss.state()==FitnessState::Invalid)++work_.invalidLosses;return result.loss;
    }
private:std::shared_ptr<const DiscreteRegressionProblem> problem_;std::shared_ptr<Admission> admission_;DiscreteTrainingWork work_;const ExecutionContext* execution_=nullptr;
};
class DiscreteExpressionCandidate : public EvaluatedCandidate {
    template<class C> friend struct CandidateCheckpointCodec;
public:
    DiscreteExpressionCandidate(const DiscreteExpression& e,std::shared_ptr<const DiscreteRegressionProblem> p,std::shared_ptr<DiscreteTrainingObjective> objective={})
        :problem_(require(std::move(p))),expression_(problem_->checkedExpression(e)),text_(expression_.toString()),objective_(std::move(objective)){
        if(objective_&&objective_->problem()!=problem_)throw std::invalid_argument("discrete preparation context mismatch");
    }
    DiscreteExpressionCandidate(const DiscreteExpressionCandidate&)=default;
    DiscreteExpressionCandidate(DiscreteExpressionCandidate&&) noexcept=default;
    DiscreteExpressionCandidate& operator=(DiscreteExpressionCandidate other) noexcept {swap(other);return *this;}
    void swap(DiscreteExpressionCandidate& c) noexcept {swapEvaluation(c);problem_.swap(c.problem_);expression_.swap(c.expression_);text_.swap(c.text_);objective_.swap(c.objective_);parent_.swap(c.parent_);}
    void replaceExpression(const DiscreteExpression& e){if(!problem_)throw std::logic_error("moved-from discrete candidate");auto checked=problem_->checkedExpression(e);auto text=checked.toString();expression_.swap(checked);text_.swap(text);invalidateFitness();}
    const DiscreteExpression& expression() const noexcept{return expression_;}
    const std::string& text() const noexcept{return text_;}
    const auto& problem() const noexcept{return problem_;}
    std::shared_ptr<const void> problemIdentity() const noexcept override{return objective_?std::shared_ptr<const void>(objective_):std::shared_ptr<const void>(problem_);}
    bool parallelEvaluationSafe() const noexcept override{return typeid(*this)==typeid(DiscreteExpressionCandidate)&&problem_->parallelEvaluationSafe();}
    void beginParallelEvaluation(ExecutionContext& execution) override {if(objective_){auto local=objective_->fork(execution);parent_=objective_;objective_=std::move(local);}}
    void finishParallelEvaluation() override {if(parent_){auto local=std::move(objective_);objective_=std::move(parent_);objective_->merge(*local);}}
    std::string stats() override{return text_;}
protected:Fitness evaluate() const override{if(!problem_)throw std::logic_error("moved-from discrete candidate");return objective_?objective_->score(expression_):problem_->measure(expression_).loss;}
private:
    static std::shared_ptr<const DiscreteRegressionProblem> require(std::shared_ptr<const DiscreteRegressionProblem> p){if(!p)throw std::invalid_argument("discrete problem required");return p;}
    std::shared_ptr<const DiscreteRegressionProblem> problem_;DiscreteExpression expression_;std::string text_;std::shared_ptr<DiscreteTrainingObjective> objective_,parent_;
};
struct DiscreteRegressionOrdering {
    void validateDirection(ObjectiveDirection d) const {if(d!=ObjectiveDirection::Minimize)throw std::invalid_argument("discrete loss minimizes");}
    bool better(const DiscreteExpressionCandidate& a,const DiscreteExpressionCandidate& b,ObjectiveDirection d) const {
        validateDirection(d);if(a.fitness()!=b.fitness())return a.fitness()<b.fitness();
        if(a.expression().nodeCount()!=b.expression().nodeCount())return a.expression().nodeCount()<b.expression().nodeCount();
        if(a.expression().depth()!=b.expression().depth())return a.expression().depth()<b.expression().depth();
        return a.text()<b.text();
    }
};
