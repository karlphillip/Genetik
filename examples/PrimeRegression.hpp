#pragma once
// Benchmark preparation and diagnostics only; candidates see ordinary numerical rows.
#include "RegressionWorkflow.hpp"
#include <array>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <functional>
#include <locale>
#include <numeric>
#include <tuple>
#include <string_view>

namespace prime_example {
inline constexpr std::array<std::uint32_t,5> seeds{0,1,2,3,42};
inline SymbolicRegressionConfig config(std::uint64_t budget) {
    auto c=regression_example::searchConfig(budget);
    c.grammar.operators=regression_example::selectOperators({"add","sub","mul","div","log"});
    c.trainingLossThreshold=0; c.fitting->lossThreshold=0;
    return c;
}
inline std::vector<std::uint32_t> reference() {
    // Fixed upper bound suffices for 1,200 primes; independently check completeness below.
    std::array<bool,10001> composite{};
    for(std::size_t p=2;p<=10000/p;++p)if(!composite[p])
        for(std::size_t j=p*p;j<=10000;j+=p)composite[j]=true;
    std::vector<std::uint32_t> result;
    for(std::uint32_t n=2;n<=10000 && result.size()<1200;++n)if(!composite[n])result.push_back(n);
    if(result.size()!=1200)throw std::logic_error("reference sieve bound insufficient");
    std::size_t index=0;
    for(std::uint32_t n=2;n<=result.back();++n) {
        bool prime=true;
        for(std::uint32_t d=2;d<=n/d;++d)if(n%d==0){prime=false;break;}
        if(prime && (index>=result.size() || result[index++]!=n))throw std::logic_error("independent reference mismatch");
    }
    for(const auto& [i,p]:std::array<std::pair<std::size_t,std::uint32_t>,7>{{{1,2},{2,3},{3,5},{100,541},{1000,7919},{1001,7927},{1200,9733}}})
        if(result.at(i-1)!=p)throw std::logic_error("OEIS reference anchor mismatch");
    return result;
}
inline void validatePrefix(const std::vector<std::uint32_t>& supplied,const std::vector<std::uint32_t>& ref) {
    if(supplied.size()!=1000)throw std::invalid_argument("prime prefix: expected exactly 1000 values");
    for(std::size_t i=0;i<1000;++i)if(supplied[i]!=ref.at(i))
        throw std::invalid_argument("prime prefix: incorrect value/order at one-based index "+std::to_string(i+1));
}
inline std::vector<std::uint32_t> loadPrefix(const std::string& path,const std::vector<std::uint32_t>& ref) {
    std::ifstream in(path,std::ios::binary);
    if(!in)throw std::invalid_argument(path+": cannot open prime prefix");
    // Bound even hostile files before allocating an unbounded line.
    std::string content;char ch;
    while(in.get(ch)){if(content.size()==65536)throw std::invalid_argument(path+": exceeds 65536 bytes");content+=ch;}
    if(!in.eof())throw std::runtime_error(path+": read failure");
    std::vector<std::uint32_t> values;std::size_t begin=0,line=0;
    while(begin<content.size()) {
        ++line;auto end=content.find('\n',begin);if(end==std::string::npos)end=content.size();
        std::string_view field(content.data()+begin,end-begin);begin=end+1;
        if(field.size()>32)throw std::invalid_argument(path+": line "+std::to_string(line)+": exceeds 32 bytes");
        if(!field.empty() && field.back()=='\r')field.remove_suffix(1);
        while(!field.empty() && (field.front()==' ' || field.front()=='\t'))field.remove_prefix(1);
        while(!field.empty() && (field.back()==' ' || field.back()=='\t'))field.remove_suffix(1);
        if(field.empty())continue;
        std::uint32_t value=0;auto parsed=std::from_chars(field.data(),field.data()+field.size(),value);
        if(parsed.ec!=std::errc{} || parsed.ptr!=field.data()+field.size() || field.front()<'0' || field.front()>'9')
            throw std::invalid_argument(path+": line "+std::to_string(line)+": expected unsigned decimal integer");
        if(values.size()==1000)throw std::invalid_argument(path+": more than 1000 values");
        values.push_back(value);
    }
    try{validatePrefix(values,ref);}catch(const std::exception& e){throw std::invalid_argument(path+": "+e.what());}
    return values;
}
inline double input(std::size_t n){return static_cast<double>(n)/1000.0;}
inline double target(std::uint32_t p){return static_cast<double>(p)/10000.0;}
inline std::vector<RegressionRow> rows(const std::vector<std::uint32_t>& ref,std::size_t first=1,std::size_t last=1000) {
    if(first==0 || first>last || last>ref.size())throw std::invalid_argument("reference index range");
    std::vector<RegressionRow> result;
    for(std::size_t n=first;n<=last;++n)result.push_back({{input(n)},target(ref[n-1])});
    return result;
}
inline std::optional<double> decode(const Expression& expression,std::size_t n) {
    const auto result=expression.evaluate(std::array{input(n)});
    if(!result.valid())return {};
    volatile double raw=10000.0*result.value();
    if(!std::isfinite(raw))return {};
    return raw;
}
inline std::optional<std::int64_t> rounded(double prediction) {
    if(!std::isfinite(prediction))return {};
    const double value=std::round(prediction); // Exactly once, halfway away from zero.
    // INT64_MAX converts to 2^63 in binary64: NEVER use that as an inclusive bound.
    if(value < -0x1p63 || value >= 0x1p63)return {};
    return static_cast<std::int64_t>(value);
}
struct Prediction {
    std::size_t index; std::uint32_t reference;
    std::optional<double> raw,residual;
    std::optional<std::int64_t> integer;
};
inline Prediction prediction(std::size_t n,std::uint32_t p,std::optional<double> raw) {
    if(raw && !std::isfinite(*raw))raw.reset();
    Prediction point{n,p,raw,{},{}};
    if(raw) {
        point.integer=rounded(*raw);
        const double residual=*raw-static_cast<double>(p);
        if(std::isfinite(residual))point.residual=residual;
    }
    return point;
}
struct Metrics {
    std::size_t count=0,invalid=0,unrepresentable=0,negative=0,matches=0,duplicates=0;
    std::size_t rawPairs=0,rawViolations=0,roundedPairs=0,roundedViolations=0;
    std::optional<double> mse,rmse,mae,maximum,meanRelative,maxRelative;
};
inline Metrics metrics(const std::vector<Prediction>& points) {
    if(points.empty())throw std::invalid_argument("empty assessment range");
    Metrics m;m.count=points.size();double squared=0,absolute=0,relative=0,maximum=0,maxRelative=0;bool good=true;
    std::set<std::int64_t> seen;
    for(std::size_t i=0;i<points.size();++i) {
        const auto& p=points[i];
        if(p.reference==0)throw std::invalid_argument("relative errors require positive references");
        if(!p.raw){++m.invalid;good=false;}
        else {
            if(*p.raw<0)++m.negative;
            if(!p.integer)++m.unrepresentable;
            if(!p.residual)good=false;
            else {
                const double error=std::abs(*p.residual),r=error/p.reference;
                volatile double square=error*error;
                squared+=square;absolute+=error;relative+=r;
                maximum=std::max(maximum,error);maxRelative=std::max(maxRelative,r);
                if(!std::isfinite(squared) || !std::isfinite(absolute) || !std::isfinite(relative))good=false;
            }
        }
        if(p.integer) {
            if(*p.integer==p.reference)++m.matches;
            if(!seen.insert(*p.integer).second)++m.duplicates;
        }
        if(i) {
            const auto& previous=points[i-1];
            if(previous.raw && p.raw){++m.rawPairs;if(*p.raw<=*previous.raw)++m.rawViolations;}
            if(previous.integer && p.integer){++m.roundedPairs;if(*p.integer<=*previous.integer)++m.roundedViolations;}
        }
    }
    if(good) {
        const double count=static_cast<double>(points.size());
        m.mse=squared/count;m.rmse=std::sqrt(*m.mse);m.mae=absolute/count;m.maximum=maximum;
        m.meanRelative=relative/count;m.maxRelative=maxRelative;
    }
    return m;
}
inline void printMetrics(const std::string& name,const std::string& range,const Metrics& m) {
    std::cout<<"model="<<name<<" range="<<range;
    for(const auto& [key,value]:std::array<std::pair<const char*,std::optional<double>>,6>{{
        {"mse",m.mse},{"rmse",m.rmse},{"mae",m.mae},{"max_abs",m.maximum},{"mean_relative",m.meanRelative},{"max_relative",m.maxRelative}}}) {
        std::cout<<' '<<key<<'=';if(value)std::cout<<*value;else std::cout<<"invalid";
    }
    std::cout<<" matches="<<m.matches<<" denominator="<<m.count<<" match_rate="<<static_cast<double>(m.matches)/m.count
             <<" invalid_predictions="<<m.invalid<<" integer_unrepresentable="<<m.unrepresentable<<" negative="<<m.negative
             <<" raw_increase_violations="<<m.rawViolations<<" raw_assessable_pairs="<<m.rawPairs
             <<" rounded_increase_violations="<<m.roundedViolations<<" rounded_assessable_pairs="<<m.roundedPairs
             <<" rounded_duplicates="<<m.duplicates<<'\n';
}
inline std::ofstream output(const std::filesystem::path& path) {
    std::ofstream out(path);out.exceptions(std::ios::badbit|std::ios::failbit);out.imbue(std::locale::classic());out<<std::setprecision(17);return out;
}
inline void exportData(const std::filesystem::path& directory,const std::vector<std::uint32_t>& ref) {
    std::filesystem::create_directories(directory);
    for(const auto& [name,first,last]:std::array<std::tuple<const char*,std::size_t,std::size_t>,2>{{{"fitting.csv",1,1000},{"extrapolation.csv",1001,1200}}}) {
        auto out=output(directory/name);out<<"n,prime,u,v\n";
        for(std::size_t n=first;n<=last;++n)out<<n<<','<<ref.at(n-1)<<','<<input(n)<<','<<target(ref[n-1])<<'\n';
    }
    auto prefix=output(directory/"first-1000.txt"),all=output(directory/"first-1200.txt");
    for(std::size_t i=0;i<ref.size();++i){all<<ref[i]<<'\n';if(i<1000)prefix<<ref[i]<<'\n';}
}
// Small, example-local Householder QR. No normal equations or optimizer used for references.
inline std::vector<double> leastSquares(const std::vector<std::vector<double>>& design,const std::vector<double>& targetValues) {
    if(design.empty() || design.size()!=targetValues.size() || design.front().empty() || design.front().size()>design.size())
        throw std::invalid_argument("least-squares shape");
    const std::size_t n=design.size(),d=design.front().size();
    std::vector<std::vector<long double>> a(n,std::vector<long double>(d));std::vector<long double> b(n);
    for(std::size_t i=0;i<n;++i) {
        if(design[i].size()!=d || !std::isfinite(targetValues[i]))throw std::invalid_argument("least-squares data");
        b[i]=targetValues[i];for(std::size_t j=0;j<d;++j){if(!std::isfinite(design[i][j]))throw std::invalid_argument("least-squares data");a[i][j]=design[i][j];}
    }
    for(std::size_t k=0;k<d;++k) {
        long double norm=0;for(std::size_t i=k;i<n;++i)norm=std::hypot(norm,a[i][k]);
        if(norm==0)throw std::invalid_argument("rank-deficient baseline");
        const auto alpha=-std::copysign(norm,a[k][k]);std::vector<long double> v(n-k);
        for(std::size_t i=k;i<n;++i)v[i-k]=a[i][k];
        v[0]-=alpha;
        long double vv=0;for(auto value:v)vv+=value*value;
        for(std::size_t j=k;j<d;++j) {
            long double dot=0;for(std::size_t i=k;i<n;++i)dot+=v[i-k]*a[i][j];
            for(std::size_t i=k;i<n;++i)a[i][j]-=2*v[i-k]*dot/vv;
        }
        long double dot=0;for(std::size_t i=k;i<n;++i)dot+=v[i-k]*b[i];
        for(std::size_t i=k;i<n;++i)b[i]-=2*v[i-k]*dot/vv;
    }
    std::vector<double> result(d);std::vector<long double> x(d);
    for(std::size_t k=d;k-->0;) {
        long double value=b[k];for(std::size_t j=k+1;j<d;++j)value-=a[k][j]*x[j];
        // Reject numerically rank-deficient triangular systems, not arbitrary regularization.
        if(std::abs(a[k][k])<=64*std::numeric_limits<long double>::epsilon())throw std::invalid_argument("rank-deficient baseline");
        x[k]=value/a[k][k];result[k]=static_cast<double>(x[k]);if(!std::isfinite(result[k]))throw std::runtime_error("nonfinite baseline coefficient");
    }
    return result;
}
struct Baseline {
    std::string name;std::vector<double> coefficients;
    std::optional<Expression> normalized;
    std::optional<double> predict(std::size_t n)const {
        if(normalized)return decode(*normalized,n);
        const double value=static_cast<double>(n)*std::log(static_cast<double>(n));
        if(!std::isfinite(value))return {};
        return value;
    }
};
inline std::vector<Baseline> baselines(const std::vector<RegressionRow>& fitting) {
    using K=ExpressionKind;
    const ExpressionLimits limits{31,6,8};const auto u=Expression::variable(0,1,limits);
    const auto constant=[&](double c){return Expression::constant(c,1,limits);};
    const auto add=[&](const Expression& a,const Expression& b){return Expression::binary(K::Add,a,b,limits);};
    const auto mul=[&](const Expression& a,const Expression& b){return Expression::binary(K::Multiply,a,b,limits);};
    std::vector<double> y;for(const auto& row:fitting)y.push_back(row.target);
    std::vector<Baseline> result;
    for(int kind=0;kind<4;++kind) {
        std::vector<std::vector<double>> matrix;
        for(const auto& row:fitting) {
            const double x=row.inputs.at(0);
            if(kind==0)matrix.push_back({1});
            if(kind==1)matrix.push_back({1,x});
            if(kind==2)matrix.push_back({1,x,x*x,(x*x)*x});
            if(kind==3)matrix.push_back({x*std::log(x),x,1});
        }
        const auto c=leastSquares(matrix,y);Expression model=constant(c[0]);
        if(kind==1)model=add(constant(c[0]),mul(constant(c[1]),u));
        if(kind==2)model=add(add(constant(c[0]),mul(constant(c[1]),u)),
            add(mul(constant(c[2]),mul(u,u)),mul(constant(c[3]),mul(mul(u,u),u))));
        if(kind==3)model=add(add(mul(constant(c[0]),mul(u,Expression::unary(K::Log,u,limits))),mul(constant(c[1]),u)),constant(c[2]));
        result.push_back({std::array{"constant","affine","cubic","fitted_log"}[kind],c,model});
    }
    result.insert(result.begin()+3,Baseline{"n_log_n",{},std::nullopt});return result;
}
inline bool better(const SymbolicRegressionResult& a,std::uint32_t aSeed,const SymbolicRegressionResult& b,std::uint32_t bSeed) {
    if(!a.best)return false;
    if(!b.best)return true;
    const RegressionOrdering order;
    if(order.better(*a.best,*b.best,ObjectiveDirection::Minimize))return true;
    if(order.better(*b.best,*a.best,ObjectiveDirection::Minimize))return false;
    return aSeed<bSeed;
}
inline void writePredictions(const std::filesystem::path& path,const std::vector<Prediction>& predictions) {
    auto out=output(path);out<<"index,range,reference,continuous_prediction,rounded_prediction,residual,arithmetic_valid,integer_representable\n";
    for(const auto& p:predictions) {
        out<<p.index<<','<<(p.index<=1000?"fitting":"extrapolation")<<','<<p.reference<<',';
        if(p.raw)out<<*p.raw;
        out<<',';if(p.integer)out<<*p.integer;
        out<<',';if(p.residual)out<<*p.residual;
        out<<','<<static_cast<bool>(p.raw)<<','<<static_cast<bool>(p.integer)<<'\n';
    }
}
} // namespace prime_example
