#pragma once
#include "ScalarExpressionTemplate.hpp"
#include "SymbolicRegression.hpp"

// Immutable model/data/structural restrictions. No transformed target dataset.
class ScalarTemplateProblem final {
public:
    ScalarTemplateProblem(const ScalarExpressionTemplate& scaffold,std::shared_ptr<const RegressionProblem> problem,
                std::vector<std::size_t> allowedVariables,std::vector<ExpressionKind> allowedOperators)
        :scaffold_(scaffold),problem_(std::move(problem)),variables_(std::move(allowedVariables)),operators_(std::move(allowedOperators)) {
        if(!problem_ || problem_->dimensions()!=scaffold_.variableCount())throw std::invalid_argument("template/problem schema differs");
        for(std::size_t i=0;i<variables_.size();++i) {
            if(variables_[i]>=problem_->dimensions() || std::find(variables_.begin(),variables_.begin()+i,variables_[i])!=variables_.begin()+i)
                throw std::invalid_argument("template variables must be unique full-schema indices");
        }
        for(std::size_t i=0;i<operators_.size();++i) {
            if(!expressionArity(operators_[i]) || std::find(operators_.begin(),operators_.begin()+i,operators_[i])!=operators_.begin()+i)
                throw std::invalid_argument("template operators must be distinct supported operations");
        }
        const auto a=problem_->limits(),b=scaffold_.modelLimits();
        if(a.maxNodes!=b.maxNodes || a.maxDepth!=b.maxDepth || a.maxAbsConstant!=b.maxAbsConstant || a.maxArithmeticOperations!=b.maxArithmeticOperations)
            throw std::invalid_argument("template and full problem must use the same model limits");
    }
    static std::shared_ptr<const ScalarTemplateProblem> create(const ScalarExpressionTemplate& t,
           std::shared_ptr<const RegressionProblem> p,std::vector<std::size_t> variables,std::vector<ExpressionKind> operators) {
        return std::make_shared<const ScalarTemplateProblem>(t,std::move(p),std::move(variables),std::move(operators));
    }
    const ScalarExpressionTemplate& expressionTemplate() const noexcept{return scaffold_;}
    std::shared_ptr<const RegressionProblem> problem() const noexcept{return problem_;}
    const std::vector<std::size_t>& allowedVariables() const noexcept{return variables_;}
    const std::vector<ExpressionKind>& allowedOperators() const noexcept{return operators_;}
    void validateSubstitution(const Expression& expression) const {
        if(expression.variableCount()!=problem_->dimensions())throw std::invalid_argument("substitution schema differs");
        const auto limits=scaffold_.effectiveSubstitutionLimits();
        if(expression.nodeCount()>limits.maxNodes || expression.depth()>limits.maxDepth || expression.arithmeticCost()>limits.maxArithmeticOperations)throw std::length_error("substitution exceeds effective model limits");
        for(const auto& n:expression.nodes()) {
            if(n.kind()==ExpressionKind::Constant && std::abs(n.constantValue())>limits.maxAbsConstant)throw std::length_error("substitution constant limit");
            if(n.kind()==ExpressionKind::Variable && std::find(variables_.begin(),variables_.end(),n.variableIndex())==variables_.end())throw std::invalid_argument("forbidden substitution variable");
            if(expressionArity(n.kind()) && std::find(operators_.begin(),operators_.end(),n.kind())==operators_.end())throw std::invalid_argument("forbidden substitution operator");
        }
    }
    Expression materialize(const Expression& hole) const {validateSubstitution(hole);return scaffold_.materialize(hole);}
    Expression substitution(const Expression& model) const {auto hole=scaffold_.substitution(model);validateSubstitution(hole);return hole;}
    void validateGrammar(const ExpressionGrammar& grammar) const {
        for(auto op:grammar.operators)if(std::find(operators_.begin(),operators_.end(),op)==operators_.end())throw std::invalid_argument("grammar requests forbidden template operator");
        genetik_detail::validateExpressionGrammar(variables_.size(),scaffold_.effectiveSubstitutionLimits(),grammar);
    }
private:
    const ScalarExpressionTemplate scaffold_;
    const std::shared_ptr<const RegressionProblem> problem_;
    const std::vector<std::size_t> variables_;
    const std::vector<ExpressionKind> operators_;
};
class ScalarTemplateParameters final:public ConstantParameterSelection {
public:
    explicit ScalarTemplateParameters(std::shared_ptr<const ScalarTemplateProblem> domain):domain_(std::move(domain)) {
        if(!domain_)throw std::invalid_argument("template domain required");
    }
    bool parallelEvaluationSafe() const noexcept override{return true;}
    std::vector<std::size_t> parameters(const Expression& model) const override {
        (void)domain_->substitution(model);return domain_->expressionTemplate().adjustableConstants(model);
    }
private:std::shared_ptr<const ScalarTemplateProblem> domain_;
};
class ScalarTemplateContext final:public RegressionCandidateContext {
public:
    ScalarTemplateContext(std::shared_ptr<const ScalarTemplateProblem> domain,std::shared_ptr<RegressionPreparation> preparation={})
        :domain_(std::move(domain)),preparation_(std::move(preparation)) {if(!domain_)throw std::invalid_argument("template domain required");}
    std::shared_ptr<const RegressionProblem> problem() const override{return domain_->problem();}
    std::shared_ptr<RegressionPreparation> preparation() const override{return preparation_;}
    void validate(const Expression& expression) const override{(void)domain_->substitution(expression);}
    bool parallelEvaluationSafe() const noexcept override{return true;}
    std::shared_ptr<const ScalarTemplateProblem> domain() const noexcept{return domain_;}
private:
    const std::shared_ptr<const ScalarTemplateProblem> domain_;
    const std::shared_ptr<RegressionPreparation> preparation_;
};
inline ConstantFitResult fitScalarTemplateConstants(const ScalarTemplateProblem& domain,const Expression& substitution,
                TrainingObjective& objective,const ConstantFitConfig& config={}) {
    if(objective.problem()!=domain.problem())throw std::invalid_argument("template fitting objective differs");
    validateConstantFitConfigForLimit(config,domain.expressionTemplate().effectiveSubstitutionLimits().maxAbsConstant);
    auto model=domain.materialize(substitution);
    return fitSelectedConstants(model,objective,domain.expressionTemplate().adjustableConstants(model),config);
}

class ScalarTemplateVariation {
public:
    ScalarTemplateVariation(std::shared_ptr<const ScalarTemplateContext> context,ExpressionGrammar grammar={})
        :context_(std::move(context)),grammar_(std::move(grammar)) {
        if(!context_)throw std::invalid_argument("template context required");
        context_->domain()->validateGrammar(grammar_);
    }
    VariationStatistics statistics() const noexcept{return statistics_;}
    void markElite(ExpressionCandidate& c) const{c.setPreparationOrigin(PreparationOrigin::Elite);}
    void markOffspring(ExpressionCandidate& c,const ExpressionCandidate& p) const{c.setPreparationOrigin(c.expression().sameRepresentation(p.expression())?PreparationOrigin::UnchangedOffspring:PreparationOrigin::ChangedOffspring);}
    ExpressionCandidate initialize(std::mt19937& rng) {
        count(statistics_.initializations);
        return candidate(random(rng,grammar_.initialMaxDepth));
    }
    ExpressionCandidate mutation(const ExpressionCandidate& parent,float rate,std::mt19937& rng) {
        require(parent);auto child=parent;
        if(std::uniform_real_distribution<float>(0,1)(rng)>=rate)return child;
        const auto hole=context_->domain()->substitution(parent.expression());
        for(std::size_t p=0;p<grammar_.proposalLimit;++p) {
            count(statistics_.mutationProposals);
            const auto root=index(hole.nodeCount(),rng);
            auto next=replaceExpressionSubtree(hole,root,random(rng,grammar_.mutationMaxDepth),limits());
            if(!next){count(statistics_.structuralRejections);continue;}
            child.replaceExpression(context_->domain()->materialize(*next));count(statistics_.mutationChanges,child.text()!=parent.text());return child;
        }
        count(statistics_.mutationFallbacks);return child;
    }
    void crossover(const ExpressionCandidate& father,const ExpressionCandidate& mother,ExpressionCandidate& a,ExpressionCandidate& b,float rate,std::mt19937& rng) {
        require(father);require(mother);auto left=father,right=mother;
        if(std::uniform_real_distribution<float>(0,1)(rng)<rate) {
            const auto f=context_->domain()->substitution(father.expression()),m=context_->domain()->substitution(mother.expression());bool accepted=false;
            for(std::size_t p=0;p<grammar_.proposalLimit;++p) {
                count(statistics_.crossoverProposals);const auto fr=index(f.nodeCount(),rng),mr=index(m.nodeCount(),rng);
                auto x=replaceExpressionSubtree(f,fr,subtree(m,mr),limits()),y=replaceExpressionSubtree(m,mr,subtree(f,fr),limits());
                if(!x||!y){count(statistics_.structuralRejections);continue;}
                left.replaceExpression(context_->domain()->materialize(*x));right.replaceExpression(context_->domain()->materialize(*y));
                count(statistics_.crossoverChanges,(left.text()!=father.text())+(right.text()!=mother.text()));accepted=true;break;
            }
            if(!accepted)count(statistics_.crossoverFallbacks);
        }
        a=std::move(left);b=std::move(right);
    }
    ExpressionCandidate candidate(const Expression& hole) const{return ExpressionCandidate::constrained(context_->domain()->materialize(hole),context_);}
private:
    static std::size_t index(std::size_t n,std::mt19937& rng){return std::uniform_int_distribution<std::size_t>(0,n-1)(rng);}
    static void count(std::uint64_t& n,std::uint64_t amount=1){if(amount>std::numeric_limits<std::uint64_t>::max()-n)throw std::overflow_error("template variation counter exhausted");n+=amount;}
    ExpressionLimits limits() const{return context_->domain()->expressionTemplate().effectiveSubstitutionLimits();}
    Expression random(std::mt19937& rng,std::size_t depth) const{return genetik_detail::generateExpression(rng,depth,context_->problem()->dimensions(),limits(),grammar_,&context_->domain()->allowedVariables());}
    static Expression subtree(const Expression& e,std::size_t root) {const auto [begin,end]=expressionSubtree(e,root);const auto nodes=e.nodes();return Expression(std::span(nodes.data()+begin,end-begin),e.variableCount(),e.limits());}
    void require(const ExpressionCandidate& c) const {
        if(c.problemIdentity()!=context_)throw std::invalid_argument("template candidate run context differs");
        context_->validate(c.expression());
    }
    std::shared_ptr<const ScalarTemplateContext> context_;ExpressionGrammar grammar_;VariationStatistics statistics_;
};

inline SymbolicRegressionResult scalarTemplateRegression(std::shared_ptr<const ScalarTemplateProblem> domain,
         const SymbolicRegressionConfig& config,RandomContext& random,std::shared_ptr<TrainingObjective> objective={}) {
    if(!domain)throw std::invalid_argument("template domain required");
    // Validate all configuration before any RNG access or training work.
    if(random.evaluationAttempts()!=0 || !config.population || config.population>std::numeric_limits<unsigned int>::max() || config.elites>=config.population ||
       !config.fitEveryGenerations || !std::isfinite(config.crossoverRate) || config.crossoverRate<0 || config.crossoverRate>1 ||
       !std::isfinite(config.mutationRate) || config.mutationRate<0 || config.mutationRate>1 || !std::isfinite(config.trainingLossThreshold) || config.trainingLossThreshold<0)
        throw std::invalid_argument("invalid template search configuration");
    domain->validateGrammar(config.grammar);
    if(config.fitting) {
        validateConstantFitConfigForLimit(*config.fitting,domain->expressionTemplate().effectiveSubstitutionLimits().maxAbsConstant);
        for(double d:config.grammar.constants)if(d<config.fitting->lower || d>config.fitting->upper)throw std::invalid_argument("template terminal outside fitting interval");
        if(config.grammar.continuousProbability>0 && (config.grammar.continuousLower<config.fitting->lower || config.grammar.continuousUpper>config.fitting->upper))throw std::invalid_argument("template sampling outside fitting interval");
    }
    if(!objective)objective=std::make_shared<TrainingObjective>(domain->problem(),config.maxTrainingLossAttempts,config.diagnostics);
    if(objective->problem()!=domain->problem() || objective->maximum()!=config.maxTrainingLossAttempts || objective->diagnostics()!=config.diagnostics || objective->work()!=TrainingWork{})throw std::invalid_argument("template search requires a fresh matching objective");
    if(random.execution().requested()>1 && (!objective->parallelEvaluationSafe() || config.diagnostics))throw std::invalid_argument("parallel template search requires qualified objective and no detailed diagnostics");
    if(random.execution().requested()>1)(void)objective->forkParallel(random.execution());
    auto preparation=std::make_shared<ConstantRefinement>(objective,config.fitting,config.fitEveryGenerations,std::make_shared<ScalarTemplateParameters>(domain));
    auto context=std::make_shared<const ScalarTemplateContext>(domain,preparation);
    ScalarTemplateVariation variation(context,config.grammar);
    return genetik_detail::runSymbolicSearch(domain->problem(),config,random,objective,preparation,std::move(variation));
}
