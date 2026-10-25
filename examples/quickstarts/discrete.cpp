#include <DiscreteExpressionParsing.hpp>
#include <DiscreteExpressionFormatting.hpp>
#include <DiscreteSymbolicRegression.hpp>
#include <charconv>
#include <iostream>

int main(int argc,char** argv) {
    try {
        std::size_t threads=1;
        if(argc!=1){if(argc!=3||std::string_view(argv[1])!="--threads")throw std::invalid_argument("usage: genetik_discrete_quickstart [--threads N]");
            auto text=std::string_view(argv[2]);auto parsed=std::from_chars(text.data(),text.data()+text.size(),threads);if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||!threads||threads>64)throw std::invalid_argument("threads must be 1..64");}
        auto expression=parseDiscreteExpression("if((x0 % 2) == 0, (x0 * x0), ((3 * x0) + 1))",1);
        const auto canonical=expression.toString();
        if(!parseDiscreteExpression(canonical,1).sameRepresentation(expression))return 1;
        for(std::int64_t n:{-3,0,2,3}){
            auto result=expression.evaluate(std::array{n});if(!result.valid())return 1;
            std::cout<<"n="<<n<<" value="<<result.value->integer()<<" visits="<<result.nodeVisits<<" operations="<<result.operations<<'\n';
        }
        std::cout<<"Canonical: "<<canonical<<"\nPretty: "<<formatDiscreteExpression(expression,DiscreteFormat::Unicode,[](std::size_t){return "n";})<<"\nLaTeX: "<<formatDiscreteExpression(expression,DiscreteFormat::LaTeX,[](std::size_t){return "n";})<<'\n';
        auto lazy=parseDiscreteExpression("if(x0 == 0, 0, 10 % x0)",1);
        if(lazy.evaluate(std::array<std::int64_t,1>{0}).value->integer()!=0)return 1;
        if(lazy.evaluate(std::array<std::int64_t,1>{-1}).valid())return 1;
        std::vector<DiscreteRegressionRow> rows;
        for(std::int64_t n=-4;n<=4;++n)rows.push_back({{n},n%2==0?n*n:3*n+1}); // independent reference, no target injected into search
        auto problem=DiscreteRegressionProblem::create(1,rows);DiscreteSearchConfig config;config.population=24;config.maxLossAttempts=504;config.maxGenerations=20;
        RandomContext random(42,ExecutionConfig{static_cast<unsigned>(threads)});auto search=discreteSymbolicRegression(problem,config,random);
        if(!search.best)throw std::runtime_error("bounded search had no viable candidates");
        std::cout<<"Bounded search: mismatches="<<search.best->fitness()<<" attempts="<<search.work.lossAttempts<<" committed="<<search.publishedGenerations<<" stop="<<static_cast<int>(search.stop)<<"\nModel: "<<search.best->text()<<'\n';
        try{(void)parseDiscreteExpression("if(1, true, 0)",1);return 1;}catch(const DiscreteParseError& error){std::cout<<"Rejected: "<<error.what()<<'\n';}
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
