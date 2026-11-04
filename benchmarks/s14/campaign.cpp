#include "MultivariateRationalRegression.hpp"
#include <SymbolicRegression.hpp>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {
using Rows=std::vector<RegressionRow>;
std::string quote(const std::string& s){std::ostringstream o;o<<'"';for(char c:s){if(c=='"'||c=='\\')o<<'\\';if(c=='\n')o<<"\\n";else o<<c;}o<<'"';return o.str();}
Rows readCsv(const std::string& path,bool training){
    std::ifstream in(path);if(!in)throw std::runtime_error("cannot open CSV: "+path);
    std::string line;if(!std::getline(in,line)||line!="x0,x1,y")throw std::runtime_error("expected x0,x1,y CSV header");
    Rows rows;while(std::getline(in,line)){if(line.empty())continue;std::stringstream ss(line);std::string a,b,c; if(!std::getline(ss,a,',')||!std::getline(ss,b,',')||!std::getline(ss,c,','))throw std::runtime_error("malformed CSV row");
        const double x=std::stod(a),y=std::stod(b),z=std::stod(c);if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z))throw std::runtime_error("nonfinite CSV row");rows.push_back({{x,y},z});}
    if(rows.empty()) throw std::runtime_error("empty CSV");
    (void)training;
    return rows;
}
std::vector<genetik_s14::Observation> toS14(const Rows& rows){std::vector<genetik_s14::Observation> out;out.reserve(rows.size());for(const auto& r:rows)out.push_back({r.inputs[0],r.inputs[1],r.target});return out;}
struct Score{std::size_t invalid=0;double rmse=0,maxAbs=0;};
Score score(const Expression& e,const Rows& rows){Score s;long double sum=0;genetik_detail::EvaluationWorkspace w;for(const auto& r:rows){const auto p=w.evaluate(e,r.inputs);if(!p.valid()){++s.invalid;continue;}const double d=p.value()-r.target;if(!std::isfinite(d)){++s.invalid;continue;}sum+=static_cast<long double>(d)*d;s.maxAbs=std::max(s.maxAbs,std::abs(d));}if(!s.invalid)s.rmse=std::sqrt(static_cast<double>(sum/rows.size()));else s.rmse=std::numeric_limits<double>::infinity();return s;}
void emitScore(const char* name,const Score& s){std::cout<<'"'<<name<<"\":{\"invalid\":"<<s.invalid<<",\"rmse\":";if(std::isfinite(s.rmse))std::cout<<s.rmse;else std::cout<<"null";std::cout<<",\"max_abs\":"<<s.maxAbs<<'}';}
const char* domainStatus(genetik_s14::S14DomainStatus s){switch(s){case genetik_s14::S14DomainStatus::Valid:return "valid";case genetik_s14::S14DomainStatus::Invalid:return "invalid";case genetik_s14::S14DomainStatus::Uncertain:return "uncertain";}return "uncertain";}
const char* domainFinding(genetik_s14::DomainFinding s){switch(s){case genetik_s14::DomainFinding::SeparatedFromZero:return "separated_from_zero";case genetik_s14::DomainFinding::DemonstratedZero:return "demonstrated_zero";case genetik_s14::DomainFinding::Inconclusive:return "inconclusive";}return "inconclusive";}
const char* predictionStatus(genetik_s14::PredictionNumericalStatus s){switch(s){case genetik_s14::PredictionNumericalStatus::NotAssessed:return "not_assessed";case genetik_s14::PredictionNumericalStatus::FiniteBounded:return "finite_bounded";case genetik_s14::PredictionNumericalStatus::OverflowPossible:return "overflow_possible";case genetik_s14::PredictionNumericalStatus::Uncertain:return "uncertain";}return "uncertain";}
}

int main(int argc,char** argv){
    try{
        if(argc!=8)throw std::invalid_argument("usage: s14_campaign METHOD TASK TRACK SEED TRAIN.csv HELDOUT.csv EXTRAP.csv");
        const std::string method=argv[1],task=argv[2],track=argv[3];const auto seed=static_cast<std::uint32_t>(std::stoul(argv[4]));
        const auto train=readCsv(argv[5],true);const auto training=std::chrono::steady_clock::now();
        std::string expression;double trainRmse=0;std::size_t candidates=0,lossAttempts=0,mathOps=0,laOps=0,jacobians=0;std::string stop;
        if(method=="constructive_qr"||method=="constructive_lm"){
            genetik_s14::Config c;c.refineWithLM=method=="constructive_lm";c.maximumRows=1000;c.maximumWorkspaceBytes=16*1024*1024;
            c.maximumArithmeticOperations=50000000;c.maximumLinearAlgebraOperations=50000000;c.maximumAbsoluteCoefficient=8;c.expressionLimits={127,16,8,2048};
            c.lm.maxIterations=16;c.lm.maxObjectiveEvaluations=32;c.lm.maxJacobianEvaluations=16;c.lm.maxWorkspaceBytes=16*1024*1024;c.lm.maxMathematicalOperations=50000000;
            const auto fit=genetik_s14::fit(toS14(train),c);stop=genetik_s14::statusName(fit.status);candidates=fit.attemptedFamilies;lossAttempts=fit.trainingLossEvaluations;mathOps=fit.arithmeticOperations;laOps=fit.linearAlgebraOperations; jacobians=fit.lmWork.jacobianEvaluations;
            if(fit.status!=genetik_s14::Status::Success||!fit.selected)throw std::runtime_error(std::string("constructive fit failed: ")+fit.detail);
            const auto& m=fit.models[*fit.selected];expression=m.expression.toString();trainRmse=std::sqrt(m.trainingSSE/train.size());
            const auto domain=genetik_s14::assessDomain(m,{-1,1,-1,1},1024,8,c.minimumAbsoluteDenominator);
            std::cout<<std::setprecision(17)<<"{\"method\":"<<quote(method)<<",\"task\":"<<quote(task)<<",\"track\":"<<quote(track)<<",\"seed\":"<<seed<<",\"expression\":"<<quote(expression)<<",\"numerator_degree\":"<<m.numeratorDegree<<",\"denominator_degree\":"<<m.denominatorDegree<<",\"parameters\":"<<m.parameterCount<<",\"coefficients_p\":[";
            for(std::size_t i=0;i<m.numerator.size();++i){if(i)std::cout<<',';std::cout<<m.numerator[i];}std::cout<<"],\"coefficients_q\":[";for(std::size_t i=0;i<m.denominator.size();++i){if(i)std::cout<<',';std::cout<<m.denominator[i];}std::cout<<"],\"pivot_ratio\":"<<m.pivotRatio<<",\"fit_train_rmse\":"<<trainRmse<<",\"candidate_families\":"<<candidates<<",\"loss_evaluations\":"<<lossAttempts<<",\"mathematical_operations\":"<<mathOps<<",\"linear_algebra_operations\":"<<laOps<<",\"jacobian_evaluations\":"<<jacobians<<",\"lm_stop\":"<<quote(m.refined?genetik_rational_detail::lmStopName(m.lmStop):"not_run")<<",\"domain_status\":"<<quote(domainStatus(domain.status))<<",\"domain_finding\":"<<quote(domainFinding(domain.finding))<<",\"domain_reason\":"<<quote(domain.reason)<<",\"domain_minimum_separation\":"<<domain.minimumCertifiedSeparation<<",\"domain_cells\":"<<domain.cellsVisited<<",\"domain_interval_operations\":"<<domain.intervalOperations<<",\"prediction_numerical_status\":"<<quote(predictionStatus(domain.predictionNumericalStatus))<<",\"prediction_bound\":";
            if(std::isfinite(domain.maximumAbsolutePredictionBound))std::cout<<domain.maximumAbsolutePredictionBound;else std::cout<<"null";
            std::cout<<",\"stop\":"<<quote(stop)<<",\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-training).count();
            auto held=readCsv(argv[6],false),extrap=readCsv(argv[7],false);const auto hs=score(m.expression,held),xs=score(m.expression,extrap);std::cout<<',';emitScore("heldout",hs);std::cout<<',';emitScore("extrapolation",xs);std::cout<<"}\n";return 0;
        }
        if(method!="ga_coordinate"&&method!="ga_lm")throw std::invalid_argument("unknown method");
        const auto problem=RegressionProblem::create(train,{127,10,8,2048});
        SymbolicRegressionConfig c;c.population=32;c.maxGenerationAttempts=16;c.maxEvaluations=512;c.maxTrainingLossAttempts=12000;c.trainingLossThreshold=1e-14;
        c.grammar.operators={ExpressionKind::Add,ExpressionKind::Subtract,ExpressionKind::Multiply,
                             ExpressionKind::Divide,ExpressionKind::Powi,ExpressionKind::Sin};
        c.grammar.exponents={2};c.grammar.continuousProbability=.5;c.grammar.continuousLower=-3;c.grammar.continuousUpper=3;c.grammar.initialMaxDepth=4;c.grammar.mutationMaxDepth=3;
        ConstantFitConfig f;f.lower=-8;f.upper=8;f.maxLossAttempts=12;f.maxSweeps=6;f.maxProposals=128;f.lossThreshold=1e-14;
        f.method=method=="ga_lm"?ConstantFitMethod::LevenbergMarquardt:ConstantFitMethod::Coordinate;
        f.levenbergMarquardt.maxIterations=8;f.levenbergMarquardt.maxObjectiveEvaluations=12;f.levenbergMarquardt.maxJacobianEvaluations=8;f.levenbergMarquardt.maxMathematicalOperations=1000000;f.levenbergMarquardt.maxWorkspaceBytes=4*1024*1024;
        c.fitting=f;RandomContext random(seed);const auto start=std::chrono::steady_clock::now();const auto result=symbolicRegression(problem,c,random);const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        if(!result.best) throw std::runtime_error("GA produced no best candidate");
        const auto& e=result.best->expression();expression=e.toString();trainRmse=std::sqrt(result.best->fitness());candidates=result.candidateEvaluations;lossAttempts=result.trainingWork.lossAttempts;mathOps=result.trainingWork.rowAttempts;stop=searchStopName(result.stoppingReason);
        auto held=readCsv(argv[6],false),extrap=readCsv(argv[7],false);const auto hs=score(e,held),xs=score(e,extrap);
        std::cout<<std::setprecision(17)<<"{\"method\":"<<quote(method)<<",\"task\":"<<quote(task)<<",\"track\":"<<quote(track)<<",\"seed\":"<<seed<<",\"expression\":"<<quote(expression)<<",\"numerator_degree\":null,\"denominator_degree\":null,\"parameters\":null,\"fit_train_rmse\":"<<trainRmse<<",\"candidate_evaluations\":"<<candidates<<",\"training_loss_attempts\":"<<lossAttempts<<",\"training_row_attempts\":"<<result.trainingWork.rowAttempts<<",\"mathematical_operations\":null,\"fitting_loss_attempts\":"<<result.trainingWork.fittingLossAttempts<<",\"jacobian_evaluations\":"<<result.optimizerWork.jacobianEvaluations<<",\"invalid_losses\":"<<result.trainingWork.invalidLosses<<",\"stop\":"<<quote(stop)<<",\"seconds\":"<<seconds<<',';emitScore("heldout",hs);std::cout<<',';emitScore("extrapolation",xs);std::cout<<"}\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
