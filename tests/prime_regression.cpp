#include "../examples/PrimeRegression.hpp"
#include "../examples/RegressionData.hpp"
using namespace prime_example;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class F>static void rejects(F f){bool caught=false;try{f();}catch(const std::invalid_argument&){caught=true;}check(caught,"expected API error");}
static void close(double a,double b,double tolerance=1e-12){check(std::abs(a-b)<=tolerance,"independent numerical expectation");}
int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::invalid_argument("test workspace required");
        const std::filesystem::path dir=argv[1];
        const auto ref=reference();check(ref.size()==1200 && ref[0]==2 && ref[99]==541 && ref[999]==7919 && ref[1000]==7927 && ref[1199]==9733,"reference anchors");
        exportData(dir,ref);const auto supplied=loadPrefix((dir/"first-1000.txt").string(),ref);check(supplied.size()==1000,"supplied prefix");
        auto wrong=supplied;wrong[10]=wrong[11];rejects([&]{validatePrefix(wrong,ref);});
        wrong=supplied;std::swap(wrong[0],wrong[1]);rejects([&]{validatePrefix(wrong,ref);});
        wrong.pop_back();rejects([&]{validatePrefix(wrong,ref);});
        for(const std::string malformed:{"", "2\n3\n", "2.0\n", "-2\n", "+2\n", "4294967296\n", "2junk\n", "\"2\"\n", "2 3\n"}) {
            {auto out=output(dir/"bad.txt");out<<malformed;}
            rejects([&]{(void)loadPrefix((dir/"bad.txt").string(),ref);});
        }
        {auto out=output(dir/"edges.txt");out<<"\r\n";for(auto value:supplied)out<<" \t"<<value<<"\t \r\n";}
        check(loadPrefix((dir/"edges.txt").string(),ref)==supplied,"CRLF and whitespace");
        {auto out=output(dir/"bad.txt");for(auto value:supplied)out<<value<<'\n';out<<7927;}
        rejects([&]{(void)loadPrefix((dir/"bad.txt").string(),ref);});
        {auto out=output(dir/"bad.txt");out<<std::string(65537,' ');}
        rejects([&]{(void)loadPrefix((dir/"bad.txt").string(),ref);});
        rejects([&]{(void)loadPrefix((dir/"missing.txt").string(),ref);});
        const auto fitting=rows(ref);check(fitting.size()==1000 && fitting.front().inputs[0]==.001 && fitting.front().target==.0002 && fitting.back().inputs[0]==1 && fitting.back().target==.7919,"one-based scaling");
        const auto expression=Expression::variable(0);close(*decode(expression,1),10);close(*decode(expression,1000),10000);
        check(!decode(Expression::binary(ExpressionKind::Divide,expression,Expression::constant(0,1)),1),"decode invalid arithmetic");
        check(!decode(Expression::constant(std::numeric_limits<double>::max(),1),1),"decode overflow");
        check(rounded(1.5)==2 && rounded(-1.5)==-2 && rounded(-.5)==-1 && rounded(.5)==1,"rounding ties away from zero");
        check(rounded(-0x1p63)==std::numeric_limits<std::int64_t>::min(),"inclusive lower integer boundary");
        check(!rounded(0x1p63) && rounded(std::nextafter(0x1p63,0)) && !rounded(std::nextafter(-0x1p63,-INFINITY)),"exclusive upper integer boundary");
        check(!rounded(INFINITY) && !rounded(NAN),"nonfinite conversion rejected");
        const auto m=metrics({prediction(1,2,2),prediction(2,3,4),prediction(3,5,4)});
        close(*m.mse,2./3);close(*m.rmse,std::sqrt(2./3));close(*m.mae,2./3);close(*m.maximum,1);close(*m.meanRelative,8./45);close(*m.maxRelative,1./3);
        check(m.matches==1 && m.count==3 && m.duplicates==1 && m.rawPairs==2 && m.rawViolations==1 && m.roundedViolations==1,"index matches and duplicates");
        check(metrics({prediction(1,2,2),prediction(2,3,3),prediction(3,5,2)}).duplicates==1,"nonadjacent duplicate occurrences");
        const auto invalid=metrics({prediction(1,2,2),prediction(2,3,{}),prediction(3,5,5)});
        check(!invalid.mse && !invalid.mae && invalid.matches==2 && invalid.count==3 && invalid.invalid==1 && invalid.rawPairs==0 && invalid.roundedPairs==0,"invalids retain denominator and do not bridge pairs");
        const auto negative=metrics({prediction(1,2,-1.5),prediction(2,3,-1.5)});
        check(negative.negative==2 && negative.invalid==0 && negative.duplicates==1 && negative.roundedViolations==1,"negative outputs are numerical predictions");
        const auto range=metrics({prediction(1,2,0x1p63)});check(range.mse && range.unrepresentable==1 && !range.invalid && !range.matches,"integer range separate from continuous validity");
        const auto huge=metrics({prediction(1,2,std::numeric_limits<double>::max())});check(!huge.mse && !huge.invalid,"raw loss overflow not favorable");
        // Independent small full-rank QR fixtures, not comparison against the GP fitter.
        const auto affine=leastSquares({{1,-1},{1,0},{1,1}},{-1,2,5});close(affine[0],2);close(affine[1],3);
        const auto mean=leastSquares({{1},{1},{1}},{1,2,6});close(mean[0],3);
        std::vector<std::vector<double>> design;std::vector<double> y;
        for(double x:{-2.,-1.,0.,1.,2.}){design.push_back({1,x,x*x,x*x*x});y.push_back(2-3*x+.5*x*x+1.25*x*x*x);}
        const auto cubic=leastSquares(design,y);close(cubic[0],2);close(cubic[1],-3);close(cubic[2],.5);close(cubic[3],1.25);
        design.clear();y.clear();for(double u:{.1,.2,.4,.6,.8,1.}){design.push_back({u*std::log(u),u,1});y.push_back(.2*u*std::log(u)+.8*u+.01);}
        const auto logarithmic=leastSquares(design,y);close(logarithmic[0],.2);close(logarithmic[1],.8);close(logarithmic[2],.01);
        rejects([]{(void)leastSquares({{1,1},{1,1}},{1,2});});
        const auto fittedReferences=baselines(fitting);check(fittedReferences.size()==5,"five baseline families");
        // Frozen training choice stays selected even when a different candidate scores better later.
        const auto small=RegressionProblem::create({{{.1},.2},{{.2},.2}},{31,6,8});
        SymbolicRegressionResult first,second;first.best.emplace(Expression::constant(.2,1),small);second.best.emplace(Expression::constant(.3,1),small);
        first.best->calculateFitness();second.best->calculateFitness();check(better(first,42,second,0),"training selects better loss");
        const auto frozenText=first.best->text();const auto later=RegressionProblem::create({{{1.1},.3}},{31,6,8});
        check(later->measure(second.best->expression()).loss.score()<later->measure(first.best->expression()).loss.score(),"reversed evaluation fixture");
        check(first.best->text()==frozenText && better(first,42,second,0),"assessment cannot replace frozen fitting choice");
        check(better(first,0,first,42) && !better(first,42,first,0),"stable seed tie");
        const auto parsed=regression_example::loadData((dir/"fitting.csv").string(),{"u"},"v");
        for(std::size_t i=0;i<fitting.size();++i)check(parsed.rows[i].inputs==fitting[i].inputs && parsed.rows[i].target==fitting[i].target,"binary64 table round trip");
        // Same public engine/fitting path, short bounded run: correctness, never recovery.
        const auto p1=RegressionProblem::create(fitting,{31,6,8}),p2=RegressionProblem::create(parsed.rows,{31,6,8});
        auto c=config(128);c.population=4;c.elites=1;c.maxGenerationAttempts=1;c.fitting->maxLossAttempts=8;
        RandomContext r1(42),r2(42);const auto a=symbolicRegression(p1,c,r1),b=symbolicRegression(p2,c,r2);
        check(a.best && b.best && a.best->text()==b.best->text() && a.best->fitness()==b.best->fitness() && a.trainingWork==b.trainingWork && a.variation==b.variation && a.candidateEvaluations==b.candidateEvaluations && a.attemptedGenerations==b.attemptedGenerations && a.publishedGenerations==b.publishedGenerations && a.bestGeneration==b.bestGeneration && a.stoppingReason==b.stoppingReason,"parsed/in-memory deterministic search");
        check(a.trainingWork.lossAttempts<=128 && a.trainingWork.rowAttempts<=128000,"bounded loss and row work");
        std::cout<<"prime data, scaling, QR, metrics, frozen selection and public search contracts passed\n";return 0;
    }catch(const std::exception& e){regression_example::printError(e);return 1;}
}
