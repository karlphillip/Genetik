#pragma once
#include "TypedRegression.hpp"

// A coefficient edit returns a newly validated value. Node/shape/topology identity is preserved.
inline TypedExpression withTypedCoefficient(const TypedExpression& expression,std::size_t nodeIndex,
                                            std::size_t coefficient,double value) {
    auto nodes=expression.nodes();
    const auto& old=nodes.at(nodeIndex).constantValue();
    if(coefficient>=old.shape().coefficients())throw std::out_of_range("typed constant coefficient outside node");
    std::vector<double> coefficients(old.coefficients().begin(),old.coefficients().end());coefficients[coefficient]=value;
    const auto shape=old.shape();
    auto replacement=shape.kind()==TypedValueKind::Scalar?TypedValue::scalar(value):
        shape.kind()==TypedValueKind::Vector?TypedValue::vector(coefficients,expression.limits().values):
        TypedValue::matrix(shape.rows(),shape.columns(),coefficients,expression.limits().values);
    nodes[nodeIndex]=TypedExpressionNode::constant(replacement);
    return TypedExpression(nodes,expression.schema(),expression.limits());
}
struct TypedConstantFitResult {
    TypedExpression expression;
    Fitness loss,startingLoss;
    bool improved=false;
    ConstantFitStop stop=ConstantFitStop::SweepLimit;
    std::uint64_t lossAttempts=0,sweeps=0,proposals=0;
};
inline TypedConstantFitResult fitTypedConstants(const TypedExpression& input,TypedTrainingObjective& objective,
                                               const ConstantFitConfig& config={}) {
    validateConstantFitConfigForLimit(config,objective.problem()->limits().maxAbsConstant);
    (void)objective.problem()->checkedExpression(input);
    std::vector<std::pair<std::size_t,std::size_t>> parameters;
    const auto nodes=input.nodes();
    for(std::size_t n=0;n<nodes.size();++n)if(nodes[n].kind()==TypedExpressionKind::Constant) {
        const auto coefficients=nodes[n].constantValue().coefficients();
        for(std::size_t i=0;i<coefficients.size();++i) {
            if(coefficients[i]<config.lower || coefficients[i]>config.upper)
                throw std::invalid_argument("starting typed coefficient outside fitting bounds");
            parameters.emplace_back(n,i);
        }
    }
    return genetik_detail::fitCoordinates<TypedConstantFitResult>(input,objective,config,parameters,objective.work_,
        [](const TypedExpression& e,auto parameter){return e.nodes()[parameter.first].constantValue().coefficients()[parameter.second];},
        [](const TypedExpression& e,auto parameter,double value){return withTypedCoefficient(e,parameter.first,parameter.second,value);});
}
// Coordinator owns the parent accounting; each worker gets an independent objective/refiner.
class TypedConstantRefinement final:public TypedRegressionPreparation {
public:
    TypedConstantRefinement(std::shared_ptr<TypedTrainingObjective> objective,std::optional<ConstantFitConfig> config={},std::uint64_t interval=1)
        :objective_(std::move(objective)),config_(config),interval_(interval) {
        if(!objective_ || !interval_)throw std::invalid_argument("typed refinement requires objective and positive cadence");
        if(config_)validateConstantFitConfigForLimit(*config_,objective_->problem()->limits().maxAbsConstant);
    }
    bool parallelEvaluationSafe() const noexcept override{return objective_->parallelEvaluationSafe();}
    std::shared_ptr<TypedRegressionPreparation> forkParallel(ExecutionContext& execution) override {
        return std::make_shared<TypedConstantRefinement>(objective_->forkParallel(execution),config_,interval_);
    }
    void mergeParallel(const TypedRegressionPreparation& base) override {
        const auto& child=dynamic_cast<const TypedConstantRefinement&>(base);
        std::exception_ptr failure;try{objective_->mergeParallel(*child.objective_);}catch(...){failure=std::current_exception();}
        bool overflow=false;
        for(std::size_t i=0;i<statistics_.origins.size();++i) {
            auto& a=statistics_.origins[i];const auto& b=child.statistics_.origins[i];
            mergeTrainingCounter(a.requests,b.requests,overflow);mergeTrainingCounter(a.fits,b.fits,overflow);
            mergeTrainingCounter(a.skipped,b.skipped,overflow);mergeTrainingCounter(a.ordinary,b.ordinary,overflow);
            mergeTrainingCounter(a.baselines,b.baselines,overflow);mergeTrainingCounter(a.probes,b.probes,overflow);
            mergeTrainingCounter(a.rows,b.rows,overflow);mergeTrainingCounter(a.improved,b.improved,overflow);
        }
#define GENETIK_TYPED_STAT(field) mergeTrainingCounter(statistics_.field,child.statistics_.field,overflow);
        GENETIK_TYPED_STAT(skippedBySchedule) GENETIK_TYPED_STAT(completedImproved) GENETIK_TYPED_STAT(completedUnchanged)
        GENETIK_TYPED_STAT(noConstants) GENETIK_TYPED_STAT(invalidStarts) GENETIK_TYPED_STAT(abortedFits)
#undef GENETIK_TYPED_STAT
        for(std::size_t i=0;i<statistics_.terminations.size();++i)mergeTrainingCounter(statistics_.terminations[i],child.statistics_.terminations[i],overflow);
        if(child.rawInitialBest && (!rawInitialBest || *child.rawInitialBest<*rawInitialBest))rawInitialBest=child.rawInitialBest;
        if(failure)std::rethrow_exception(failure);
        if(overflow)throw std::overflow_error("typed preparation counters overflow");
    }
    PreparedTypedExpression prepare(const TypedExpression& expression,const TypedRegressionProblem& problem,PreparationContext context) override {
        if(&problem!=objective_->problem().get())throw std::invalid_argument("typed refinement problem identity differs");
        auto& stage=statistics_.origins.at(static_cast<std::size_t>(context.generation==0?PreparationOrigin::Initialization:context.origin));
        countTrainingWork(stage.requests);
        const auto before=objective_->work();
        auto record=[&]{const auto& after=objective_->work();stage.ordinary+=after.ordinaryLossAttempts-before.ordinaryLossAttempts;stage.baselines+=after.fittingBaselineAttempts-before.fittingBaselineAttempts;stage.probes+=after.fittingProbeAttempts-before.fittingProbeAttempts;stage.rows+=after.rowAttempts-before.rowAttempts;stage.improved+=after.fittingImproved-before.fittingImproved;};
        auto initial=[&](Fitness f){if(context.generation==0 && f.state()==FitnessState::Valid && (!rawInitialBest || f.score()<*rawInitialBest))rawInitialBest=f.score();};
        if(!config_ || context.generation%interval_!=0) {
            if(config_){countTrainingWork(statistics_.skippedBySchedule);countTrainingWork(stage.skipped);}
            Fitness loss;
            try{loss=objective_->measure(expression).loss;}
            catch(...){record();throw;}
            record();initial(loss);return {expression,loss};
        }
        countTrainingWork(stage.fits);
        auto result=[&]{try{return fitTypedConstants(expression,*objective_,*config_);}catch(...){record();countTrainingWork(statistics_.abortedFits);throw;}}();
        record();initial(result.startingLoss);countTrainingWork(statistics_.terminations.at(static_cast<std::size_t>(result.stop)));
        if(result.improved)countTrainingWork(statistics_.completedImproved);
        else if(result.stop==ConstantFitStop::NoConstants)countTrainingWork(statistics_.noConstants);
        else if(result.stop==ConstantFitStop::InvalidStart)countTrainingWork(statistics_.invalidStarts);
        else countTrainingWork(statistics_.completedUnchanged);
        return {std::move(result.expression),result.loss};
    }
    const PreparationStatistics& statistics() const noexcept{return statistics_;}
    std::optional<double> rawInitialBest;
private:
    std::shared_ptr<TypedTrainingObjective> objective_;
    const std::optional<ConstantFitConfig> config_;
    const std::uint64_t interval_;
    PreparationStatistics statistics_;
};
