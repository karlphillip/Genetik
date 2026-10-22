#pragma once
#include "PrimeExactReport.hpp"
#include <ExpressionFormatting.hpp>
#include <chrono>
namespace prime_example {
inline std::string exactElapsed(double elapsed){
    const auto seconds=static_cast<std::uint64_t>(std::max(0.0,elapsed));std::ostringstream s;
    s<<std::setfill('0')<<std::setw(2)<<seconds/3600<<':'<<std::setw(2)<<(seconds/60)%60<<':'<<std::setw(2)<<seconds%60;return s.str();
}
inline std::string exactEquation(const Expression& e,ExpressionTextFormat format){
    const auto variable=[](std::size_t index,ExpressionTextFormat f){if(index!=0)throw std::logic_error("prime variable binding");return f==ExpressionTextFormat::LaTeX?std::string("\\frac{n}{1000}"):std::string("n/1000");};
    const auto body=formatExpression(e,format,variable);
    return format==ExpressionTextFormat::LaTeX?"\\hat{p}(n) = 10000 \\cdot \\left("+body+"\\right)":"p̂(n) = 10000 · ("+body+")";
}
inline std::string jsonString(std::string_view value){std::string out="\"";constexpr char digits[]="0123456789abcdef";for(unsigned char c:value){if(c=='"'||c=='\\'){out+='\\';out+=static_cast<char>(c);}else if(c<32){out+="\\u00";out+=digits[c>>4];out+=digits[c&15];}else out+=static_cast<char>(c);}return out+'"';}
struct ExactDisplayState {std::optional<double> previousLoss;std::optional<std::size_t> previousMatches;};
inline void exactHumanSnapshot(std::ostream& out,const std::vector<ExpressionCandidate>& members,
        const std::vector<std::uint32_t>& ref,const ExactProgress& p,std::size_t top,double elapsed,
        std::string_view kind,std::string_view state,ExactReportingWork& work,ExactDisplayState& prior,
        std::ostream* metrics=nullptr,const std::optional<ExpressionCandidate>& archive={},
        std::string_view checkpoint={},std::optional<std::uint64_t> checkpointGeneration={},double checkpointAge=0,bool raw=false){
    // One ordinary prediction pass, shared by the human and JSONL renderers. The
    // established raw renderer owns that pass and emits all original fields.
    std::ostringstream dense;dense.imbue(std::locale::classic());dense<<std::setprecision(17);
    exactSnapshot(dense,members,ref,p,top,elapsed,kind,state,work);
    const auto ranked=exactRanking(members,top);std::istringstream lines(dense.str());std::string header;std::getline(lines,header);
    struct Row {std::string fields,canonical,decoded;};std::vector<Row> rows;
    for(std::size_t i=0;i<ranked.indices.size();++i){Row row;std::getline(lines,row.fields);std::getline(lines,row.canonical);std::getline(lines,row.decoded);rows.push_back(std::move(row));}
    const auto field=[](const std::string& fields,const std::string& name){const auto begin=fields.find(name+"=");if(begin==std::string::npos)throw std::logic_error("missing snapshot field");const auto a=begin+name.size()+1;return fields.substr(a,fields.find(' ',a)-a);};
    if(raw)out<<dense.str();
    else {
        const auto flags=out.flags();const auto precision=out.precision();
        if(kind=="winning")out<<"\nEXACT PREFIX RECOVERED\n";
        out<<"\nPrime Search • Generation ";if(p.generation)out<<*p.generation;else out<<"none";
        out<<" • "<<exactElapsed(elapsed)<<"\n";
        if(!ranked.indices.empty()){
            const auto& c=members[ranked.indices[0]];const auto matches=static_cast<std::size_t>(std::stoull(field(rows[0].fields,"exact_matches").substr(0,field(rows[0].fields,"exact_matches").find('/'))));
            const double fitness=exactPresentationFitness(c.fitness());
            out<<std::fixed<<std::setprecision(8)<<"Fitness        "<<fitness;
            if(prior.previousLoss){const double difference=fitness-exactPresentationFitness(*prior.previousLoss);out<<(difference>0?"  ↑ +":difference<0?"  ↓ ":"  = ")<<difference;}
            out<<"\nExact primes   "<<matches<<" / "<<ref.size();if(prior.previousMatches){const auto d=static_cast<std::int64_t>(matches)-static_cast<std::int64_t>(*prior.previousMatches);out<<(d>0?"  ↑ +":d<0?"  ↓ ":"  = ")<<d;}
            out<<"\nRMSE           "<<field(rows[0].fields,"raw_rmse")<<"\n";
            out<<"Loss           "<<std::setprecision(17)<<c.fitness()<<" (search objective, lower is better)\n";
            out<<"\nBest formula\n"<<exactEquation(c.expression(),ExpressionTextFormat::Unicode)<<"\n\nComplexity     "<<c.expression().nodeCount()<<" nodes • depth "<<c.expression().depth()<<" • "<<field(rows[0].fields,"constant_count")<<" constants\n";
            if(kind=="final"||kind=="winning")out<<"Canonical g(x0)  "<<c.text()<<"\nLaTeX           "<<exactEquation(c.expression(),ExpressionTextFormat::LaTeX)<<"\n";
            for(std::size_t i=1;i<ranked.indices.size();++i){const auto& other=members[ranked.indices[i]];out<<"  #"<<i+1<<" fitness "<<std::setprecision(8)<<exactPresentationFitness(other.fitness())<<" • "<<field(rows[i].fields,"exact_matches")<<" exact • RMSE "<<field(rows[i].fields,"raw_rmse")<<" • "<<other.expression().nodeCount()<<" nodes\n     "<<exactEquation(other.expression(),ExpressionTextFormat::Unicode)<<"\n";}
            prior.previousLoss=c.fitness();prior.previousMatches=matches;
        }else out<<"No committed valid candidates.\n";
        out<<"Evaluations    "<<p.losses<<" / "<<p.allowance<<"\nBudget         "<<std::setprecision(2)<<(p.allowance?100.0*(static_cast<double>(p.losses)/static_cast<double>(p.allowance)):0)<<"%\n";
        out<<"Seed / threads "<<p.seed<<" / "<<p.requested<<" requested, "<<p.effective<<" effective\nPopulation     "<<ranked.valid<<" valid • "<<ranked.invalid<<" invalid • "<<ranked.unevaluated<<" unevaluated\nGenerations    "<<p.published<<" published / "<<p.attempts<<" attempted\n";
        if(archive){out<<"Best-ever      "<<std::setprecision(8)<<exactPresentationFitness(archive->fitness())<<" fitness • loss "<<std::setprecision(17)<<archive->fitness();if(p.bestGeneration)out<<" • generation "<<*p.bestGeneration;out<<"\n";if(ranked.indices.empty()||archive->text()!=members[ranked.indices[0]].text()||archive->fitness()!=members[ranked.indices[0]].fitness())out<<"Best-ever model "<<exactEquation(archive->expression(),ExpressionTextFormat::Unicode)<<"\n";}
        if(!checkpoint.empty()) {out<<"Checkpoint     "<<checkpoint;if(checkpointGeneration)out<<" • generation "<<*checkpointGeneration<<" • "<<exactElapsed(checkpointAge)<<" ago";out<<"\n";}
        if(kind=="final"||kind=="winning")out<<"Stopping state "<<state<<"\n";
        out.flags(flags);out.precision(precision);out.flush();if(!out)throw std::runtime_error("pretty snapshot output failed after publication");
    }
    if(metrics){
        const auto timestamp=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        // Field objects retain every old raw key; values are strings to avoid
        // precision loss in integer counters and invalid/optional diagnostics.
        const auto object=[&](const std::string& fields){std::string result="{";std::istringstream input(fields);std::string token;bool first=true;while(input>>token){const auto equal=token.find('=');if(equal==token.npos)continue;if(!first)result+=',';first=false;result+=jsonString(token.substr(0,equal))+":"+jsonString(token.substr(equal+1));}return result+"}";};
        *metrics<<"{\"schema\":\"genetik.prime-progress/1\",\"timestamp_unix_ms\":"<<timestamp<<",\"progress\":"<<object(header)<<",\"candidates\":[";
        for(std::size_t i=0;i<rows.size();++i){if(i)*metrics<<',';const auto& c=members[ranked.indices[i]];*metrics<<"{\"metrics\":"<<object(rows[i].fields)<<",\"canonical\":"<<jsonString(c.text())<<",\"decoded\":"<<jsonString(rows[i].decoded)<<",\"presentation_fitness\":"<<std::setprecision(17)<<exactPresentationFitness(c.fitness())<<"}";}
        *metrics<<"],\"checkpoint\":"<<jsonString(checkpoint)<<"}\n";metrics->flush();if(!*metrics)throw std::runtime_error("metrics output failed after publication");
    }
}
} // namespace prime_example
