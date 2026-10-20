#pragma once
#include "TypedExpression.hpp"
#include "ConstantFitting.hpp" // shared admission, work units and preparation context

class TypedResourceExhausted : public std::runtime_error {
public: using std::runtime_error::runtime_error;
};
struct TypedRegressionRow { std::vector<TypedValue> inputs; TypedValue target; };
struct TypedDatasetLimits {
    std::size_t maxRows=100000, maxCoefficients=10000000;
    std::size_t maxArithmeticPerLoss=1000000000;
};
struct TypedRegressionMetrics {
    Fitness loss;
    std::optional<double> maxAbsoluteError;
    std::size_t rowsAttempted=0, invalidRows=0;
};
// Immutable copied data. Evaluation-only reporting is independent of candidate fitness.
class TypedRegressionProblem final {
public:
    TypedRegressionProblem(std::span<const TypedShape> schema, TypedShape output,
                           const std::vector<TypedRegressionRow>& rows,
                           TypedExpressionLimits limits={}, TypedDatasetLimits dataLimits={})
        : schema_(schema.begin(),schema.end()), output_(output), limits_(limits), dataLimits_(dataLimits) {
        using namespace genetik_typed_detail;
        if(rows.empty() || !dataLimits.maxRows || !dataLimits.maxCoefficients)
            throw std::invalid_argument("typed dataset must be nonempty with positive storage limits");
        bound(rows.size(),dataLimits.maxRows,"typed dataset row limit");
        validateShape(output,limits.values);
        // Validate schema/configuration without requiring an output terminal or constant allowance.
        std::vector<TypedExpressionNode> probe;
        if(schema_.empty()) {
            // Configuration-only validation with a temporary permissive constant allowance.
            auto config=limits; config.maxConstantNodes=std::max(std::size_t{1},config.maxConstantNodes);
            config.maxConstantCoefficients=std::max(std::size_t{1},config.maxConstantCoefficients);
            (void)TypedExpression::constant(TypedValue::scalar(0),schema_,config);
        } else (void)TypedExpression::variable(0,schema_,limits);
        std::size_t coefficients=0;
        for(const auto& row:rows) {
            if(row.inputs.size()!=schema_.size() || row.target.shape()!=output_)
                throw std::invalid_argument("typed dataset binding/output shape mismatch");
            for(std::size_t i=0;i<row.inputs.size();++i) {
                if(row.inputs[i].shape()!=schema_[i])throw std::invalid_argument("typed dataset input shape mismatch");
                coefficients=add(coefficients,row.inputs[i].shape().coefficients(),"typed dataset size overflow");
                for(double v:row.inputs[i].coefficients())if(!std::isfinite(v))throw std::invalid_argument("nonfinite typed input");
            }
            for(double v:row.target.coefficients())if(!std::isfinite(v))throw std::invalid_argument("nonfinite typed target");
            coefficients=add(coefficients,output_.coefficients(),"typed dataset size overflow");
            bound(coefficients,dataLimits.maxCoefficients,"typed dataset coefficient limit");
        }
        components_=multiply(rows.size(),output_.coefficients(),"typed loss denominator overflow");
        rows_=rows;
    }
    static std::shared_ptr<const TypedRegressionProblem> create(std::span<const TypedShape> schema,TypedShape output,
            const std::vector<TypedRegressionRow>& rows,TypedExpressionLimits limits={},TypedDatasetLimits dataLimits={}) {
        return std::make_shared<const TypedRegressionProblem>(schema,output,rows,limits,dataLimits);
    }
    const std::vector<TypedShape>& schema() const noexcept{return schema_;}
    TypedShape outputShape() const noexcept{return output_;}
    std::size_t sampleCount() const noexcept{return rows_.size();}
    TypedExpressionLimits limits() const noexcept{return limits_;}
    TypedDatasetLimits datasetLimits() const noexcept{return dataLimits_;}
    TypedExpression checkedExpression(const TypedExpression& e) const {
        if(e.schema()!=schema_ || e.shape()!=output_)throw std::invalid_argument("typed expression/problem schema or output differs");
        auto checked=TypedExpression(e.nodes(),schema_,limits_);
        using namespace genetik_typed_detail;
        const auto perRow=add(checked.arithmeticCost(),multiply(3,output_.coefficients(),"typed loss work overflow"),"typed loss work overflow");
        bound(add(multiply(rows_.size(),perRow,"typed loss work overflow"),1,"typed loss work overflow"),
              dataLimits_.maxArithmeticPerLoss,"typed full loss plan limit");
        return checked;
    }
    TypedRegressionMetrics measure(const TypedExpression& expression, std::uint64_t* rowWork=nullptr,
            std::uint64_t* arithmeticWork=nullptr, bool inspectAllRows=false,
            std::size_t maximumArithmetic=std::numeric_limits<std::size_t>::max()) const {
        if(expression.schema()!=schema_ || expression.shape()!=output_)
            throw std::invalid_argument("typed expression/problem schema or output differs");
        std::size_t used=0;
        maximumArithmetic=std::min(maximumArithmetic,dataLimits_.maxArithmeticPerLoss);
        auto charge=[&](std::size_t n) {
            if(n>maximumArithmetic-used)throw TypedResourceExhausted("typed loss local arithmetic allowance exhausted");
            used+=n;
            if(arithmeticWork) {
                if(n>std::numeric_limits<std::uint64_t>::max()-*arithmeticWork)throw std::overflow_error("typed arithmetic counter exhausted");
                *arithmeticWork+=n;
            }
        };
        TypedRegressionMetrics result;
        double sum=0,maximum=0;
        bool invalid=false;
        for(const auto& row:rows_) {
            ++result.rowsAttempted;if(rowWork)countTrainingWork(*rowWork);
            const auto prediction=expression.evaluate(row.inputs,{maximumArithmetic-used,limits_.maxTemporaryCoefficients});
            if(prediction.status()==TypedEvaluationStatus::ResourceLimit)
                throw TypedResourceExhausted("typed prediction resource allowance exhausted");
            charge(prediction.arithmeticOperations());
            if(!prediction.valid()) {
                invalid=true;++result.invalidRows;
                if(!inspectAllRows)return {Fitness::invalid(),{},result.rowsAttempted,result.invalidRows};
                continue;
            }
            // After loss invalidity, reporting still evaluates every remaining prediction.
            if(invalid)continue;
            const auto values=prediction.value().coefficients();const auto targets=row.target.coefficients();
            for(std::size_t i=0;i<values.size();++i) {
                charge(1);volatile double residual=values[i]-targets[i];
                if(!std::isfinite(residual)){invalid=true;break;}
                charge(1);volatile double square=residual*residual;
                if(!std::isfinite(square)){invalid=true;break;}
                charge(1);volatile double next=sum+square;
                if(!std::isfinite(next)){invalid=true;break;}
                sum=next;maximum=std::max(maximum,std::abs(static_cast<double>(residual)));
            }
            if(invalid && !inspectAllRows)return {Fitness::invalid(),{},result.rowsAttempted,result.invalidRows};
        }
        if(invalid)return {Fitness::invalid(),{},result.rowsAttempted,result.invalidRows};
        charge(1);volatile double mse=sum/static_cast<double>(components_);
        if(!std::isfinite(mse))return {Fitness::invalid(),{},result.rowsAttempted,result.invalidRows};
        return {Fitness::valid(mse),maximum,result.rowsAttempted,0};
    }
private:
    std::vector<TypedShape> schema_;
    const TypedShape output_;
    const TypedExpressionLimits limits_;
    const TypedDatasetLimits dataLimits_;
    std::vector<TypedRegressionRow> rows_;
    std::size_t components_=0;
};

class TypedTrainingObjective {
public:
    explicit TypedTrainingObjective(std::shared_ptr<const TypedRegressionProblem> problem,
            std::uint64_t maximum=std::numeric_limits<std::uint64_t>::max()) :problem_(std::move(problem)),maximum_(maximum) {
        if(!problem_)throw std::invalid_argument("typed objective requires problem");
    }
    virtual ~TypedTrainingObjective()=default;
    TypedTrainingObjective(const TypedTrainingObjective&)=delete;
    TypedTrainingObjective& operator=(const TypedTrainingObjective&)=delete;
    virtual bool parallelEvaluationSafe() const noexcept{return typeid(*this)==typeid(TypedTrainingObjective);}
    std::shared_ptr<TypedTrainingObjective> forkParallel(ExecutionContext& execution) {
        if(!parallelEvaluationSafe())throw std::invalid_argument("typed objective needs independent parallel factory");
        auto local=cloneForParallel();
        if(!local || local.get()==this || local.use_count()!=1 || local->problem_!=problem_ || local->maximum_!=maximum_ || local->work_!=TrainingWork{})
            throw std::invalid_argument("invalid typed objective parallel factory");
        if(!admission_)admission_=std::make_shared<TrainingLossAdmission>(maximum_,work_.lossAttempts);
        local->admission_=admission_;local->execution_=&execution;return local;
    }
    void mergeParallel(const TypedTrainingObjective& local) {
        bool overflow=false;
#define GENETIK_TYPED_ADD(field) mergeTrainingCounter(work_.field,local.work_.field,overflow);
        GENETIK_TYPED_ADD(lossAttempts) GENETIK_TYPED_ADD(rowAttempts) GENETIK_TYPED_ADD(fittingInvocations)
        GENETIK_TYPED_ADD(fittingLossAttempts) GENETIK_TYPED_ADD(proposals) GENETIK_TYPED_ADD(accepted)
        GENETIK_TYPED_ADD(invalid) GENETIK_TYPED_ADD(infeasible) GENETIK_TYPED_ADD(duplicates)
        GENETIK_TYPED_ADD(invalidLosses) GENETIK_TYPED_ADD(ordinaryLossAttempts)
        GENETIK_TYPED_ADD(fittingBaselineAttempts) GENETIK_TYPED_ADD(fittingProbeAttempts) GENETIK_TYPED_ADD(fittingImproved)
#undef GENETIK_TYPED_ADD
        mergeTrainingCounter(arithmetic_,local.arithmetic_,overflow);
        mergeWorker(local);
        if(overflow)throw std::overflow_error("typed worker accounting overflow");
    }
    TypedRegressionMetrics measure(const TypedExpression& expression,TrainingLossKind kind=TrainingLossKind::Ordinary) {
        if(kind!=TrainingLossKind::Ordinary && kind!=TrainingLossKind::FittingBaseline && kind!=TrainingLossKind::FittingProbe)
            throw std::invalid_argument("unknown typed loss kind");
        if(admission_)admission_->start(execution_);else if(work_.lossAttempts==maximum_)throw TrainingLossBudgetExceeded();
        countTrainingWork(work_.lossAttempts);
        if(kind==TrainingLossKind::Ordinary)countTrainingWork(work_.ordinaryLossAttempts);
        else {countTrainingWork(work_.fittingLossAttempts);countTrainingWork(kind==TrainingLossKind::FittingBaseline ? work_.fittingBaselineAttempts:work_.fittingProbeAttempts);}
        try{auto result=compute(expression);if(result.loss.state()==FitnessState::Invalid)countTrainingWork(work_.invalidLosses);return result;}
        catch(...){std::throw_with_nested(std::runtime_error("typed training loss attempt "+std::to_string(work_.lossAttempts)+" failed"));}
    }
    const auto& problem() const noexcept{return problem_;}
    std::uint64_t maximum() const noexcept{return maximum_;}
    std::uint64_t remaining() const{return admission_?admission_->remaining():maximum_-work_.lossAttempts;}
    const TrainingWork& work() const noexcept{return work_;}
    std::uint64_t arithmeticOperations() const noexcept{return arithmetic_;}
    // Fitter bookkeeping never charges a second objective attempt.
    TrainingWork& fittingWork() noexcept{return work_;}
protected:
    virtual std::shared_ptr<TypedTrainingObjective> cloneForParallel() const {
        if(typeid(*this)!=typeid(TypedTrainingObjective))throw std::invalid_argument("derived typed objective must supply factory");
        return std::make_shared<TypedTrainingObjective>(problem_,maximum_);
    }
    virtual void mergeWorker(const TypedTrainingObjective&) noexcept {}
    virtual TypedRegressionMetrics compute(const TypedExpression& expression){return problem_->measure(expression,&work_.rowAttempts,&arithmetic_);}
private:
    std::shared_ptr<const TypedRegressionProblem> problem_;
    const std::uint64_t maximum_;
    TrainingWork work_;
    std::uint64_t arithmetic_=0;
    std::shared_ptr<TrainingLossAdmission> admission_;
    const ExecutionContext* execution_=nullptr;
};
struct PreparedTypedExpression {TypedExpression expression;Fitness loss;};
class TypedRegressionPreparation {
public:
    virtual ~TypedRegressionPreparation()=default;
    virtual bool parallelEvaluationSafe() const noexcept{return false;}
    virtual std::shared_ptr<TypedRegressionPreparation> forkParallel(ExecutionContext&){throw std::invalid_argument("typed preparation needs independent parallel factory");}
    virtual void mergeParallel(const TypedRegressionPreparation&) {}
    virtual PreparedTypedExpression prepare(const TypedExpression&,const TypedRegressionProblem&,PreparationContext)=0;
};
class TypedScoring : public TypedRegressionPreparation {
public:
    explicit TypedScoring(std::shared_ptr<TypedTrainingObjective> objective):objective_(std::move(objective)) {
        if(!objective_)throw std::invalid_argument("typed scoring requires objective");
    }
    bool parallelEvaluationSafe() const noexcept override{return typeid(*this)==typeid(TypedScoring) && objective_->parallelEvaluationSafe();}
    std::shared_ptr<TypedRegressionPreparation> forkParallel(ExecutionContext& e) override{return std::make_shared<TypedScoring>(objective_->forkParallel(e));}
    void mergeParallel(const TypedRegressionPreparation& other) override{objective_->mergeParallel(*dynamic_cast<const TypedScoring&>(other).objective_);}
    PreparedTypedExpression prepare(const TypedExpression& e,const TypedRegressionProblem& p,PreparationContext) override {
        if(&p!=objective_->problem().get())throw std::invalid_argument("typed scoring problem identity differs");
        return {e,objective_->measure(e).loss};
    }
protected: std::shared_ptr<TypedTrainingObjective> objective_;
};

class TypedExpressionCandidate : public EvaluatedCandidate {
public:
    TypedExpressionCandidate(const TypedExpression& expression, std::shared_ptr<const TypedRegressionProblem> problem,
                        std::shared_ptr<TypedRegressionPreparation> preparation = {})
        : problem_(requireProblem(std::move(problem))), expression_(problem_->checkedExpression(expression)),
          text_(expression_.toString()), preparation_(std::move(preparation)) {}
    TypedExpressionCandidate(const TypedExpressionCandidate&) = default;
    TypedExpressionCandidate(TypedExpressionCandidate&&) noexcept = default;
    TypedExpressionCandidate& operator=(TypedExpressionCandidate other) noexcept { swap(other); return *this; }
    void swap(TypedExpressionCandidate& other) noexcept {
        swapEvaluation(other); problem_.swap(other.problem_); expression_.swap(other.expression_); text_.swap(other.text_); preparation_.swap(other.preparation_); parentPreparation_.swap(other.parentPreparation_); std::swap(context_,other.context_);
    }
    bool parallelEvaluationSafe() const noexcept override {
        return typeid(*this)==typeid(TypedExpressionCandidate) && (!preparation_ || preparation_->parallelEvaluationSafe());
    }
    void beginParallelEvaluation(ExecutionContext& execution) override {
        if(preparation_){
            auto local=preparation_->forkParallel(execution);
            if(!local || local.get()==preparation_.get() || local.use_count()!=1)
                throw std::invalid_argument("parallel preparation factory must return an exclusively owned independent instance");
            parentPreparation_=preparation_;preparation_=std::move(local);
        }
    }
    void finishParallelEvaluation() override {
        if(parentPreparation_){auto local=std::move(preparation_);preparation_=std::move(parentPreparation_);preparation_->mergeParallel(*local);}
    }
    // All throwing work precedes installation/invalidation, preserving old genome/cache on failure.
    void replaceExpression(const TypedExpression& expression) {
        if (!problem_) throw std::logic_error("moved-from expression candidate");
        auto ready = problem_->checkedExpression(expression);
        auto text = ready.toString();
        expression_.swap(ready); text_.swap(text); invalidateFitness();
    }
    // Population supplies the authoritative prospective generation before evaluation.
    // Origin is diagnostic only; it never decides cadence or permits fitness reuse.
    void setPreparationGeneration(std::uint64_t generation) noexcept {
        context_.generation=generation;
        if(generation==0)context_.origin=PreparationOrigin::Initialization;
        else if(context_.origin==PreparationOrigin::Initialization)context_.origin=PreparationOrigin::Unspecified;
    }
    void setPreparationOrigin(PreparationOrigin origin) noexcept {context_.origin=origin;}
    const TypedExpression& expression() const noexcept { return expression_; }
    const std::string& text() const noexcept { return text_; }
    std::shared_ptr<const TypedRegressionProblem> problem() const noexcept { return problem_; }
    std::shared_ptr<const void> problemIdentity() const noexcept final { return problem_; }
    std::string stats() override { return text_; }
protected:
    Fitness prepareEvaluation() override {
        if (!preparation_) return evaluate(); // preserve numerical/custom evaluator API
        auto ready = preparation_->prepare(expression_, *problem_,context_);
        if (ready.loss.state() == FitnessState::Unevaluated)
            throw std::logic_error("preparation returned unevaluated fitness");
        replaceExpression(ready.expression); // validate and detach before installation
        return ready.loss;
    }
    Fitness evaluate() const override {
        if (!problem_) throw std::logic_error("moved-from expression candidate");
        return problem_->measure(expression_).loss;
    }
private:
    static std::shared_ptr<const TypedRegressionProblem> requireProblem(std::shared_ptr<const TypedRegressionProblem> problem) {
        if (!problem) throw std::invalid_argument("regression problem is required");
        return problem;
    }
    std::shared_ptr<const TypedRegressionProblem> problem_;
    TypedExpression expression_;
    std::string text_;
    std::shared_ptr<TypedRegressionPreparation> preparation_;
    std::shared_ptr<TypedRegressionPreparation> parentPreparation_;
    PreparationContext context_;
};


struct TypedRegressionOrdering {
    void validateDirection(ObjectiveDirection direction) const {
        if(direction!=ObjectiveDirection::Minimize)throw std::invalid_argument("typed regression requires minimization");
    }
    template<class C> bool better(const C& a,const C& b,ObjectiveDirection direction) const {
        validateDirection(direction);
        if(a.fitness()!=b.fitness())return a.fitness()<b.fitness();
        const auto& x=a.expression();const auto& y=b.expression();
        if(x.nodeCount()!=y.nodeCount())return x.nodeCount()<y.nodeCount();
        if(x.depth()!=y.depth())return x.depth()<y.depth();
        if(x.constantCount()!=y.constantCount())return x.constantCount()<y.constantCount();
        if(x.constantCoefficients()!=y.constantCoefficients())return x.constantCoefficients()<y.constantCoefficients();
        return a.text()<b.text();
    }
};
