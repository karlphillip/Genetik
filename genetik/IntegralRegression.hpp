#pragma once
#include "IntegralExpression.hpp"
#include "ScalarTemplateRegression.hpp"

struct IntegralDataLimits {
    std::size_t maxRows=4096,maxInputCoefficients=1048576;
    std::uint64_t maxWorstCaseSamples=16777216,maxWorstCaseExpressionOperations=4294967296ULL;
    std::uint64_t maxWorstCaseNodeVisits=4294967296ULL,maxWorstCaseBindings=4294967296ULL,maxWorstCaseQuadratureOperations=4294967296ULL;
};
struct IntegralRestrictions {
    std::optional<std::vector<std::size_t>> variables; // absent: all N external slots and local N
    std::optional<std::vector<ExpressionKind>> operators; // absent: all existing scalar operators
};
struct IntegralMetrics {
    Fitness loss=Fitness::invalid();std::optional<double> maxAbsoluteError;
    std::size_t rowsAttempted=0;bool complete=false,lossArithmeticFailure=false;
    std::array<std::size_t,7> outcomes{}; // IntegralStatus order
    std::vector<IntegralResult> predictions; // reporting only
    IntegralWork work;
};
class IntegralEvaluationFailure : public std::runtime_error {
public:
    explicit IntegralEvaluationFailure(const IntegralResult& result)
        :std::runtime_error(std::string("integral ")+integralStatusName(result.status)),status_(result.status),reason_(result.reason){}
    IntegralStatus status() const noexcept{return status_;} IntegralReason reason() const noexcept{return reason_;}
private:IntegralStatus status_;IntegralReason reason_;
};
class IntegralResourceExhausted : public IntegralEvaluationFailure {public:using IntegralEvaluationFailure::IntegralEvaluationFailure;};
class IntegralTrainingObjective;
// Candidate genomes have N+1 slots; observation rows have ONLY N external inputs.
class IntegralRegressionProblem final : public RegressionProblem {
public:
    IntegralRegressionProblem(std::size_t external,const Expression& lower,const Expression& upper,const std::vector<RegressionRow>& rows,
            ExpressionLimits limits={9,4,2},IntegralPolicy policy={},IntegralRestrictions restrictions={},IntegralDataLimits data={})
        :RegressionProblem(MeasurementSchema{schema(external),limits}),
         prototype_(external,lower,upper,Expression::constant(0,schema(external),limits),external,policy) {
        if(limits.maxNodes>policy.maxModelNodes-lower.nodeCount()-upper.nodeCount())throw std::length_error("integrand model node allowance");
        if(rows.empty() || rows.size()>data.maxRows || rows.size()>9007199254740992ULL ||
           (external && rows.size()>data.maxInputCoefficients/external))throw std::length_error("integral dataset storage allowance");
        auto product=[&](std::size_t per,std::uint64_t maximum){if(per && rows.size()>maximum/per)throw std::length_error("integral objective worst-case work allowance");};
        product(policy.maxIntegrandEvaluations,data.maxWorstCaseSamples);product(policy.maxExpressionOperations,data.maxWorstCaseExpressionOperations);
        product(policy.maxNodeVisits,data.maxWorstCaseNodeVisits);product(policy.maxBindingsValidated,data.maxWorstCaseBindings);product(policy.maxQuadratureOperations,data.maxWorstCaseQuadratureOperations);
        for(const auto& row:rows){if(row.inputs.size()!=external || !std::isfinite(row.target))throw std::invalid_argument("integral observation schema/target");for(double x:row.inputs)if(!std::isfinite(x))throw std::invalid_argument("nonfinite integral input");}
        if(restrictions.variables)variables_=*restrictions.variables;else for(std::size_t i=0;i<dimensions();++i)variables_.push_back(i);
        if(restrictions.operators)operators_=*restrictions.operators;else operators_={ExpressionKind::Add,ExpressionKind::Subtract,ExpressionKind::Multiply,ExpressionKind::Divide,ExpressionKind::Log,ExpressionKind::Sin,ExpressionKind::Cos,ExpressionKind::Exp,ExpressionKind::Sqrt,ExpressionKind::Powi};
        for(std::size_t i=0;i<variables_.size();++i)
            if(variables_[i]>=dimensions() || std::find(variables_.begin(),variables_.begin()+i,variables_[i])!=variables_.begin()+i)
                throw std::invalid_argument("integrand variables must be unique original indices");
        for(std::size_t i=0;i<operators_.size();++i)
            if(!expressionArity(operators_[i]) || std::find(operators_.begin(),operators_.begin()+i,operators_[i])!=operators_.begin()+i)
                throw std::invalid_argument("integrand operators must be distinct supported operations");
        rows_=rows;
    }
    static std::shared_ptr<const IntegralRegressionProblem> create(std::size_t n,const Expression& a,const Expression& b,const std::vector<RegressionRow>& rows,
            ExpressionLimits limits={9,4,2},IntegralPolicy policy={},IntegralRestrictions restrictions={},IntegralDataLimits data={}) {
        return std::make_shared<const IntegralRegressionProblem>(n,a,b,rows,limits,policy,std::move(restrictions),data);
    }
    bool parallelEvaluationSafe() const noexcept override{return true;}
    std::size_t externalDimensions() const noexcept{return prototype_.externalDimensions();}
    std::size_t sampleCount() const noexcept override{return rows_.size();}
    const std::vector<RegressionRow>& rows() const noexcept{return rows_;}
    const std::vector<std::size_t>& allowedVariables() const noexcept{return variables_;}
    const std::vector<ExpressionKind>& allowedOperators() const noexcept{return operators_;}
    IntegralPolicy policy() const noexcept{return prototype_.policy();}
    const Expression& lower() const noexcept{return prototype_.lower();}
    const Expression& upper() const noexcept{return prototype_.upper();}
    void validateIntegrand(const Expression& e) const {
        (void)checkedExpression(e);
        for(const auto& n:e.nodes()) {
            if(n.kind()==ExpressionKind::Variable && std::find(variables_.begin(),variables_.end(),n.variableIndex())==variables_.end())throw std::invalid_argument("forbidden integrand variable");
            if(expressionArity(n.kind()) && std::find(operators_.begin(),operators_.end(),n.kind())==operators_.end())throw std::invalid_argument("forbidden integrand operation");
        }
    }
    IntegralExpression model(const Expression& integrand) const {
        validateIntegrand(integrand);return IntegralExpression(externalDimensions(),lower(),upper(),integrand,externalDimensions(),policy());
    }
    IntegralMetrics assess(const Expression& e) const {IntegralWork w;return evaluate(e,true,w,nullptr,nullptr);}
    RegressionMetrics measure(const Expression& e,std::uint64_t* rows=nullptr) const override {
        IntegralWork w;auto m=evaluate(e,false,w,rows,nullptr);return {m.loss,m.maxAbsoluteError,m.rowsAttempted};
    }
private:
    friend class IntegralTrainingObjective;
    static std::size_t schema(std::size_t n){if(n==std::numeric_limits<std::size_t>::max())throw std::invalid_argument("integral schema overflow");return n+1;}
    IntegralMetrics evaluate(const Expression& e,bool all,IntegralWork& work,std::uint64_t* rows,const ExecutionContext* execution) const {
        const auto expression=model(e);IntegralMetrics result;genetik_detail::IntegralWorkspace workspace;
        double sum=0,maximum=0;bool valid=true;
        auto arithmetic=[&](ExpressionKind k,double a,double b){return genetik_detail::scalarBinary(k,a,b,[&]{countTrainingWork(work.lossArithmeticOperations);});};
        for(const auto& row:rows_) {
            ++result.rowsAttempted;if(rows)countTrainingWork(*rows);
            IntegralWork delta;IntegralResult prediction;
            bool merged=false;
            auto merge=[&]{merged=true;bool overflow=false;mergeIntegralWork(work,delta,overflow);if(overflow)throw std::overflow_error("integral objective accounting exhausted");};
            try{prediction=workspace.evaluate(expression,row.inputs,delta,execution);merge();}catch(...){if(!merged)merge();throw;}
            ++result.outcomes[static_cast<std::size_t>(prediction.status)];
            if(all)result.predictions.push_back(prediction);
            if(!prediction.accepted()) {
                valid=false;
                if(!all) {
                    if(prediction.status==IntegralStatus::Cancelled)throw EvaluationCancelled();
                    if(prediction.status==IntegralStatus::ResourceLimit)throw IntegralResourceExhausted(prediction);
                    if(prediction.status!=IntegralStatus::MathematicalInvalidity)throw IntegralEvaluationFailure(prediction);
                    result.work=work;return result;
                }
                continue;
            }
            const auto residual=arithmetic(ExpressionKind::Subtract,*prediction.estimate,row.target);
            std::optional<double> square,next;
            if(residual)square=arithmetic(ExpressionKind::Multiply,*residual,*residual);
            if(square)next=arithmetic(ExpressionKind::Add,sum,*square);
            if(!next) {
                result.lossArithmeticFailure=true;valid=false;if(!all){result.work=work;return result;}
            }else {sum=*next;maximum=std::max(maximum,std::abs(*residual));}
        }
        if(valid){const auto mse=arithmetic(ExpressionKind::Divide,sum,static_cast<double>(rows_.size()));if(mse){result.loss=Fitness::valid(*mse);result.maxAbsoluteError=maximum;}else result.lossArithmeticFailure=true;}
        result.complete=true;result.work=work;return result;
    }
    const IntegralExpression prototype_;
    std::vector<RegressionRow> rows_;std::vector<std::size_t> variables_;std::vector<ExpressionKind> operators_;
};
class IntegralTrainingObjective : public TrainingObjective {
public:
    IntegralTrainingObjective(std::shared_ptr<const IntegralRegressionProblem> p,std::uint64_t maximum=UINT64_MAX,std::shared_ptr<RefinementDiagnostics> diagnostics={})
        :TrainingObjective(p,maximum,std::move(diagnostics)),integrals_(std::move(p)){}
    bool parallelEvaluationSafe() const noexcept override{return typeid(*this)==typeid(IntegralTrainingObjective) && !diagnostics();}
    const IntegralWork& integralWork() const noexcept{return work_;}
protected:
    std::shared_ptr<TrainingObjective> cloneForParallel() const override {
        if(typeid(*this)!=typeid(IntegralTrainingObjective))throw std::invalid_argument("derived integral objective requires its own worker factory");
        return std::make_shared<IntegralTrainingObjective>(integrals_,maximum());
    }
    void mergeWorker(const TrainingObjective& source) noexcept override {
        const auto* child=dynamic_cast<const IntegralTrainingObjective*>(&source);
        if(!child){wrongType_=true;return;}mergeIntegralWork(work_,child->work_,overflow_);
    }
    void validateMergedObjectiveWork() override {
        if(wrongType_)throw std::invalid_argument("integral worker factory type differs");
        if(overflow_)throw std::overflow_error("integral objective counter exhausted");
    }
    RegressionMetrics compute(const Expression& e) override {
        IntegralWork delta;bool merged=false;
        auto merge=[&]{merged=true;mergeIntegralWork(work_,delta,overflow_);validateMergedObjectiveWork();};
        try{auto m=integrals_->evaluate(e,false,delta,&rowAttemptsCounter(),executionContext());merge();return {m.loss,m.maxAbsoluteError,m.rowsAttempted};}
        catch(...){if(!merged)merge();throw;}
    }
private:const std::shared_ptr<const IntegralRegressionProblem> integrals_;IntegralWork work_;bool overflow_=false,wrongType_=false;
};
struct IntegralRegressionResult {SymbolicRegressionResult search;IntegralWork integralWork;};
inline IntegralRegressionResult integralRegression(std::shared_ptr<const IntegralRegressionProblem> problem,
      const SymbolicRegressionConfig& config,RandomContext& random,std::shared_ptr<IntegralTrainingObjective> objective={}) {
    if(!problem)throw std::invalid_argument("integral problem required");
    if(!objective)objective=std::make_shared<IntegralTrainingObjective>(problem,config.maxTrainingLossAttempts,config.diagnostics);
    ScalarExpressionTemplate site(Expression::constant(0,problem->dimensions(),problem->limits()),0,problem->limits(),problem->limits());
    auto domain=ScalarTemplateProblem::create(site,problem,problem->allowedVariables(),problem->allowedOperators());
    auto result=scalarTemplateRegression(domain,config,random,objective);return {std::move(result),objective->integralWork()};
}
