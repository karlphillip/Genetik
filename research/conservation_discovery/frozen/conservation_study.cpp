#include <ConservationDiscovery.hpp>
#include <ExpressionParsing.hpp>
#include <SymbolicAlgebra.hpp>
#include <SymbolicRegression.hpp>

#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

namespace {
struct State { double y, v; };
State field(State s, int system, double damping = 0.0) {
    return {s.v, (system == 0 ? -s.y : -2.0*s.y*s.y*s.y) - damping*s.v};
}
State plus(State a, State b, double k) { return {a.y+k*b.y, a.v+k*b.v}; }
State rk4(State s, int system, double dt, double damping = 0.0) {
    const auto a=field(s,system,damping), b=field(plus(s,a,dt/2),system,damping);
    const auto c=field(plus(s,b,dt/2),system,damping), d=field(plus(s,c,dt),system,damping);
    return {s.y+dt*(a.y+2*b.y+2*c.y+d.y)/6,
            s.v+dt*(a.v+2*b.v+2*c.v+d.v)/6};
}
ConservationTrajectory trajectory(State start, int system, double damping=0.0) {
    ConservationTrajectory t; State s=start; constexpr double dt=.025;
    for(int i=0;i<=400;++i) { if(i>=40 && i%4==0) t.states.push_back({s.y,s.v}); s=rk4(s,system,dt,damping); }
    return t;
}
std::vector<ConservationTrajectory> split(int system, bool heldout, bool synthetic=false) {
    const std::vector<State> train{{1,0},{-1,0},{0,1},{0,-1},{1,1},{-1,-1}};
    const std::vector<State> test{{2,0},{0,-.5}};
    std::vector<ConservationTrajectory> result;
    for(auto s:(synthetic?std::vector<State>{{1,0},{0,1},{-1,0},{0,-1}}:(heldout?test:train)))
        result.push_back(trajectory(s,system));
    return result;
}
std::vector<RegressionRow> flatten(const std::vector<ConservationTrajectory>& ts) {
    std::vector<RegressionRow> rows;
    for(const auto& t:ts) for(const auto& s:t.states) rows.push_back({s,{0.0}});
    return rows;
}
SymbolicExpression combine(const SymbolicExpression& a,const SymbolicExpression& b,ExpressionKind op) {
    std::vector<SymbolicNode> nodes(a.nodes().begin(),a.nodes().end());
    const auto off=nodes.size();
    for(auto n:b.nodes()) { for(std::size_t j=0;j<expressionArity(n.kind);++j)n.children[j]+=off; nodes.push_back(n); }
    SymbolicNode root; root.kind=op; root.children={a.root(),off+b.root(),0}; nodes.push_back(root);
    auto assumptions=std::vector<SymbolicAssumption>(a.assumptions().begin(),a.assumptions().end());
    return SymbolicExpression(nodes,nodes.size()-1,2,{},assumptions);
}
bool exactLieZero(const Expression& expression,int system) {
    auto e=SymbolicExpression::fromExpression(expression);
    auto dy=e.differentiate(0),dv=e.differentiate(1);
    if(dy.status!=SymbolicStatus::Success||dv.status!=SymbolicStatus::Success)return false;
    auto fy=SymbolicExpression::fromExpression(parseExpression("x1",2));
    auto fv=SymbolicExpression::fromExpression(parseExpression(system==0?"(-1)*x0":"(-2)*x0*x0*x0",2));
    auto left=combine(*dy.expression,fy,ExpressionKind::Multiply);
    auto right=combine(*dv.expression,fv,ExpressionKind::Multiply);
    auto sum=combine(left,right,ExpressionKind::Add).expandPolynomial();
    if(sum.status!=SymbolicStatus::Success)return false;
    return sum.expression->nodes()[sum.expression->root()].kind==ExpressionKind::Constant &&
           sum.expression->nodes()[sum.expression->root()].value.zero();
}
bool exactNonconstant(const Expression& expression) {
    auto expanded=SymbolicExpression::fromExpression(expression).expandPolynomial();
    return expanded.status==SymbolicStatus::Success &&
           expanded.expression->nodes()[expanded.expression->root()].kind!=ExpressionKind::Constant;
}
double lawValue(int system,double y,double v) { return system==0?y*y+v*v:y*y*y*y+v*v; }
double generatorError(int system,bool smoke) {
    const auto starts=smoke?std::vector<State>{{1,0},{0,1},{-1,0},{0,-1}}:
        std::vector<State>{{1,0},{-1,0},{0,1},{0,-1},{1,1},{-1,-1},{2,0},{0,-.5}};
    double maximum=0;
    for(auto initial:starts){State coarse=initial,fine=initial;for(int i=0;i<=400;++i){if(i>=40&&i%4==0)maximum=std::max(maximum,std::hypot(coarse.y-fine.y,coarse.v-fine.v));coarse=rk4(coarse,system,.025);fine=rk4(fine,system,.0125);fine=rk4(fine,system,.0125);}}
    return maximum;
}
struct Heldout { double within=0, separation=0; bool finite=true; };
Heldout heldoutMetrics(const Expression& e,const std::vector<ConservationTrajectory>& groups) {
    std::vector<std::vector<double>> values; double scale=0;
    genetik_detail::EvaluationWorkspace workspace;
    for(const auto& g:groups){auto& out=values.emplace_back();for(const auto&s:g.states){auto p=workspace.evaluate(e,s);if(!p.valid()){return {0,0,false};}scale=std::max(scale,std::abs(p.value()));out.push_back(p.value());}}
    if(!(scale>1e-12))return {0,0,false};
    std::vector<double> means;double grand=0;std::size_t n=0;for(auto&g:values){double m=0;for(auto&v:g){v/=scale;m+=v;}m/=g.size();means.push_back(m);grand+=m*g.size();n+=g.size();}grand/=n;
    double within=0,between=0;for(size_t i=0;i<values.size();++i){for(double v:values[i])within+=(v-means[i])*(v-means[i]);between+=values[i].size()*(means[i]-grand)*(means[i]-grand);}
    const double total=within+between;return {total>0?within/total:0,std::sqrt(between/n)*scale,true};
}
void report(int system,unsigned seed,bool smoke) {
    auto training=split(system,false,smoke); auto heldout=split(system,true);
    auto problem=std::make_shared<const ConservationDiscoveryProblem>(training,ExpressionLimits{31,7,16});
    SymbolicRegressionConfig cfg; cfg.population=32; cfg.elites=2; cfg.maxGenerationAttempts=20;
    cfg.maxEvaluations=640; cfg.maxTrainingLossAttempts=640*problem->sampleCount();
    cfg.grammar.operators={ExpressionKind::Add,ExpressionKind::Subtract,ExpressionKind::Multiply};
    cfg.grammar.constants={-2,-1,-.5,.5,1,2}; cfg.grammar.initialMaxDepth=4; cfg.grammar.mutationMaxDepth=3;
    RandomContext random(seed);
    auto result=symbolicRegression(problem,cfg,random);
    std::cout<<"{\"system\":\""<<(system==0?"harmonic":"quartic")<<"\",\"seed\":"<<seed
             <<",\"stop\":\""<<searchStopName(result.stoppingReason)<<"\",\"expression\":\"";
    if(!result.best){std::cout<<"\",\"valid\":false,\"candidate_evaluations\":"<<result.candidateEvaluations<<"}\n";return;}
    const auto& e=result.best->expression(); const auto metrics=heldoutMetrics(e,heldout);
    std::cout<<e.toString()<<"\",\"valid\":true,\"nodes\":"<<e.nodeCount()
             <<",\"candidate_evaluations\":"<<result.candidateEvaluations
             <<",\"training_rows\":"<<result.trainingWork.rowAttempts
             <<",\"generator_rk4_refinement_error\":"<<generatorError(system,smoke)
             <<",\"heldout_within_fraction\":"<<(metrics.finite?metrics.within:-1)
             <<",\"heldout_separation\":"<<(metrics.finite?metrics.separation:-1)
             <<",\"heldout_metrics_valid\":"<<(metrics.finite?"true":"false")<<"}\n";
}
void verify(int system,const std::string& text) {
    const auto e=parseExpression(text,2,{},ExpressionLimits{31,7,16});
    bool affine=false;const auto groups=split(system,true);std::vector<double> q,h;genetik_detail::EvaluationWorkspace workspace;
    for(const auto&t:groups)for(const auto&s:t.states){auto p=workspace.evaluate(e,s);if(p.valid()){q.push_back(p.value());h.push_back(lawValue(system,s[0],s[1]));}}
    if(q.size()>2){double qm=0,hm=0;for(auto x:q)qm+=x;for(auto x:h)hm+=x;qm/=q.size();hm/=h.size();double cov=0,var=0,scale=1;for(size_t i=0;i<q.size();++i){cov+=(q[i]-qm)*(h[i]-hm);var+=(q[i]-qm)*(q[i]-qm);scale=std::max(scale,std::abs(h[i]));}double a=var?cov/var:0,b=hm-a*qm,err=0;for(size_t i=0;i<q.size();++i)err=std::max(err,std::abs(h[i]-(a*q[i]+b)));affine=std::abs(a)>1e-10&&err<1e-8*scale;}
    std::cout<<"{\"expression\":\""<<text<<"\",\"nonconstant\":"<<(exactNonconstant(e)?"true":"false")
             <<",\"lie_derivative_identically_zero\":"<<(exactLieZero(e,system)?"true":"false")
             <<",\"reference_law_affine_recovery\":"<<(affine?"true":"false")<<"}\n";
}
}
int main(int argc,char**argv){
    if(argc==4&&std::string(argv[1])=="verify") { std::cout<<std::setprecision(17);verify(std::string(argv[2])=="harmonic"?0:1,argv[3]);return 0; }
    if(argc!=4){std::cerr<<"usage: conservation_study harmonic|quartic|damped seed synthetic|target OR verify system expression\n";return 2;}
    int system=std::string(argv[1])=="harmonic"?0:1; unsigned seed=static_cast<unsigned>(std::stoul(argv[2]));
    const bool synthetic=std::string(argv[3])=="synthetic";
    std::cout<<std::setprecision(17);
    if(std::string(argv[1])=="damped") {
        const auto t=trajectory({1,0},0,.15);const double a=lawValue(0,t.states.front()[0],t.states.front()[1]),b=lawValue(0,t.states.back()[0],t.states.back()[1]);
        std::cout<<"{\"system\":\"damped\",\"energy_start\":"<<a<<",\"energy_end\":"<<b<<",\"energy_change\":"<<(b-a)<<"}\n";return a!=b?0:1;
    }
    report(system,seed,synthetic);return 0;
}
