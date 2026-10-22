#include "../examples/PrimeExactOptions.hpp"
#include <ExpressionParsing.hpp>
#include <barrier>
#include <sstream>
using namespace prime_example;
static void check(bool c,const char* m){if(!c)throw std::runtime_error(m);}
template<class F>static void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,"expected checkpoint rejection");}
static SymbolicRegressionConfig fixture(unsigned generations){auto c=config(100000);c.population=8;c.elites=1;c.maxGenerationAttempts=generations;c.grammar.initialMaxDepth=2;c.grammar.mutationMaxDepth=2;c.grammar.operators={ExpressionKind::Add,ExpressionKind::Multiply};c.fitting->maxLossAttempts=4;return c;}
static ExpressionLimits limits(){return {3,2,8};}
static void formatTests(){
    const auto e=[](std::string_view s){return parseExpression(s,2);};
    for(auto loss:{0.,1.,500.,1000.,std::nextafter(1001.,0.),1001.,std::numeric_limits<double>::denorm_min()}){const auto f=exactPresentationFitness(loss);check(f>=0&&f<=1&&(f==1)==(loss==0),"presentation fitness boundary");}
    check(exactPresentationFitness(20)>exactPresentationFitness(21),"presentation direction");
    auto huge=Expression::constant(8,1,{31,6,8});for(unsigned i=0;i<4;++i)huge=Expression::binary(ExpressionKind::Multiply,huge,huge,{31,6,8});
    ExactPrefixProblem extremes(std::vector<std::uint32_t>(1000,1));
    check(extremes.measure(huge).loss==Fitness::valid(1001)&&exactPresentationFitness(1001)==0,"binary64 secondary saturation preserves original objective and valid endpoint");

    rejects([]{exactPresentationFitness(std::nextafter(1001.,2000.));});rejects([]{exactPresentationFitness(-1);});check(exactPresentationFitness(.5)==1-.5/1001,"presentation fractional synthetic input");
    check(formatExpression(e("x0 + (x1 + 2)"))=="x0 + (x1 + 2)","right-associated addition remains grouped");
    check(formatExpression(e("x0*(x1*2)"))=="x0·(x1·2)","right-associated multiplication remains grouped");
    check(formatExpression(e("(x0+x1)*2"))=="(x0 + x1)·2","precedence");
    check(formatExpression(e("x0/(x1/2)"))=="x0 / (x1 / 2)","division grouping");
    check(formatExpression(e("x0-(-2)"))=="x0 − (−2)","negative right operand");
    check(formatExpression(e("powi((-2),2)"))=="(−2)²","negative power base");
    check(formatExpression(e("log(x0)+sqrt(x1)"))=="ln(x0) + √(x1)","scientific notation");
    check(formatExpression(e("x0/x1"),ExpressionTextFormat::LaTeX)=="\\frac{x_{0}}{x_{1}}","latex division");
    for(auto s:{"sin(x0)","cos(x0)","exp(x0)","sqrt(x0)","log(x0)","powi(x0,0)","powi(x0,8)","(-0.0)","1e-300"}){
        const auto value=e(s),copy=value;const auto canonical=value.toString();for(auto f:{ExpressionTextFormat::Canonical,ExpressionTextFormat::Unicode,ExpressionTextFormat::LaTeX})check(!formatExpression(value,f).empty(),"all scientific formats");check(value.sameRepresentation(copy)&&value.toString()==canonical,"formatter does not edit tree");
    }
    const auto prime=parseExpression("x0+1",1);check(exactEquation(prime,ExpressionTextFormat::Unicode)=="p̂(n) = 10000 · ((n/1000) + 1)","decoded scale groups complete model");
    rejects([&]{formatExpression(prime,ExpressionTextFormat::Unicode,{},ExpressionFormatLimits{2,10});});
    // Winning presentation and signed deltas are deterministic engineering fixtures.
    auto p=std::make_shared<const ExactPrefixProblem>(std::vector<std::uint32_t>{10000});ExpressionCandidate c(Expression::constant(1,1),p);c.calculateFitness();
    ExactProgress progress;progress.generation=7;progress.seed=42;progress.population=1;progress.allowance=100;progress.losses=50;
    ExactReportingWork work;ExactDisplayState prior;prior.previousLoss=1;prior.previousMatches=0;std::ostringstream text,json;
    exactHumanSnapshot(text,{c},p->referenceValues(),progress,1,90,"winning","exact_training_fit",work,prior,&json,c,"out/example/latest.gtkc",7,2);
    check(text.str().find("EXACT PREFIX RECOVERED")!=std::string::npos&&text.str().find("1 / 1")!=std::string::npos&&text.str().find("↑ +1")!=std::string::npos&&text.str().find("Canonical g(x0)")!=std::string::npos&&text.str().find("LaTeX")!=std::string::npos,"complete success block");
    check(json.str().find("genetik.prime-progress/1")!=std::string::npos&&json.str().find("presentation_fitness")!=std::string::npos,"versioned JSONL record");
}
class Gated final:public ExactPrefixProblem {
    mutable std::atomic<unsigned> starts_{0};mutable std::barrier<> gate_{2};
public:Gated():ExactPrefixProblem({1,4,13},::limits()){}
    bool parallelEvaluationSafe() const noexcept override{return true;}
    RegressionMetrics measure(const Expression& e,std::uint64_t* rows=nullptr)const override{if(starts_.fetch_add(1)<2)gate_.arrive_and_wait();return ExactPrefixProblem::measure(e,rows);}
};
static std::string summary(const ExactSearchResult& result,const RandomContext& random){
    CheckpointWriter w;w.integer(*result.bestGeneration);w.integer(result.attempts);w.integer(result.published);w.integer(result.candidateRequests);w.integer(result.unpublishedLosses);
    for(const auto& c:result.population)CandidateCheckpointCodec<ExpressionCandidate>::encode(w,c);
    CandidateCheckpointCodec<ExpressionCandidate>::encode(w,*result.best);
    writeTrainingWork(w,result.training);writePreparationStatistics(w,result.preparation);writeVariationStatistics(w,result.variation);
    for(const auto& h:result.history){w.integer(h.lossAttempts);w.integer(h.generation);w.real(h.loss);w.text(h.canonical);}
    w.text(RunCheckpointAccess::random(random).engine);return w.data();
}
static std::string phase(const std::filesystem::path& dir,unsigned generations,unsigned threads,std::optional<CheckpointDocument> resume={}){
    auto cfg=fixture(generations);std::shared_ptr<const ExactPrefixProblem> problem=threads>1?std::make_shared<const Gated>():std::make_shared<const ExactPrefixProblem>(std::vector<std::uint32_t>{1,4,13},limits());
    RandomContext random(2,ExecutionConfig{threads});ExactRunOptions io;std::ostringstream metrics;io.pretty=true;io.metrics=&metrics;io.checkpointDirectory=dir;io.resume=std::move(resume);io.checkpointInterval=std::chrono::hours(24);
    std::ostringstream out;const auto result=runExactSearch(problem,cfg,random,out,1,std::chrono::seconds(30),true,[]{return ExactClock::now();},io);
    check(result.published==generations&&result.stop==SearchStop::GenerationLimit,"bounded fixture reaches declared generation without optimum gate");return summary(result,random);
}
static void equivalence(const std::filesystem::path& root,bool parallel){
    for(unsigned n:parallel?std::vector<unsigned>{2,4}:std::vector<unsigned>{1}){
        const auto dir=root/("threads"+std::to_string(n));
        const auto continuous=phase(dir/"continuous",100,n);
        (void)phase(dir/"split",40,n); // all run objects destroyed by return
        const auto document=loadCheckpoint(dir/"split/latest.gtkc");
        const auto resumed=phase(dir/"split",100,n,document);
        check(continuous==resumed,"100 vs40+resume60 exact population/coefficients/fitness/archive/RNG/work/counters/history equivalence");
    }
}
static void fileTests(const std::filesystem::path& root){
    CheckpointDocument d;d.codec="custom.example/1";d.identity="owned-problem";d.payload="first";
    auto bytes=encodeCheckpoint(d);check(decodeCheckpoint(bytes).payload=="first","envelope roundtrip");
    for(auto broken:{bytes.substr(0,bytes.size()-1),bytes+"junk",std::string("GENETIK-CHECKPOINT 2\n0\n0\n"),std::string("GENETIK-CHECKPOINT 0\n0\n0\n")})rejects([&]{decodeCheckpoint(broken);});
    auto corrupt=bytes;corrupt.back()^=1;rejects([&]{decodeCheckpoint(corrupt);});
    auto checksum=bytes;checksum[21]=checksum[21]=='f'?'0':'f';rejects([&]{decodeCheckpoint(checksum);});
    rejects([&]{decodeCheckpoint(bytes,CheckpointLimits{2,2,2});});
    CheckpointWriter fields;fields.integer(0x0102030405060708ULL);fields.real(-0.0);CheckpointReader values(fields.data());check(values.integer()==0x0102030405060708ULL&&std::signbit(values.real()),"fixed-endian integer and signed binary64 fields");values.finish();
    auto incompatible=d;incompatible.runtime="other-runtime";rejects([&]{decodeCheckpoint(encodeCheckpoint(incompatible));});
    auto scalarProblem=RegressionProblem::create({{{1},1}},{64,16,std::numeric_limits<double>::max()});
    for(auto source:{"x0+1","x0-1","x0*x0","x0/0","log(x0)","sin(x0)","cos(x0)","exp(x0)","sqrt(x0)","powi(x0,0)","powi(x0,1)","powi(x0,8)","(-0.0)","1e-300","1.7976931348623157e308","1+1"}){
        ExpressionCandidate original(parseExpression(source,1,{},scalarProblem->limits()),scalarProblem);original.calculateFitness();CheckpointWriter writer;CandidateCheckpointCodec<ExpressionCandidate>::encode(writer,original);
        CheckpointReader reader(writer.data());const auto copy=CandidateCheckpointCodec<ExpressionCandidate>::decode(reader,ScalarCheckpointContext{scalarProblem,{}});reader.finish();check(copy.expression().sameRepresentation(original.expression())&&copy.evaluation()==original.evaluation(),"all scalar codec node/constant bits and fitness outcomes");
    }
    const auto file=root/"atomic.gtkc";saveCheckpointAtomic(file,d);
    auto replacement=d;replacement.payload="second";
    rejects([&]{saveCheckpointAtomic(file,replacement,[]{throw std::runtime_error("injected failure before rename");});});
    check(loadCheckpoint(file).payload=="first","previous checkpoint survives interrupted replacement");
    rejects([&]{saveCheckpointAtomic(file,replacement,[]{throw std::bad_alloc();});});
    check(loadCheckpoint(file).payload=="first","allocation failure before replacement retains previous checkpoint");
    rejects([&]{loadCheckpoint(root/"absent.gtkc");});rejects([&]{saveCheckpointAtomic(root/"missing/invalid.gtkc",d);});
    for(unsigned g=0;g<5;++g)saveCheckpointSeries(root/"series",g,d,3);
    std::size_t numbered=0;for(const auto& e:std::filesystem::directory_iterator(root/"series"))if(e.path().filename().string().starts_with("generation-"))++numbered;
    check(numbered==3&&loadCheckpoint(root/"series/latest.gtkc").generation==4,"bounded retention after good replacement");
    rejects([&]{saveCheckpointSeries(root/"series",3,d,3);});
    d.identity="another problem";rejects([&]{saveCheckpointSeries(root/"series",5,d,3);});
    check(loadCheckpoint(root/"series/latest.gtkc").generation==4,"incompatible save leaves known-good latest");
}
class Interrupt final:public ExactPrefixProblem {
    std::atomic<bool>& flag_;mutable unsigned calls_=0;
public:explicit Interrupt(std::atomic<bool>& flag):ExactPrefixProblem({1,4,13},::limits(),&flag),flag_(flag){}
    RegressionMetrics measure(const Expression& e,std::uint64_t* rows=nullptr)const override{if(++calls_==4)flag_.store(true);return ExactPrefixProblem::measure(e,rows);}
};
static void interruption(const std::filesystem::path& root){
    (void)phase(root/"interrupt",40,1);auto clean=loadCheckpoint(root/"interrupt/latest.gtkc");auto before=decodeExactRun(clean);
    auto cfg=fixture(100);std::atomic<bool> cancel{false};auto problem=std::make_shared<const Interrupt>(cancel);RandomContext random(2);ExactRunOptions io;io.resume=clean;io.checkpointDirectory=root/"interrupt";
    std::ostringstream text;auto stopped=runExactSearch(problem,cfg,random,text,1,std::chrono::seconds(30),false,[]{return ExactClock::now();},io);
    check(stopped.stop==SearchStop::Cancelled&&stopped.published==40&&stopped.attempts==41&&stopped.training.lossAttempts>before.training.lossAttempts&&stopped.unpublishedLosses>0,"cancel/drain discards attempted G41 but retains consumed work");
    const auto saved=decodeExactRun(loadCheckpoint(root/"interrupt/latest.gtkc"));
    check(saved.random.engine==RunCheckpointAccess::random(random).engine&&saved.training==stopped.training,"final cancellation checkpoint exact committed state and post-failure RNG/work");
    CheckpointReader a(saved.populationBytes),b(before.populationBytes);
    const auto sa=readCommittedPopulation<ExpressionCandidate>(a,ScalarCheckpointContext{problem,{}}),sb=readCommittedPopulation<ExpressionCandidate>(b,ScalarCheckpointContext{problem,{}});
    check(sa.generation==sb.generation&&sa.bestGeneration==sb.bestGeneration&&sa.best.expression().sameRepresentation(sb.best.expression())&&sa.best.evaluation()==sb.best.evaluation()&&sa.lastOutcome==GenerationOutcome::OperationalFailure,"committed archive survives interrupted attempt");
    for(std::size_t i=0;i<sa.members.size();++i)check(sa.members[i].expression().sameRepresentation(sb.members[i].expression())&&sa.members[i].evaluation()==sb.members[i].evaluation(),"all committed candidate data/fitness survive interrupted attempt");
    // The failed attempt consumed scheduling/work; resume retries committed G+1.
    auto regular=std::make_shared<const ExactPrefixProblem>(std::vector<std::uint32_t>{1,4,13},limits());RandomContext next(2);cfg.maxGenerationAttempts=42;io.resume=loadCheckpoint(root/"interrupt/latest.gtkc");
    auto continued=runExactSearch(regular,cfg,next,text,1,std::chrono::seconds(30),false,[]{return ExactClock::now();},io);
    check(continued.published==41&&continued.attempts==42&&continued.training.lossAttempts>saved.training.lossAttempts,"continuation after failed private generation uses next committed schedule");
    auto invalid=clean;invalid.codec="wrong";rejects([&]{decodeExactRun(invalid);});
    auto bad=before;bad.algorithm.attempts=0;rejects([&]{decodeExactRun(encodeExactRun(bad,clean.identity));});
    bad=before;bad.training.lossAttempts=bad.allowance+1;rejects([&]{decodeExactRun(encodeExactRun(bad,clean.identity));});
    bad=before;bad.preparation.origins[0].rows++;rejects([&]{validateExactWork(bad,3);});
    const auto wrongProblem=std::make_shared<const ExactPrefixProblem>(std::vector<std::uint32_t>{1,4,14},limits());RandomContext mismatch(2);io.resume=clean;io.checkpointDirectory.clear();
    rejects([&]{runExactSearch(wrongProblem,fixture(100),mismatch,text,1,std::chrono::seconds(30),false,[]{return ExactClock::now();},io);});check(mismatch.evaluationAttempts()==0&&mismatch.engine()==std::mt19937(2),"failed identity validation leaves fresh RNG unchanged");
    auto incompatible=fixture(100);incompatible.mutationRate=.5;rejects([&]{runExactSearch(regular,incompatible,mismatch,text,1,std::chrono::seconds(30),false,[]{return ExactClock::now();},io);});
    incompatible=fixture(100);incompatible.maxTrainingLossAttempts=before.training.lossAttempts-1;rejects([&]{runExactSearch(regular,incompatible,mismatch,text,1,std::chrono::seconds(30),false,[]{return ExactClock::now();},io);});
}
int main(int argc,char** argv){try{
    if(argc<3)throw std::invalid_argument("mode and output required");
    const auto root=std::filesystem::path(argv[2]);
    // Caller supplies a designated test-owned build/temp directory.
    if(root.filename()!="checkpoint-fixture")throw std::invalid_argument("unsafe checkpoint fixture directory");
    const std::string mode=argv[1];
    if(mode=="phase40"||mode=="phase100"||mode=="phase-resume"){
        std::filesystem::create_directories(root);std::string result;
        if(mode=="phase40")result=phase(root/"split",40,1);
        else if(mode=="phase100")result=phase(root/"continuous",100,1);
        else result=phase(root/"split",100,1,loadCheckpoint(root/"split/latest.gtkc"));
        std::ofstream out(root/(mode+".state"),std::ios::binary);out.write(result.data(),static_cast<std::streamsize>(result.size()));if(!out)throw std::runtime_error("fixture state write");return 0;
    }
    std::filesystem::remove_all(root);std::filesystem::create_directories(root);
    if(mode=="format")formatTests();else if(mode=="overlap")equivalence(root,true);else{fileTests(root);equivalence(root,false);interruption(root);}
    std::cout<<"checkpoint "<<mode<<" passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
