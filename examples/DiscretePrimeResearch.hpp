#pragma once
// Application-only labels, frozen search policy and diagnostics. No candidate oracle.
#include <DiscreteRunCheckpoint.hpp>
#include <DiscreteExpressionParsing.hpp>
#include <DiscreteExpressionFormatting.hpp>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace discrete_prime {
inline bool referencePrime(std::int64_t n) {
    if(n<2)return false;
    for(std::int64_t d=2;d<=n/d;++d)if(n%d==0)return false;
    return true;
}
inline std::vector<std::int64_t> referencePrimes() {
    std::vector<std::int64_t> out;
    for(std::int64_t n=2;out.size()<1200;++n)if(referencePrime(n))out.push_back(n);
    if(out[999]!=7919||out.back()!=9733)throw std::logic_error("prime reference anchors");
    return out;
}
inline void validateTask(std::string_view task){if(task!="primality"&&task!="nth-prime")throw std::invalid_argument("--task primality|nth-prime required");}
inline DiscreteSearchConfig config(std::string_view task,std::uint64_t budget) {
    validateTask(task);DiscreteSearchConfig c;c.population=32;c.elites=2;c.maxGenerations=1000;c.maxLossAttempts=budget;
    c.grammar.operators.push_back(DiscreteKind::Sum);c.grammar.operators.push_back(DiscreteKind::Product);
    c.grammar.constants={-2,-1,0,1,2,4,8,16};c.grammar.positiveConstantDivisorsOnly=false;
    c.grammar.initialMaxDepth=4;c.grammar.mutationMaxDepth=3;c.grammar.maxIterationNesting=1;c.grammar.iterationBoundsAreTerminals=true;
    c.contextIdentity="genetik.discrete-prime-research/1;task="+std::string(task)+";labels=trial-division1200/1;objective=exact-mismatch-count/1;inputs=original-int64;no-fitting";return c;
}
inline DiscreteEvaluationLimits evaluationPolicy(){return {100000,100000,1048576,2048,32768,2};}
inline std::shared_ptr<const DiscreteRegressionProblem> problem(std::string_view task) {
    validateTask(task);DiscreteExpressionLimits e;e.maxNodes=49;e.maxDepth=7;e.maxConstants=49;e.maxIterationDepth=2;
    DiscreteDatasetLimits d;d.maxNodeVisitsPerLoss=50000000;d.maxOperationsPerLoss=50000000;d.maxIterationsPerLoss=20000000;
    std::vector<DiscreteRegressionRow> rows;
    if(task=="primality")for(std::int64_t n=2;n<=63;++n)rows.push_back({{n},referencePrime(n)?1:0});
    else {const auto primes=referencePrimes();for(std::size_t i=0;i<1000;++i)rows.push_back({{static_cast<std::int64_t>(i+1)},primes[i]});}
    return DiscreteRegressionProblem::create(1,rows,e,evaluationPolicy(),d);
}
// Unsigned ordered distance covers even MIN..MAX exactly without signed subtraction.
inline std::uint64_t distance(std::int64_t a,std::int64_t b) noexcept {
    return a>=b?static_cast<std::uint64_t>(a)-static_cast<std::uint64_t>(b):static_cast<std::uint64_t>(b)-static_cast<std::uint64_t>(a);
}
struct Assessment {
    std::size_t rows=0,matches=0,prefix=0,invalid=0,resources=0,cancelled=0;
    std::uint64_t visits=0,operations=0,iterations=0,maximum=0;
    long double absolute=0,squared=0;
};
inline Assessment assess(const DiscreteExpression& expression,std::span<const DiscreteRegressionRow> rows,DiscreteEvaluationLimits limits=evaluationPolicy()) {
    Assessment m;bool prefix=true;
    for(const auto& row:rows){++m.rows;auto result=expression.evaluate(row.inputs,limits);m.visits+=result.nodeVisits;m.operations+=result.operations;m.iterations+=result.iterations;
        if(!result.valid()){prefix=false;if(result.status==DiscreteEvaluationStatus::ResourceExhausted)++m.resources;else if(result.status==DiscreteEvaluationStatus::Cancelled)++m.cancelled;else ++m.invalid;continue;}
        const auto error=distance(result.value->integer(),row.target);m.maximum=std::max(m.maximum,error);m.absolute+=static_cast<long double>(error);m.squared+=static_cast<long double>(error)*static_cast<long double>(error);
        if(!error){++m.matches;if(prefix)++m.prefix;}else prefix=false;
    }return m;
}
inline std::vector<DiscreteRegressionRow> assessmentRows(std::string_view task,std::int64_t first,std::int64_t last) {
    validateTask(task);if(first<1||last<first||last>(task=="nth-prime"?1200:1024))throw std::invalid_argument("assessment range");
    std::vector<DiscreteRegressionRow> rows;const auto primes=referencePrimes();for(auto n=first;n<=last;++n)rows.push_back({{n},task=="primality"?(referencePrime(n)?1:0):primes[static_cast<std::size_t>(n-1)]});return rows;
}
inline std::string quote(std::string_view text){std::string out="\"";for(unsigned char c:text){if(c=='"'||c=='\\'){out+='\\';out+=static_cast<char>(c);}else if(c=='\n')out+="\\n";else if(c<32){const char* h="0123456789abcdef";out+="\\u00";out+=h[c>>4];out+=h[c&15];}else out+=static_cast<char>(c);}return out+'"';}
inline std::string metrics(const Assessment& m){std::ostringstream out;out<<std::setprecision(20)<<"{\"rows\":"<<m.rows<<",\"matches\":"<<m.matches<<",\"mismatches\":"<<(m.rows-m.matches-m.invalid-m.resources-m.cancelled)<<",\"prefix\":"<<m.prefix<<",\"invalid\":"<<m.invalid<<",\"resources\":"<<m.resources<<",\"cancelled\":"<<m.cancelled<<",\"visits\":"<<m.visits<<",\"operations\":"<<m.operations<<",\"iterations\":"<<m.iterations<<",\"maximum\":"<<m.maximum<<",\"mae\":";
    if(m.invalid||m.resources||m.cancelled||!m.rows)out<<"null,\"rmse\":null";else out<<m.absolute/m.rows<<",\"rmse\":"<<std::sqrt(m.squared/m.rows);return out.str()+'}';}
inline bool resourceFailure(std::exception_ptr failure){try{std::rethrow_exception(failure);}catch(const DiscreteResourceExhausted&){return true;}catch(const std::exception& e){const auto nested=dynamic_cast<const std::nested_exception*>(&e);return nested&&nested->nested_ptr()&&resourceFailure(nested->nested_ptr());}catch(...){return false;}}
inline const char* stopName(DiscreteSearchStop s){switch(s){case DiscreteSearchStop::ExactFit:return "exact_fit";case DiscreteSearchStop::GenerationLimit:return "generation_limit";case DiscreteSearchStop::LossBudget:return "loss_budget";case DiscreteSearchStop::NoViableCandidates:return "no_viable_candidates";case DiscreteSearchStop::Cancelled:return "cancelled";}throw std::logic_error("stop enum");}
}
