#pragma once
#include "ExpressionDerivative.hpp"
#include "SymbolicRegression.hpp"

// Mutable construction descriptors; the problem always copies and validates them.
struct ScalarObservationChannel {
    std::optional<std::size_t> partial; // absent: ordinary value
    std::vector<RegressionRow> observations;
    double scale = 1, weight = 1;
};
struct ObservationLimits {
    std::size_t maxDimensions=64, maxChannels=65, maxObservations=65536, maxInputCoefficients=1048576;
};
struct ObservationEvaluationLimits {
    std::uint64_t maxObservations=65536, maxAggregationOperations=1000000;
    std::size_t maxValueNodes=65536, maxValueOperations=1000000, maxValueWorkspaceBytes=8*1024*1024;
    DerivativeLimits partial;
};
enum class ObservationStatus { Success, PrimalInvalidity, DerivativeUnavailable, LossArithmeticFailure, ResourceLimit };
enum class ObservationResource { Observations, Aggregation, ValuePlan, Partial };
struct ObservationWork {
    std::uint64_t observations=0,valueEvaluations=0,partialEvaluations=0,primalOperations=0;
    std::uint64_t derivativeArithmetic=0,derivativeMathCalls=0,partialNodeVisits=0,aggregationOperations=0;
    std::uint64_t primalInvalidObservations=0,unavailableObservations=0,lossArithmeticFailures=0,resourceStops=0;
    bool operator==(const ObservationWork&) const=default;
};
inline void mergeObservationWork(ObservationWork& a,const ObservationWork& b,bool& overflow) noexcept {
#define GENETIK_OBSERVATION_ADD(field) mergeTrainingCounter(a.field,b.field,overflow);
    GENETIK_OBSERVATION_ADD(observations) GENETIK_OBSERVATION_ADD(valueEvaluations)
    GENETIK_OBSERVATION_ADD(partialEvaluations) GENETIK_OBSERVATION_ADD(primalOperations)
    GENETIK_OBSERVATION_ADD(derivativeArithmetic) GENETIK_OBSERVATION_ADD(derivativeMathCalls)
    GENETIK_OBSERVATION_ADD(partialNodeVisits) GENETIK_OBSERVATION_ADD(aggregationOperations)
    GENETIK_OBSERVATION_ADD(primalInvalidObservations) GENETIK_OBSERVATION_ADD(unavailableObservations)
    GENETIK_OBSERVATION_ADD(lossArithmeticFailures) GENETIK_OBSERVATION_ADD(resourceStops)
#undef GENETIK_OBSERVATION_ADD
}
struct ObservationChannelMetrics {
    std::optional<double> normalizedMean, rawMSE, maxAbsoluteError;
    std::size_t observationsAttempted=0,primalInvalid=0,derivativeUnavailable=0;
    std::size_t lossArithmeticFailures=0,resourceStops=0; // events; may include channel reduction
};
struct ObservationMetrics {
    Fitness loss=Fitness::invalid();
    ObservationStatus status=ObservationStatus::Success;
    bool complete=false; // every requested observation inspected (not an accuracy claim)
    std::vector<ObservationChannelMetrics> channels;
    ObservationWork work;
    std::optional<std::size_t> failedChannel, failedObservation;
    std::optional<DerivativeFailure> derivativeFailure;
    std::optional<ObservationResource> resource;
    std::optional<DerivativeResource> partialResource;
};
class ObservationResourceExhausted : public std::runtime_error {
public:
    explicit ObservationResourceExhausted(const ObservationMetrics& m)
        :std::runtime_error("scalar observation objective resource exhausted"),resource_(m.resource),partial_(m.partialResource){}
    std::optional<ObservationResource> resource() const noexcept{return resource_;}
    std::optional<DerivativeResource> partialResource() const noexcept{return partial_;}
private:std::optional<ObservationResource> resource_;std::optional<DerivativeResource> partial_;
};

class DerivativeTrainingObjective;
class DerivativeRegressionProblem final : public RegressionProblem {
public:
    DerivativeRegressionProblem(std::size_t dimensions,const std::vector<ScalarObservationChannel>& channels,
                ExpressionLimits expressionLimits={31,6,8},ObservationLimits limits={},ObservationEvaluationLimits evaluation={})
        :RegressionProblem(MeasurementSchema{dimensions,expressionLimits}),evaluation_(evaluation) {
        if(!limits.maxDimensions || !limits.maxChannels || !limits.maxObservations || !limits.maxInputCoefficients ||
           dimensions>limits.maxDimensions || channels.empty() || channels.size()>limits.maxChannels)
            throw std::invalid_argument("invalid scalar observation dimensions/channels/limits");
        std::size_t coefficients=0;
        for(std::size_t c=0;c<channels.size();++c) {
            const auto& channel=channels[c];
            if(channel.observations.empty() || !std::isfinite(channel.scale) || channel.scale<=0 ||
               !std::isfinite(channel.weight) || channel.weight<=0 || (channel.partial && *channel.partial>=dimensions))
                throw std::invalid_argument("invalid scalar observation channel");
            for(std::size_t i=0;i<c;++i)if(channels[i].partial==channel.partial)throw std::invalid_argument("duplicate scalar observation channel");
            const auto count=channel.observations.size();
            if(count>9007199254740992ULL || count>limits.maxObservations-total_ || count>(limits.maxInputCoefficients-coefficients)/dimensions)
                throw std::length_error("scalar observation storage limit exceeded");
            total_+=count;coefficients+=count*dimensions;
            for(const auto& row:channel.observations) {
                if(row.inputs.size()!=dimensions || !std::isfinite(row.target))throw std::invalid_argument("invalid scalar observation row");
                for(double x:row.inputs)if(!std::isfinite(x))throw std::invalid_argument("nonfinite scalar observation input");
            }
        }
        channels_=channels; // detach all nested caller buffers, including rvalues
        std::sort(channels_.begin(),channels_.end(),[](const auto& a,const auto& b){
            if(!a.partial)return bool(b.partial);
            if(!b.partial)return false;
            return *a.partial<*b.partial;
        });
        double sum=0;
        for(const auto& c:channels_) {volatile double next=sum+c.weight;if(!std::isfinite(next))throw std::invalid_argument("observation weight sum overflow");sum=next;}
        for(const auto& c:channels_) {volatile double alpha=c.weight/sum;if(!std::isfinite(alpha) || alpha<=0)throw std::invalid_argument("observation weight normalization underflow");weights_.push_back(static_cast<double>(alpha));}
    }
    static std::shared_ptr<const DerivativeRegressionProblem> create(std::size_t dimensions,
          const std::vector<ScalarObservationChannel>& channels,ExpressionLimits expressions={31,6,8},
          ObservationLimits limits={},ObservationEvaluationLimits evaluation={}) {
        return std::make_shared<const DerivativeRegressionProblem>(dimensions,channels,expressions,limits,evaluation);
    }
    bool parallelEvaluationSafe() const noexcept override{return true;}
    std::size_t sampleCount() const noexcept override{return total_;}
    const std::vector<ScalarObservationChannel>& channels() const noexcept{return channels_;}
    const std::vector<double>& normalizedWeights() const noexcept{return weights_;}
    ObservationEvaluationLimits evaluationLimits() const noexcept{return evaluation_;}
    ObservationMetrics measureObservations(const Expression& e) const {ObservationWork w;return evaluate(e,evaluation_,false,w,nullptr);}
    // Independent reporting allowance; no objective admission, cache or training mutation.
    ObservationMetrics assess(const Expression& e,ObservationEvaluationLimits reporting={}) const {ObservationWork w;return evaluate(e,reporting,true,w,nullptr);}
    RegressionMetrics measure(const Expression& e,std::uint64_t* rowAttempts=nullptr) const override {
        ObservationWork w;auto m=evaluate(e,evaluation_,false,w,rowAttempts);
        if(m.status==ObservationStatus::ResourceLimit)throw ObservationResourceExhausted(m);
        return {m.loss,{},static_cast<std::size_t>(w.observations)};
    }
private:
    friend class DerivativeTrainingObjective;
    struct LimitStop {};
    ObservationMetrics evaluate(const Expression& e,const ObservationEvaluationLimits& bound,bool all,
                    ObservationWork& w,std::uint64_t* rowAttempts) const {
        if(e.variableCount()!=dimensions())throw std::invalid_argument("observation expression/schema differs");
        const auto l=limits();
        if(e.nodeCount()>l.maxNodes || e.depth()>l.maxDepth || e.arithmeticCost()>l.maxArithmeticOperations)
            throw std::length_error("observation expression limits exceeded");
        (void)checkedExpression(e);
        ObservationMetrics result;result.channels.resize(channels_.size());
        std::size_t currentChannel=0,currentRow=0;
        auto finish=[&]{result.work=w;result.complete=w.observations==total_;return result;};
        auto mark=[&](ObservationStatus s) {
            if(result.status==ObservationStatus::Success || s==ObservationStatus::ResourceLimit) {
                result.status=s;result.failedChannel=currentChannel;
                result.failedObservation=currentRow<channels_[currentChannel].observations.size()?std::optional<std::size_t>(currentRow):std::nullopt;
            }
        };
        auto stop=[&](ObservationResource resource) {
            countTrainingWork(w.resourceStops);mark(ObservationStatus::ResourceLimit);result.resource=resource;throw LimitStop{};
        };
        auto charge=[&]{if(w.aggregationOperations==bound.maxAggregationOperations)stop(ObservationResource::Aggregation);countTrainingWork(w.aggregationOperations);};
        auto arithmetic=[&](ExpressionKind kind,double a,double b) {return genetik_detail::scalarBinary(kind,a,b,charge);};
        auto nonzeroUnderflow=[](double before,double after){return before!=0 && after==0;};
        double combined=0;
        try {
            for(currentChannel=0;currentChannel<channels_.size();++currentChannel) {
                const auto& channel=channels_[currentChannel];auto& report=result.channels[currentChannel];
                double sum=0,rawSum=0,rawMax=0;bool normalizedOkay=true,rawMseOkay=true,rawErrorOkay=true;
                for(currentRow=0;currentRow<channel.observations.size();++currentRow) {
                    if(w.observations==bound.maxObservations)stop(ObservationResource::Observations);
                    countTrainingWork(w.observations);++report.observationsAttempted;
                    if(rowAttempts)countTrainingWork(*rowAttempts);
                    const auto& row=channel.observations[currentRow];std::optional<double> prediction;
                    if(channel.partial) {
                        countTrainingWork(w.partialEvaluations);
                        const auto d=evaluatePartial(e,row.inputs,*channel.partial,bound.partial);
                        const auto& dw=d.work();
                        bool overflow=false;ObservationWork delta;
                        delta.primalOperations=dw.primalOperations;delta.derivativeArithmetic=dw.derivativeArithmetic;
                        delta.derivativeMathCalls=dw.derivativeMathCalls;delta.partialNodeVisits=dw.nodesVisited;
                        mergeObservationWork(w,delta,overflow);if(overflow)throw std::overflow_error("observation work counter exhausted");
                        if(d.status()==DerivativeStatus::ResourceLimit) {
                            ++report.resourceStops;countTrainingWork(w.resourceStops);mark(ObservationStatus::ResourceLimit);result.resource=ObservationResource::Partial;result.partialResource=d.exhaustedResource();
                        } else if(d.status()==DerivativeStatus::PrimalInvalidity){++report.primalInvalid;countTrainingWork(w.primalInvalidObservations);mark(ObservationStatus::PrimalInvalidity);}
                        else if(d.status()==DerivativeStatus::Unavailable) {
                            ++report.derivativeUnavailable;countTrainingWork(w.unavailableObservations);mark(ObservationStatus::DerivativeUnavailable);
                            if(!result.derivativeFailure)result.derivativeFailure=d.derivativeFailure();
                        } else prediction=d.derivative();
                    } else {
                        // Conservative whole-plan admission; ordinary interpretation is unchanged.
                        if(e.nodeCount()>bound.maxValueNodes || e.arithmeticCost()>bound.maxValueOperations || e.nodeCount()>bound.maxValueWorkspaceBytes/sizeof(double)) {
                            ++report.resourceStops;countTrainingWork(w.resourceStops);mark(ObservationStatus::ResourceLimit);result.resource=ObservationResource::ValuePlan;
                        } else {
                            countTrainingWork(w.valueEvaluations);
                            const auto p=e.evaluate(row.inputs);
                            bool overflow=false;mergeTrainingCounter(w.primalOperations,p.arithmeticOperations(),overflow);
                            if(overflow)throw std::overflow_error("observation primal work counter exhausted");
                            if(!p.valid()){++report.primalInvalid;countTrainingWork(w.primalInvalidObservations);mark(ObservationStatus::PrimalInvalidity);}else prediction=p.value();
                        }
                    }
                    if(!prediction) {
                        normalizedOkay=rawMseOkay=rawErrorOkay=false;
                        if(!all)return finish();
                        continue;
                    }
                    auto residual=arithmetic(ExpressionKind::Subtract,*prediction,row.target);
                    if(all) {
                        if(!residual)rawMseOkay=rawErrorOkay=false;
                        else {
                            rawMax=std::max(rawMax,std::abs(*residual));
                            volatile double square=*residual * *residual;
                            volatile double next=rawSum+square;
                            if(!std::isfinite(square) || !std::isfinite(next) || nonzeroUnderflow(*residual,square))rawMseOkay=false;else rawSum=next;
                        }
                    }
                    std::optional<double> scaled,square,next;
                    if(residual)scaled=arithmetic(ExpressionKind::Divide,*residual,channel.scale);
                    if(scaled && !nonzeroUnderflow(*residual,*scaled))square=arithmetic(ExpressionKind::Multiply,*scaled,*scaled);
                    if(square && !nonzeroUnderflow(*scaled,*square))next=arithmetic(ExpressionKind::Add,sum,*square);
                    if(!next) {
                        ++report.lossArithmeticFailures;countTrainingWork(w.lossArithmeticFailures);mark(ObservationStatus::LossArithmeticFailure);normalizedOkay=false;
                        if(!all)return finish();
                    } else sum=*next;
                }
                if(all) {
                    if(rawErrorOkay)report.maxAbsoluteError=rawMax;
                    if(rawMseOkay) {volatile double mean=rawSum/static_cast<double>(channel.observations.size());if(std::isfinite(mean) && !nonzeroUnderflow(rawSum,mean))report.rawMSE=mean;}
                }
                if(normalizedOkay) {
                    const auto mean=arithmetic(ExpressionKind::Divide,sum,static_cast<double>(channel.observations.size()));
                    std::optional<double> term,next;
                    if(mean && !nonzeroUnderflow(sum,*mean)) {report.normalizedMean=*mean;term=arithmetic(ExpressionKind::Multiply,weights_[currentChannel],*mean);}
                    if(term && !nonzeroUnderflow(*mean,*term))next=arithmetic(ExpressionKind::Add,combined,*term);
                    if(!next){++report.lossArithmeticFailures;countTrainingWork(w.lossArithmeticFailures);mark(ObservationStatus::LossArithmeticFailure);if(!all)return finish();}
                    else combined=*next;
                }
            }
            if(result.status==ObservationStatus::Success)result.loss=Fitness::valid(combined);
        } catch(const LimitStop&) {result.work=w;result.complete=false;return result;}
        return finish();
    }
    std::vector<ScalarObservationChannel> channels_;
    std::vector<double> weights_;
    std::size_t total_=0;
    const ObservationEvaluationLimits evaluation_;
};

// Worker-local supplemental accounting; only existing admission is shared.
class DerivativeTrainingObjective : public TrainingObjective {
public:
    explicit DerivativeTrainingObjective(std::shared_ptr<const DerivativeRegressionProblem> p,
                std::uint64_t maximum=std::numeric_limits<std::uint64_t>::max(),std::shared_ptr<RefinementDiagnostics> diagnostics={})
        :TrainingObjective(p,maximum,std::move(diagnostics)),observations_(std::move(p)) {}
    bool parallelEvaluationSafe() const noexcept override{return typeid(*this)==typeid(DerivativeTrainingObjective) && !diagnostics();}
    std::shared_ptr<const DerivativeRegressionProblem> observationProblem() const noexcept{return observations_;}
    const ObservationWork& observationWork() const noexcept{return supplemental_;}
protected:
    std::shared_ptr<TrainingObjective> cloneForParallel() const override {
        if(typeid(*this)!=typeid(DerivativeTrainingObjective))throw std::invalid_argument("derived observation objective needs its own worker factory");
        return std::make_shared<DerivativeTrainingObjective>(observations_,maximum());
    }
    void mergeWorker(const TrainingObjective& source) override {
        bool overflow=false;mergeObservationWork(supplemental_,dynamic_cast<const DerivativeTrainingObjective&>(source).supplemental_,overflow);
        if(overflow)throw std::overflow_error("parallel observation work counter exhausted");
    }
    RegressionMetrics compute(const Expression& e) override {
        ObservationWork delta;bool merged=false;
        auto merge=[&]{merged=true;bool overflow=false;mergeObservationWork(supplemental_,delta,overflow);if(overflow)throw std::overflow_error("observation work counter exhausted");};
        try {
            auto m=observations_->evaluate(e,observations_->evaluationLimits(),false,delta,&rowAttemptsCounter());merge();
            if(m.status==ObservationStatus::ResourceLimit)throw ObservationResourceExhausted(m);
            return {m.loss,{},static_cast<std::size_t>(delta.observations)};
        } catch(...) {if(!merged)merge();throw;}
    }
private:
    const std::shared_ptr<const DerivativeRegressionProblem> observations_;
    ObservationWork supplemental_;
};
struct DerivativeRegressionResult {SymbolicRegressionResult search;ObservationWork observationWork;};
inline DerivativeRegressionResult derivativeRegression(std::shared_ptr<const DerivativeRegressionProblem> problem,
        const SymbolicRegressionConfig& config,RandomContext& random,std::shared_ptr<DerivativeTrainingObjective> objective={}) {
    if(!problem)throw std::invalid_argument("observation problem required");
    if(!objective)objective=std::make_shared<DerivativeTrainingObjective>(problem,config.maxTrainingLossAttempts,config.diagnostics);
    if(objective->observationProblem()!=problem || objective->observationWork()!=ObservationWork{})throw std::invalid_argument("fresh matching observation objective required");
    auto result=symbolicRegression(problem,config,random,objective);
    return {std::move(result),objective->observationWork()};
}
