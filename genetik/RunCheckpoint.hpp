#pragma once
// Generic execution state and explicitly coded, bounded checkpoint storage.
// No symbolic-regression include or implicit serialization of external classes.
#include "Version.hpp"
#include "Population.hpp"
#include "GeneticAlgorithm.hpp"
#include <bit>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <system_error>
#include <set>
#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <unistd.h>
#endif

struct CheckpointError:std::runtime_error {using std::runtime_error::runtime_error;};
struct CheckpointLimits {std::size_t maxBytes=64*1024*1024,maxStringBytes=1024*1024,maxCandidates=100000;};
class CheckpointWriter {
    std::string data_;std::size_t maximum_;
public:
    explicit CheckpointWriter(std::size_t maximum=64*1024*1024):maximum_(maximum){if(!maximum)throw std::invalid_argument("zero checkpoint limit");}
    void bytes(std::string_view value){if(value.size()>maximum_-data_.size())throw CheckpointError("checkpoint size limit");data_.append(value);}
    void integer(std::uint64_t value){char b[8];for(unsigned i=0;i<8;++i)b[7-i]=static_cast<char>((value>>(8*i))&255);bytes({b,8});}
    void real(double value){static_assert(sizeof(double)==8 && std::numeric_limits<double>::is_iec559);integer(std::bit_cast<std::uint64_t>(value));}
    void text(std::string_view value){integer(value.size());bytes(value);}
    const std::string& data() const noexcept{return data_;}
};
class CheckpointReader {
    std::string_view data_;std::size_t offset_=0;CheckpointLimits limits_;
public:
    explicit CheckpointReader(std::string_view data,CheckpointLimits limits={}):data_(data),limits_(limits){if(!limits.maxBytes||!limits.maxStringBytes||!limits.maxCandidates||data.size()>limits.maxBytes)throw CheckpointError("checkpoint size limit");}
    std::uint64_t integer(){if(data_.size()-offset_<8)throw CheckpointError("truncated checkpoint field");std::uint64_t n=0;for(unsigned i=0;i<8;++i)n=(n<<8)|static_cast<unsigned char>(data_[offset_++]);return n;}
    double real(){return std::bit_cast<double>(integer());}
    std::size_t count(std::size_t maximum){const auto n=integer();if(n>maximum || n>std::numeric_limits<std::size_t>::max())throw CheckpointError("checkpoint count limit");return static_cast<std::size_t>(n);}
    std::string text(std::size_t maximum){const auto n=count(std::min(maximum,limits_.maxBytes));if(n>data_.size()-offset_)throw CheckpointError("truncated checkpoint string");std::string s(data_.substr(offset_,n));offset_+=n;return s;}
    std::string text(){return text(limits_.maxStringBytes);}
    void finish() const {if(offset_!=data_.size())throw CheckpointError("trailing checkpoint data");}
    const CheckpointLimits& limits()const noexcept{return limits_;}
};
inline std::uint64_t checkpointChecksum(std::string_view data) noexcept {
    std::uint64_t hash=14695981039346656037ULL;
    for(unsigned char b:data){hash^=b;hash*=1099511628211ULL;}return hash;
}
inline std::string checkpointHex(std::uint64_t n){char b[16];auto r=std::to_chars(b,b+16,n,16);return std::string(b,r.ptr);}
inline std::string checkpointRuntime() {
    std::ostringstream out;out.imbue(std::locale::classic());
    out<<std::string("Genetik-")+genetikVersion+"/run-state-1;compiler=";
#ifdef __VERSION__
    out<<__VERSION__;
#elif defined(_MSC_FULL_VER)
    out<<"MSVC-"<<_MSC_FULL_VER;
#else
    out<<"unqualified";
#endif
#ifdef __GLIBCXX__
    out<<";glibcxx="<<__GLIBCXX__<<"/"<<_GLIBCXX_RELEASE;
#endif
#ifdef __GLIBC__
    out<<";glibc="<<__GLIBC__<<'.'<<__GLIBC_MINOR__;
#endif
#ifdef __OPTIMIZE__
    out<<";optimized=1";
#else
    out<<";optimized=0";
#endif
#ifdef NDEBUG
    out<<";ndebug=1";
#else
    out<<";ndebug=0";
#endif
#if defined(__linux__)
    // Record the actual loaded standard/runtime math object bytes, including
    // distro patches. No external commands, credentials or path-dependent IDs.
    std::ifstream maps("/proc/self/maps");std::set<std::string> paths;std::string line;std::size_t mapLines=0;
    while(std::getline(maps,line)){if(++mapLines>16384||line.size()>16384)throw CheckpointError("runtime maps limit");const auto start=line.find('/');if(start==line.npos)continue;const auto path=line.substr(start);const auto name=std::filesystem::path(path).filename().string();if(name.starts_with("libstdc++.")||name.starts_with("libm.")||name.starts_with("libc."))paths.insert(path);}
    for(const auto& path:paths){std::ifstream file(path,std::ios::binary);if(!file)throw CheckpointError("cannot fingerprint loaded runtime");std::uint64_t hash=14695981039346656037ULL;std::size_t bytes=0;char block[8192];while(file){file.read(block,sizeof(block));const auto n=static_cast<std::size_t>(file.gcount());if(n>64*1024*1024-bytes)throw CheckpointError("runtime fingerprint size limit");bytes+=n;for(std::size_t i=0;i<n;++i){hash^=static_cast<unsigned char>(block[i]);hash*=1099511628211ULL;}}if(!file.eof())throw CheckpointError("runtime fingerprint read failure");out<<';'<<std::filesystem::path(path).filename().string()<<'='<<checkpointHex(hash);}
#endif
    out<<";size_t="<<sizeof(std::size_t)<<";endian="<<(std::endian::native==std::endian::little?"little":"big")
       <<";round="<<std::fegetround();return out.str();
}
struct CheckpointDocument {
    std::string codec,identity,payload,runtime=checkpointRuntime();
    std::optional<std::uint64_t> generation{};
};
inline std::string encodeCheckpoint(const CheckpointDocument& document,CheckpointLimits limits={}) {
    if(document.runtime.size()>limits.maxStringBytes||document.codec.size()>limits.maxStringBytes||document.identity.size()>limits.maxStringBytes)throw CheckpointError("checkpoint metadata string limit");
    CheckpointWriter w(limits.maxBytes);w.text(std::string("Genetik-")+genetikVersion+"/run-state-1");w.text(document.runtime);w.text(document.codec);w.text(document.identity);w.integer(document.generation.has_value());if(document.generation)w.integer(*document.generation);w.text(document.payload);
    const auto& payload=w.data();const auto header="GENETIK-CHECKPOINT 1\n"+checkpointHex(checkpointChecksum(payload))+"\n"+std::to_string(payload.size())+"\n";
    if(header.size()>limits.maxBytes||payload.size()>limits.maxBytes-header.size())throw CheckpointError("checkpoint source byte limit");
    return header+payload;
}
inline CheckpointDocument decodeCheckpoint(std::string_view bytes,CheckpointLimits limits={}) {
    if(bytes.size()>limits.maxBytes)throw CheckpointError("checkpoint source byte limit");
    const auto a=bytes.find('\n'),b=a==bytes.npos?a:bytes.find('\n',a+1),c=b==bytes.npos?b:bytes.find('\n',b+1);
    if(a!=20 || b==bytes.npos || c==bytes.npos || c>128 || bytes.substr(0,a)!="GENETIK-CHECKPOINT 1")throw CheckpointError("unsupported or malformed checkpoint format");
    std::uint64_t checksum=0,size=0;auto h=std::from_chars(bytes.data()+a+1,bytes.data()+b,checksum,16);auto n=std::from_chars(bytes.data()+b+1,bytes.data()+c,size);
    if(h.ec!=std::errc{}||h.ptr!=bytes.data()+b||n.ec!=std::errc{}||n.ptr!=bytes.data()+c||size>limits.maxBytes||size!=bytes.size()-c-1)throw CheckpointError("checkpoint length/header mismatch");
    const auto payload=bytes.substr(c+1);if(checkpointChecksum(payload)!=checksum)throw CheckpointError("checkpoint checksum mismatch");
    CheckpointReader r(payload,limits);
    if(r.text()!=std::string("Genetik-")+genetikVersion+"/run-state-1")throw CheckpointError("incompatible Genetik checkpoint state version");
    CheckpointDocument d;d.runtime=r.text();d.codec=r.text();d.identity=r.text();if(r.count(1))d.generation=r.integer();d.payload=r.text(limits.maxBytes);r.finish();
    if(d.runtime!=checkpointRuntime())throw CheckpointError("checkpoint runtime/FP environment differs");
    return d;
}
inline CheckpointDocument loadCheckpoint(const std::filesystem::path& path,CheckpointLimits limits={}) {
    std::ifstream in(path,std::ios::binary);if(!in)throw CheckpointError("cannot open checkpoint: "+path.string());
    std::string bytes;char block[8192];
    while(in){in.read(block,sizeof(block));const auto n=static_cast<std::size_t>(in.gcount());if(bytes.size()>limits.maxBytes || n>limits.maxBytes-bytes.size())throw CheckpointError("checkpoint source byte limit");bytes.append(block,n);}
    if(!in.eof())throw CheckpointError("checkpoint read failure");
    return decodeCheckpoint(bytes,limits);
}
namespace genetik_detail {
// Sync directory entries, including the parent entry of each newly created
// directory. No file-format change; failures remain visible to the coordinator.
inline void syncCheckpointDirectory(const std::filesystem::path& path){
#if defined(__unix__) || defined(__APPLE__)
    const int fd=::open(path.c_str(),O_RDONLY|O_DIRECTORY);
    if(fd<0)throw std::system_error(errno,std::generic_category(),"checkpoint directory open");
    const int result=::fsync(fd),error=errno;::close(fd);
    if(result!=0)throw std::system_error(error,std::generic_category(),"checkpoint directory fsync");
#else
    (void)path;throw CheckpointError("durable checkpoint directories require qualified POSIX fsync");
#endif
}
inline void createCheckpointDirectories(const std::filesystem::path& path){
    std::vector<std::filesystem::path> missing;
    auto current=std::filesystem::absolute(path);
    while(!std::filesystem::exists(current)){
        missing.push_back(current);current=current.parent_path();
    }
    if(!std::filesystem::is_directory(current))throw CheckpointError("checkpoint ancestor is not a directory");
    for(auto i=missing.rbegin();i!=missing.rend();++i){
        if(!std::filesystem::create_directory(*i)&&!std::filesystem::is_directory(*i))throw CheckpointError("checkpoint directory creation failed");
        syncCheckpointDirectory(*i);syncCheckpointDirectory(i->parent_path());
    }
}
} // namespace genetik_detail

// POSIX persistence, no unqualified fallback claiming durability elsewhere.
// beforeRename is an optional fault-injection hook; it runs after validation/fsync.
inline void saveCheckpointAtomic(const std::filesystem::path& destination,const CheckpointDocument& document,
        const std::function<void()>& beforeRename={}) {
#if defined(__unix__) || defined(__APPLE__)
    const auto bytes=encodeCheckpoint(document);(void)decodeCheckpoint(bytes);
    auto directory=destination.parent_path();if(directory.empty())directory=".";
    std::string pattern=(directory/(destination.filename().string()+".tmp-XXXXXX")).string();
    std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);
    int fd=::mkstemp(name.data());if(fd<0)throw std::system_error(errno,std::generic_category(),"checkpoint temporary file");
    try {
        const std::filesystem::path temporary(name.data());
        std::size_t offset=0;
        while(offset<bytes.size()) {const auto n=::write(fd,bytes.data()+offset,bytes.size()-offset);if(n<0){if(errno==EINTR)continue;throw std::system_error(errno,std::generic_category(),"checkpoint write");}if(n==0)throw CheckpointError("checkpoint write made no progress");offset+=static_cast<std::size_t>(n);}
        if(::fsync(fd)!=0)throw std::system_error(errno,std::generic_category(),"checkpoint file fsync");
        if(::close(fd)!=0){fd=-1;throw std::system_error(errno,std::generic_category(),"checkpoint close");}fd=-1;
        const auto verified=loadCheckpoint(temporary);if(verified.payload!=document.payload||verified.identity!=document.identity||verified.codec!=document.codec)throw CheckpointError("checkpoint read-back mismatch");
        if(beforeRename)beforeRename();
        std::filesystem::rename(temporary,destination);
        genetik_detail::syncCheckpointDirectory(directory);
    }catch(...){if(fd>=0)::close(fd);::unlink(name.data());throw;}
#else
    (void)destination;(void)document;(void)beforeRename;throw CheckpointError("durable checkpoint writes require qualified POSIX fsync/rename");
#endif
}
// Directory is exclusively owned by this run. latest is a full checksummed file,
// not a symlink. At least one numbered checkpoint is retained; unrelated files stay.
inline std::filesystem::path saveCheckpointSeries(const std::filesystem::path& directory,std::uint64_t generation,
        const CheckpointDocument& document,std::size_t keep=3) {
    if(!keep || keep>10000)throw std::invalid_argument("checkpoint keep must be1..10000");
    genetik_detail::createCheckpointDirectories(directory);
    auto ready=document;if(ready.generation && *ready.generation!=generation)throw CheckpointError("checkpoint generation metadata differs");ready.generation=generation;
    const auto prior=directory/"latest.gtkc";
    if(std::filesystem::exists(prior)){const auto old=loadCheckpoint(prior);if(old.identity!=ready.identity||old.codec!=ready.codec||!old.generation||*old.generation>generation)throw CheckpointError("checkpoint directory identity differs or generation would rewind");}
    std::ostringstream name;name.imbue(std::locale::classic());name<<"generation-"<<std::setfill('0')<<std::setw(20)<<generation<<".gtkc";
    const auto numbered=directory/name.str();saveCheckpointAtomic(numbered,ready);
    const auto latest=directory/"latest.gtkc";saveCheckpointAtomic(latest,ready);
    std::vector<std::filesystem::path> files;
    for(const auto& e:std::filesystem::directory_iterator(directory)) {
        const auto n=e.path().filename().string();
        if(!e.is_symlink() && e.is_regular_file() && n.size()==36 && n.starts_with("generation-") && n.ends_with(".gtkc") &&
           std::all_of(n.begin()+11,n.begin()+31,[](char c){return c>='0'&&c<='9';}))files.push_back(e.path());
    }
    std::sort(files.begin(),files.end());const bool prune=files.size()>keep;
    while(files.size()>keep){std::filesystem::remove(files.front());files.erase(files.begin());}
    if(prune)genetik_detail::syncCheckpointDirectory(directory);
    return latest;
}

template<class T> struct CommittedPopulationCheckpoint {
    std::vector<T> members;T best;std::uint64_t generation,bestGeneration;ObjectiveDirection direction;GenerationOutcome lastOutcome=GenerationOutcome::Committed;
};
struct RandomCheckpoint {std::uint32_t seed=0;std::uint64_t candidateAttempts=0;std::string engine;unsigned threads=1;};
struct AlgorithmCheckpoint {std::uint64_t attempts;unsigned published;GenerationOutcome lastOutcome=GenerationOutcome::Committed;};
// Trusted restore seam. Caller exclusively owns fresh run objects; never invoke
// during evolve/evaluation or concurrently with public access. No work/RNG consumed.
struct RunCheckpointAccess {
    static RandomCheckpoint random(const RandomContext& context){std::ostringstream out;out.imbue(std::locale::classic());out<<context.engine_;return {context.seed_,context.evaluationAttempts_,out.str(),context.execution().requested()};}
    static void restoreRandom(RandomContext& context,const RandomCheckpoint& state){
        if(context.seed()!=state.seed || context.execution().requested()!=state.threads || context.evaluationAttempts_ || context.engine_!=std::mt19937(context.seed()) || state.engine.size()>16000)throw CheckpointError("random restore requires matching fresh context");
        std::istringstream words(state.engine);words.imbue(std::locale::classic());std::uint64_t word=0;bool nonzero=false;
        for(std::size_t i=0;i<std::mt19937::state_size;++i){if(!(words>>word)||word>std::numeric_limits<std::uint32_t>::max())throw CheckpointError("invalid MT19937 state word");nonzero=nonzero||word!=0;}
        if(!(words>>word)||word>std::mt19937::state_size||!nonzero)throw CheckpointError("invalid MT19937 state index");
        words>>std::ws;if(!words.eof())throw CheckpointError("trailing MT19937 words");
        std::istringstream in(state.engine);in.imbue(std::locale::classic());std::mt19937 engine;
        if(!(in>>engine))throw CheckpointError("invalid MT19937 state");
        in>>std::ws;if(!in.eof())throw CheckpointError("trailing MT19937 state");
        context.engine_=engine;context.evaluationAttempts_=state.candidateAttempts;
    }
    template<class T,class O> static CommittedPopulationCheckpoint<T> population(const Population<T,O>& p){p.requireReady();return {p.committed_->members,p.committed_->best,p.committed_->generation,p.committed_->bestGeneration,p.direction_,p.outcome_};}
    template<class T,class O=FitnessOrdering<T>> static std::unique_ptr<Population<T,O>> restorePopulation(const CommittedPopulationCheckpoint<T>& state,RandomContext& random){
        using P=Population<T,O>;auto p=std::unique_ptr<P>(new P(random,state.direction,nullptr));
        if(state.members.empty() || state.bestGeneration>state.generation || (state.lastOutcome!=GenerationOutcome::Committed && state.lastOutcome!=GenerationOutcome::NoViableCandidates && state.lastOutcome!=GenerationOutcome::OperationalFailure))throw CheckpointError("invalid committed generation/population");
        P::requireEvaluated(state.members);if(state.best.fitnessState()!=FitnessState::Valid)throw CheckpointError("invalid best-seen candidate");
        auto members=state.members;const auto identity=members.front().problemIdentity();
        for(const auto& c:members)if(c.problemIdentity()!=identity)throw CheckpointError("mixed checkpoint candidate contexts");
        if(state.best.problemIdentity()!=identity || std::none_of(members.begin(),members.end(),P::valid))throw CheckpointError("invalid checkpoint archive/context");
        for(std::size_t i=1;i<members.size();++i)if((P::valid(members[i-1])&&!P::valid(members[i])) || (P::valid(members[i-1])&&P::valid(members[i])&&p->better(members[i-1],members[i])))throw CheckpointError("checkpoint population order differs");
        if(p->better(members.back(),state.best))throw CheckpointError("archive worse than committed best");
        p->rank(members); // recreate exact rank ranges with the qualified arithmetic
        p->committed_=std::make_unique<typename P::State>(typename P::State{std::move(members),state.best,state.generation,state.bestGeneration});
        p->problem_=identity;p->problemBound_=true;p->outcome_=state.lastOutcome;return p;
    }
    template<class P,class C,class V>static AlgorithmCheckpoint algorithm(const GeneticAlgorithm<P,C,V>& a){return {a.attempts_,a._evolutions,a.outcome_};}
    template<class P,class C,class V>static void restoreAlgorithm(GeneticAlgorithm<P,C,V>& a,AlgorithmCheckpoint state){if(a.attempts_||a._evolutions||state.attempts<state.published || (state.lastOutcome!=GenerationOutcome::Committed && state.lastOutcome!=GenerationOutcome::NoViableCandidates && state.lastOutcome!=GenerationOutcome::OperationalFailure))throw CheckpointError("invalid algorithm restore counters");a.attempts_=state.attempts;a._evolutions=state.published;a.outcome_=state.lastOutcome;}
};

template<class T>void writeCommittedPopulation(CheckpointWriter& w,const CommittedPopulationCheckpoint<T>& state){
    w.integer(state.direction==ObjectiveDirection::Minimize?1:0);w.integer(state.generation);w.integer(state.bestGeneration);w.integer(static_cast<unsigned>(state.lastOutcome));w.integer(state.members.size());
    for(const auto& c:state.members)CandidateCheckpointCodec<T>::encode(w,c);
    CandidateCheckpointCodec<T>::encode(w,state.best);
}
template<class T,class Context>CommittedPopulationCheckpoint<T> readCommittedPopulation(CheckpointReader& r,const Context& context){
    const auto direction=r.count(1)==1?ObjectiveDirection::Minimize:ObjectiveDirection::Maximize;
    const auto generation=r.integer(),bestGeneration=r.integer();const auto outcome=static_cast<GenerationOutcome>(r.count(2));const auto count=r.count(r.limits().maxCandidates);
    if(!count)throw CheckpointError("empty committed population");
    std::vector<T> members;members.reserve(count);
    for(std::size_t i=0;i<count;++i)members.push_back(CandidateCheckpointCodec<T>::decode(r,context));
    auto best=CandidateCheckpointCodec<T>::decode(r,context);return {std::move(members),std::move(best),generation,bestGeneration,direction,outcome};
}
