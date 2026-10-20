#include <TypedExpression.hpp>
#include <TypedConstantFitting.hpp>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <future>
#include <iostream>
#include <locale>
#include <mutex>
#include <new>
#include <thread>

// Test-only allocation faults and a timed gate at the first allocation INSIDE each
// evaluator call. This proves overlapping calls, not just two sequential threads.
namespace allocation_test {
struct Gate {
    std::mutex mutex; std::condition_variable condition; unsigned arrived = 0;
    void enter() {
        std::unique_lock lock(mutex); ++arrived; condition.notify_all();
        if (!condition.wait_for(lock, std::chrono::seconds(5), [&]{return arrived == 2;}))
            throw std::runtime_error("typed evaluation overlap timed out");
    }
};
thread_local int failAfter = -1;
thread_local bool counting = false;
thread_local std::size_t calls = 0;
thread_local Gate* gate = nullptr;
}
void* operator new(std::size_t n) {
    if (allocation_test::counting) ++allocation_test::calls;
    if (allocation_test::failAfter >= 0 && allocation_test::failAfter-- == 0) {
        allocation_test::failAfter = -1; throw std::bad_alloc();
    }
    if (auto* gate = allocation_test::gate) { allocation_test::gate = nullptr; gate->enter(); }
    if (auto* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void operator delete(void* p) noexcept {std::free(p);}
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void operator delete(void* p, std::size_t) noexcept {std::free(p);}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete[](void* p, std::size_t) noexcept {::operator delete(p);}
void* operator new(std::size_t n, const std::nothrow_t&) noexcept {try{return ::operator new(n);}catch(...){return nullptr;}}
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept {return ::operator new(n, std::nothrow);}
void operator delete(void* p, const std::nothrow_t&) noexcept {::operator delete(p);}
void operator delete[](void* p, const std::nothrow_t&) noexcept {::operator delete(p);}

using K = TypedExpressionKind;
using N = TypedExpressionNode;
using S = TypedShape;
using V = TypedValue;
using E = TypedExpression;
static void check(bool ok, const char* why) {if (!ok) throw std::runtime_error(why);}
template<class Exception, class F> static void throws(F f, const char* why) {
    bool caught = false; try {f();} catch (const Exception&) {caught = true;}
    check(caught, why);
}
static auto c(double x) {return E::constant(V::scalar(x));}
static auto vec(std::initializer_list<double> v) {return V::vector(std::span(v.begin(), v.size()));}
static void coefficients(const V& v, std::initializer_list<double> expected) {
    check(v.coefficients().size() == expected.size(), "coefficient count");
    check(std::equal(v.coefficients().begin(), v.coefficients().end(), expected.begin()), "independent expected coefficients");
}
static void values_and_shapes() {
    check(S::scalar() != S::vector(1) && S::vector(1) != S::matrix(1,1), "distinct kind identity");
    const auto scalar = V::scalar(-3); check(scalar.scalarValue() == -3, "scalar value");
    throws<std::logic_error>([&]{(void)vec({1}).scalarValue();}, "length one is not scalar");
    throws<std::invalid_argument>([]{(void)S::vector(0);}, "empty vector");
    throws<std::invalid_argument>([]{(void)S::matrix(1,0);}, "empty matrix dimension");
    throws<std::invalid_argument>([]{(void)V::matrix(2,2,std::array{1.,2.,3.});}, "wrong coefficient count");
    throws<std::invalid_argument>([]{(void)V::scalar(std::numeric_limits<double>::infinity());}, "infinite scalar input");
    throws<std::invalid_argument>([]{(void)vec({1.,std::numeric_limits<double>::quiet_NaN()});}, "nonfinite vector input");
    throws<std::invalid_argument>([]{(void)V::matrix(1,1,std::array{std::numeric_limits<double>::infinity()});}, "nonfinite matrix input");
    throws<std::out_of_range>([]{(void)vec({1}).at(1);}, "vector index");
    throws<std::out_of_range>([]{(void)V::matrix(1,1,std::array{1.}).at(0,1);}, "matrix index");
    std::vector<double> input{1.,2.}; double& alias=input[0];
    auto owned=V::vector(std::move(input)); alias=99;
    check(owned.at(0)==1 && input[0]==99, "rvalue input aliases not adopted");
    auto copy=owned; owned=vec({7,8}); check(copy.at(0)==1, "independent value copies");
    auto moved=std::move(copy);
    throws<std::logic_error>([&]{(void)copy.shape();}, "moved value inspection");
    copy=V::scalar(2); check(moved.at(0)==1 && copy.scalarValue()==2, "moved value reassignment");
}
static E affine() {
    const std::array schema{S::vector(3),S::vector(2)};
    const auto A=E::constant(V::matrix(2,3,std::array{1.,2.,3.,-1.,0.,4.}),schema);
    return E::binary(K::Add,E::binary(K::MatVec,A,E::variable(0,schema)),E::variable(1,schema));
}
static void arithmetic() {
    const auto f=affine(); const std::array bindings{vec({2,-1,3}),vec({5,-2})};
    const auto result=f.evaluate(bindings);check(result.valid(), "affine success");
    coefficients(result.value(),{14,8});
    check(f.shape()==S::vector(2) && f.depth()==3 && f.nodeCount()==5, "shape/depth/nodes");
    check(f.nodeShape(0)==S::matrix(2,3) && f.nodeShape(2)==S::vector(2), "inferred node shapes");
    check(f.arithmeticCost()==12 && result.arithmeticOperations()==12 && f.temporaryCoefficients()==11, "independent matvec work/payload");
    check(f.constantCount()==1 && f.constantCoefficients()==6 && f.bindingCoefficients()==5, "plan counts");
    const auto A=E::constant(V::matrix(2,3,std::array{1.,2.,3.,-1.,0.,4.}));
    const auto B=E::constant(V::matrix(3,2,std::array{2.,0.,1.,1.,-1.,2.}));
    auto product=E::binary(K::MatMul,A,B);
    coefficients(product.evaluate({}).value(),{1,8,-6,8});
    check(product.arithmeticCost()==20 && product.shape()==S::matrix(2,2), "rectangular multiplication work");
    const auto trans=E::unary(K::Transpose,A);
    coefficients(trans.evaluate({},{.maxArithmeticOperations=0}).value(),{1,-1,2,0,3,4});
    check(trans.shape()==S::matrix(3,2) && trans.arithmeticCost()==0 && trans.temporaryCoefficients()==12, "transpose shape/work/payload");
    coefficients(E::binary(K::Add,A,A).evaluate({}).value(),{2,4,6,-2,0,8});
    coefficients(E::binary(K::Subtract,A,A).evaluate({}).value(),{0,0,0,0,0,0});
    coefficients(E::binary(K::Multiply,c(-2),A).evaluate({}).value(),{-2,-4,-6,2,0,-8});
    coefficients(E::binary(K::Multiply,A,c(-2)).evaluate({}).value(),{-2,-4,-6,2,0,-8});
    const auto p=E::constant(vec({2,-1,3})),q=E::constant(vec({1,2,3}));
    coefficients(E::binary(K::Subtract,p,q).evaluate({}).value(),{1,-3,0});
    coefficients(E::binary(K::Add,p,q).evaluate({}).value(),{3,1,6});
    coefficients(E::binary(K::Multiply,c(2),p).evaluate({}).value(),{4,-2,6});
    coefficients(E::binary(K::Multiply,p,c(2)).evaluate({}).value(),{4,-2,6});
    auto dot=E::binary(K::Dot,p,q);
    check(dot.evaluate({}).value().scalarValue()==9 && dot.arithmeticCost()==5, "dot result and product-first counting");
    check(E::binary(K::Subtract,c(3),E::binary(K::Subtract,c(2),c(1))).evaluate({}).value().scalarValue()==2, "subtraction grouping");
    check(E::binary(K::Subtract,E::binary(K::Subtract,c(3),c(2)),c(1)).evaluate({}).value().scalarValue()==0, "other subtraction grouping");
    check(E::binary(K::Divide,c(3),c(2)).evaluate({}).value().scalarValue()==1.5, "division operand order");
    check(std::abs(E::unary(K::Log,c(2)).evaluate({}).value().scalarValue()-0.6931471805599453)<1e-15, "natural log");
    // Known contraction counterexample: separate product rounds to one, subtraction to zero.
    const double a=1+std::ldexp(1.,-27),b=1-std::ldexp(1.,-27);
    check(E::binary(K::Subtract,E::binary(K::Multiply,c(a),c(b)),c(1)).evaluate({}).value().scalarValue()==0,"no FMA contraction");
    const auto kept=f.evaluate(bindings);
    for(int i=0;i<8;++i)(void)product.evaluate({});
    coefficients(kept.value(),{14,8}); // scratch and intervening results have been destroyed
    const auto standalone=[] {return E::constant(vec({3,4})).evaluate({});}();
    coefficients(standalone.value(),{3,4}); // originating expression also destroyed
}
static void api_errors() {
    const std::array schema{S::scalar(),S::vector(1),S::matrix(1,1)};
    const auto scalar=E::variable(0,schema),vector=E::variable(1,schema),matrix=E::variable(2,schema);
    const std::array bound{V::scalar(3),vec({4}),V::matrix(1,1,std::array{5.})};
    check(scalar.evaluate(bound).value().scalarValue()==3 && vector.evaluate(bound).value().at(0)==4 && matrix.evaluate(bound).value().at(0,0)==5,"explicit ordered schema binding");
    throws<std::invalid_argument>([&]{(void)scalar.evaluate(std::array{V::scalar(3)});},"wrong binding arity");
    auto wrong=bound; wrong[1]=V::scalar(4);
    throws<std::invalid_argument>([&]{(void)scalar.evaluate(wrong);},"unused binding validated");
    throws<std::invalid_argument>([&]{(void)scalar.evaluate(wrong,{0,0});},"binding errors precede resource admission");
    throws<std::invalid_argument>([&]{(void)E::variable(3,schema);},"invalid variable index");
    for(auto k:{K::Add,K::Subtract,K::Divide,K::Dot,K::MatVec,K::MatMul})
        throws<std::invalid_argument>([&]{(void)E::binary(k,scalar,vector);},"unsupported combinations");
    throws<std::invalid_argument>([&]{(void)E::binary(K::Multiply,vector,vector);},"no ambiguous vector multiplication");
    throws<std::invalid_argument>([&]{(void)E::unary(K::Log,vector);},"no componentwise log");
    throws<std::invalid_argument>([&]{(void)E::unary(K::Transpose,vector);},"no vector transpose conversion");
    throws<std::invalid_argument>([&]{(void)E::binary(K::Add,vector,matrix);},"length one not 1x1");
    throws<std::invalid_argument>([]{(void)E::binary(K::Dot,E::constant(vec({1,2})),E::constant(vec({1})));},"dot shape mismatch");
    throws<std::invalid_argument>([]{(void)E::binary(K::MatVec,E::constant(V::matrix(2,2,std::array{1.,2.,3.,4.})),E::constant(vec({1})));},"matvec shape mismatch");
    throws<std::invalid_argument>([]{(void)E::binary(K::MatMul,
        E::constant(V::matrix(1,2,std::array{1.,2.})),
        E::constant(V::matrix(3,1,std::array{1.,2.,3.})));},"matrix inner-dimension mismatch");
    throws<std::invalid_argument>([&]{(void)E::binary(K::Add,scalar,c(1));},"schema mismatch");
    throws<std::invalid_argument>([]{(void)N::binary(K::Transpose);},"wrong operator factory arity");
    throws<std::invalid_argument>([]{(void)N::unary(static_cast<K>(999));},"unknown enum rejected");
    throws<std::invalid_argument>([]{(void)E(std::array{N::binary(K::Add)});},"missing operands");
    throws<std::invalid_argument>([]{(void)E(std::array{N::constant(V::scalar(1)),N::constant(V::scalar(2))});},"fragments");
    throws<std::invalid_argument>([]{(void)E(std::span<const N>{});},"empty expression");
}
static void limits() {
    constexpr auto maximum=std::numeric_limits<std::size_t>::max();
    throws<TypedLimitError>([&]{(void)S::matrix(maximum,2);},"dimension multiplication overflow");
    TypedValueLimits vl; vl.maxVectorLength=2;vl.maxMatrixRows=2;vl.maxMatrixColumns=3;vl.maxCoefficients=6;
    (void)V::matrix(2,3,std::array{1.,2.,3.,4.,5.,6.},vl);(void)V::vector(std::array{1.,2.},vl);
    throws<TypedLimitError>([&]{(void)V::vector(std::array{1.,2.,3.},vl);},"value vector boundary");
    throws<TypedLimitError>([&]{(void)V::matrix(3,2,std::array{1.,2.,3.,4.,5.,6.},vl);},"row boundary");
    throws<TypedLimitError>([&]{(void)V::matrix(1,4,std::array{1.,2.,3.,4.},vl);},"column boundary");
    vl.maxCoefficients=5;
    throws<TypedLimitError>([&]{(void)V::matrix(2,3,std::array{1.,2.,3.,4.,5.,6.},vl);},"coefficient storage boundary");
    vl.maxCoefficients=0;throws<std::invalid_argument>([&]{(void)V::vector(std::array{1.},vl);},"bad value limits");
    auto f=affine();const std::array b{vec({2,-1,3}),vec({5,-2})};
    auto exact=f.evaluate(b,{.maxArithmeticOperations=12,.maxTemporaryCoefficients=11});check(exact.valid(),"inclusive evaluation limits");
    for(auto limit:{TypedEvaluationLimits{11,11},TypedEvaluationLimits{12,10},TypedEvaluationLimits{0,0}}) {
        auto r=f.evaluate(b,limit);check(r.status()==TypedEvaluationStatus::ResourceLimit && r.arithmeticOperations()==0 && !r.failedNode(),"resource preflight separate from mathematics");
        throws<std::logic_error>([&]{(void)r.value();},"no partial resource result");
    }
    auto el=f.limits();el.maxArithmeticOperations=11;
    throws<TypedLimitError>([&]{(void)E(f.nodes(),f.schema(),el);},"static work cap");
    el=f.limits();el.maxTemporaryCoefficients=10;
    throws<TypedLimitError>([&]{(void)E(f.nodes(),f.schema(),el);},"static live output plus operands cap");
    el=f.limits();el.maxNodes=4;
    throws<TypedLimitError>([&]{(void)E(f.nodes(),f.schema(),el);},"node boundary beyond");
    el.maxNodes=5;el.maxDepth=3;(void)E(f.nodes(),f.schema(),el);el.maxDepth=2;
    throws<TypedLimitError>([&]{(void)E(f.nodes(),f.schema(),el);},"depth boundary beyond");
    el={};el.maxConstantNodes=2;el.maxConstantCoefficients=4;
    const auto p=E::constant(vec({1,2}));const auto pair=E::binary(K::Add,p,p,el);
    check(pair.constantCount()==2 && pair.constantCoefficients()==4,"occurrences counted independently");
    el.maxConstantCoefficients=3;
    throws<TypedLimitError>([&]{(void)E::binary(K::Add,p,p,el);},"aggregate constants cap before copying");
    el.maxConstantCoefficients=4;el.maxConstantNodes=1;
    throws<TypedLimitError>([&]{(void)E::binary(K::Add,p,p,el);},"constant node cap");
    el={};el.maxAbsConstant=0;(void)E::constant(V::scalar(-0.),{},el);
    throws<std::invalid_argument>([&]{(void)E::constant(V::scalar(1),{},el);},"absolute constant bound");
    el.maxAbsConstant=std::numeric_limits<double>::infinity();
    throws<std::invalid_argument>([&]{(void)E::constant(V::scalar(1),{},el);},"invalid bound");
    el={};el.maxNodes=0;throws<std::invalid_argument>([&]{(void)E::variable(0,std::array{S::scalar()},el);},"invalid structural limit");
    el={};el.maxVariables=1;
    throws<TypedLimitError>([&]{(void)E::variable(0,std::array{S::scalar(),S::scalar()},el);},"variable limit");
    el={};el.maxBindingCoefficients=7;
    throws<TypedLimitError>([&]{(void)E::variable(0,std::array{S::vector(2),S::matrix(2,3)},el);},"all bindings count including unused");
    el.maxBindingCoefficients=8;(void)E::variable(0,std::array{S::vector(2),S::matrix(2,3)},el);
    el={};el.values.maxMatrixRows=2;
    const auto A=E::constant(V::matrix(2,3,std::array{1.,2.,3.,4.,5.,6.}),{},el);
    throws<TypedLimitError>([&]{(void)E::unary(K::Transpose,A,el);},"inferred output shape also bounded");
    // Huge metadata only: reject arithmetic/storage overflow before any coefficient allocation.
    el={};el.values={maximum,maximum,maximum,maximum};el.maxBindingCoefficients=maximum;
    el.maxTemporaryCoefficients=maximum;el.maxArithmeticOperations=maximum;
    const std::array hugeSchema{S::matrix(10000000,10000000),S::matrix(10000000,10000000)};
    const auto a=E::variable(0,hugeSchema,el),d=E::variable(1,hugeSchema,el);
    throws<TypedLimitError>([&]{(void)E::binary(K::MatMul,a,d,el);},"matrix arithmetic work overflow");
    throws<TypedLimitError>([&]{(void)E::variable(0,std::array{S::vector(std::vector<double>().max_size()+1)},el);},"container storage capacity");
    const std::vector<S> many(34,S::vector(maximum/32));
    throws<TypedLimitError>([&]{(void)E::variable(0,many,el);},"aggregate binding count overflow");
    el={};el.maxNodes=10001;el.maxDepth=10001;el.maxArithmeticOperations=0;
    std::vector<N> chain{N::variable(0)};
    for(int i=0;i<10000;++i)chain.push_back(N::unary(K::Transpose));
    const E deep(chain,std::array{S::matrix(1,1)},el);
    check(deep.depth()==10001 && deep.evaluate(std::array{V::matrix(1,1,std::array{3.})}).value().at(0,0)==3 && deep.toString().size()>10000,"iterative deep construction/evaluation/export/destruction");
}
static void numerical() {
    const double tiny=std::numeric_limits<double>::denorm_min(),big=std::numeric_limits<double>::max();
    for(double zero:{0.,-0.}) {
        auto bad=E::binary(K::Divide,c(1),c(zero)).evaluate({});
        check(bad.status()==TypedEvaluationStatus::MathematicalInvalidity && bad.arithmeticOperations()==0 && bad.failedNode()==2,"signed zero divisor");
        throws<std::logic_error>([&]{(void)bad.value();},"no invalid numeric value");
        check(!E::unary(K::Log,c(zero)).evaluate({}).valid(),"signed zero log argument");
    }
    auto q=E::binary(K::Divide,c(-0.),c(2)).evaluate({});check(q.valid() && std::signbit(q.value().scalarValue()),"negative zero numerator");
    check(E::binary(K::Divide,c(tiny),c(tiny)).evaluate({}).value().scalarValue()==1,"subnormal denominator");
    check(E::binary(K::Multiply,c(tiny),c(1)).evaluate({}).value().scalarValue()==tiny,"subnormal result");
    check(E::unary(K::Log,c(tiny)).evaluate({}).valid(),"positive subnormal log");
    check(E::unary(K::Log,c(1)).evaluate({}).value().scalarValue()==0,"log one");
    check(!E::unary(K::Log,c(-1)).evaluate({}).valid(),"negative log domain");
    const auto under=E::binary(K::Divide,c(-tiny),c(2));
    check(std::signbit(under.evaluate({}).value().scalarValue()),"signed finite underflow");
    check(!E::unary(K::Log,under).evaluate({}).valid(),"underflow followed by log domain");
    check(!E::binary(K::Divide,c(1),under).evaluate({}).valid(),"underflow followed by divide domain");
    check(!E::binary(K::Divide,c(big),c(tiny)).evaluate({}).valid(),"quotient overflow");
    check(!E::binary(K::Multiply,c(0),E::unary(K::Log,c(-1))).evaluate({}).valid(),"invalid beneath zero mask");
    const auto one=E::constant(vec({1})),negativeZero=E::constant(vec({-0.}));
    check(std::signbit(E::binary(K::Dot,negativeZero,one).evaluate({}).value().scalarValue()),"first product initializes accumulator, not +0");
    const auto signedMatVec=E::binary(K::MatVec,
        E::constant(V::matrix(1,1,std::array{-0.})),one).evaluate({});
    check(signedMatVec.valid() && std::signbit(signedMatVec.value().at(0)),"matrix reduction preserves first-product zero sign");
    const auto overflow=E::binary(K::Dot,E::constant(vec({big,big,-big})),E::constant(vec({1,1,1})));
    const auto invalid=overflow.evaluate({});check(!invalid.valid() && invalid.arithmeticOperations()==3 && invalid.failedCoefficient()==0,"overflow before later cancellation");
    check(!E::binary(K::Multiply,c(0),overflow).evaluate({}).valid(),"dot overflow cannot be masked");
    const auto largeA=E::constant(V::matrix(2,2,std::array{big,big,0.,0.}));
    const auto ones=E::constant(V::matrix(2,2,std::array{1.,1.,1.,1.}));
    const auto badMatrix=E::binary(K::MatMul,largeA,ones).evaluate({});
    check(!badMatrix.valid() && badMatrix.failedCoefficient()==0 && badMatrix.arithmeticOperations()==3,"matrix row-major checked reduction");
    const auto lateA=E::constant(V::matrix(2,1,std::array{1.,big}));
    const auto late=E::binary(K::MatVec,lateA,E::constant(vec({2}))).evaluate({});
    check(!late.valid() && late.failedCoefficient()==1 && late.arithmeticOperations()==2,"invalid later output does not expose partial vector");
}
static void ownership_and_export() {
    auto schema=std::vector<S>{S::scalar()}; auto tokens=std::vector<N>{N::variable(0),N::constant(V::scalar(.1)),N::binary(K::Subtract)};
    const E expression(tokens,schema); tokens[1]=N::constant(V::scalar(999));schema[0]=S::vector(1);
    auto schemaAlias=std::vector<S>{S::scalar()};
    auto rvalueTokens=std::vector<N>{N::variable(0)};
    auto& retainedToken=rvalueTokens[0];
    const E fromRvalue(std::move(rvalueTokens),std::move(schemaAlias));
    retainedToken=N::constant(V::scalar(99));schemaAlias[0]=S::vector(1);
    check(fromRvalue.toString()=="x0" && fromRvalue.schemaString()=="schema{x0:scalar}","rvalue schema/token storage not adopted");
    auto snapshot=expression;auto detached=snapshot.nodes();detached[0]=N::constant(V::scalar(7));
    check(expression.toString()=="(x0 - scalar(0.10000000000000001))" && snapshot.schemaString()=="schema{x0:scalar}","detached tokens/schema/copies");
    const auto before=snapshot.toString();
    throws<std::invalid_argument>([&]{snapshot=E::binary(K::Add,snapshot,E::constant(vec({1})));},"failed derivation preserves original");
    check(snapshot.toString()==before,"original expression unchanged after failed replacement");
    const auto matrix=V::matrix(1,2,std::array{-0.,.1});
    check(matrix.toString()=="matrix[1,2]((-0.0),0.10000000000000001)","faithful row-major signed zero constant spelling");
    const auto exported=E::unary(K::Transpose,E::constant(matrix));
    check(exported.toString()=="transpose(matrix[1,2]((-0.0),0.10000000000000001))","unary export grouping");
    check(affine().toString()=="(matvec(matrix[2,3](1.0,2.0,3.0,(-1.0),0.0,4.0),x0) + x1)","mixed operator export and variable identity");
    struct Comma : std::numpunct<char> {char do_decimal_point() const override{return ',';}};
    const auto original=std::locale();std::locale::global(std::locale(original,new Comma));
    const auto text=exported.toString();std::locale::global(original);
    check(text=="transpose(matrix[1,2]((-0.0),0.10000000000000001))","locale-independent constants");
    auto moved=std::move(snapshot);throws<std::logic_error>([&]{(void)snapshot.depth();},"moved expression access");
    snapshot=expression;check(moved.toString()==expression.toString() && snapshot.toString()==before,"expression move/reassignment");
}
class ReturnAllocationFault final:public TypedTrainingObjective {
public:
    using TypedTrainingObjective::TypedTrainingObjective;
protected:
    TypedRegressionMetrics compute(const TypedExpression& e) override {
        auto result=TypedTrainingObjective::compute(e);
        if(armed){armed=false;allocation_test::failAfter=0;}
        return result;
    }
private:bool armed=true;
};
static void operational() {
    const auto f=affine(); const std::array b{vec({2,-1,3}),vec({5,-2})};
    const auto retained=f.evaluate(b);const auto text=f.toString();
    allocation_test::calls=0;allocation_test::counting=true;
    (void)f.evaluate(b);
    allocation_test::counting=false;
    const auto boundaries=allocation_test::calls;
    check(boundaries>0 && boundaries<=64,"bounded observed evaluation allocation boundaries");
    bool observedExecutedPrefix=false;
    for(int fail=0;fail<static_cast<int>(boundaries);++fail) {
        allocation_test::failAfter=fail;
        std::uint64_t attempted=0;
        bool caught=false;try{(void)f.evaluateWithWork(b,attempted);}catch(const std::bad_alloc&){caught=true;}
        if(attempted>0)observedExecutedPrefix=true;
        allocation_test::failAfter=-1;
        check(caught,"allocation failure is operational at each evaluation allocation");
        coefficients(retained.value(),{14,8});coefficients(f.evaluate(b).value(),{14,8});
        check(f.toString()==text && b[0].at(0)==2,"failure preserves values and bindings");
    }
    check(observedExecutedPrefix,"primitive diagnostics survive later allocation failure");
    // Fail while copying the scored value into the preparation result. Accounting
    // was already merged: the exception must not record the same objective twice.
    auto problem=TypedRegressionProblem::create(f.schema(),f.shape(),{{{b[0],b[1]},retained.value()}},f.limits());
    auto objective=std::make_shared<ReturnAllocationFault>(problem);
    TypedConstantRefinement scoring(objective);
    throws<std::bad_alloc>([&]{(void)scoring.prepare(f,*problem,{});},"post-scoring result allocation failure");
    check(objective->work().lossAttempts==1 && scoring.statistics().origins[0].ordinary==1,"post-scoring failure accounts once");
    check(scoring.prepare(f,*problem,{}).loss.score()==0 && scoring.statistics().origins[0].ordinary==2,"successful preparation after failed result construction");
    auto value=vec({1,2});allocation_test::failAfter=0;
    throws<std::bad_alloc>([&]{value=b[0];},"value copy allocation failure");
    check(value.at(0)==1,"strong value copy assignment");
    auto target=f;allocation_test::failAfter=0;
    throws<std::bad_alloc>([&]{target=f;},"expression copy allocation failure");
    check(target.toString()==text,"strong expression copy assignment");
    auto outcome=E::unary(K::Log,c(-1)).evaluate({});
    allocation_test::failAfter=0;
    throws<std::bad_alloc>([&]{outcome=retained;},"result copy allocation failure");
    check(outcome.status()==TypedEvaluationStatus::MathematicalInvalidity,"strong result copy preserves classification and pairing");
    throws<std::logic_error>([&]{(void)outcome.value();},"failed result copy cannot expose absent value");
    auto token=N::variable(0);const auto constant=N::constant(vec({1,2}));
    allocation_test::failAfter=0;
    throws<std::bad_alloc>([&]{token=constant;},"token copy allocation failure");
    check(token.kind()==K::Variable && token.variableIndex()==0,"strong token copy preserves payload identity");
    auto moved=std::move(outcome);
    throws<std::logic_error>([&]{(void)outcome.valid();},"moved result inspection");
    outcome=retained;coefficients(outcome.value(),{14,8});
    check(!moved.valid(),"moved-to result preserves original invalid outcome");
}
static void overlap() {
    const auto f=affine();allocation_test::Gate gate;
    auto worker=[&](double offset) {
        const std::array input{vec({2+offset,-1,3}),vec({5,-2})};
        allocation_test::gate=&gate;
        auto retained=f.evaluate(input);
        for(int i=0;i<100;++i) {
            const auto r=f.evaluate(input);
            check(r.valid() && r.value().at(0)==14+offset && r.value().at(1)==8-offset,"concurrent immutable evaluation with private scratch");
            const auto invalid=E::unary(K::Log,c(-1)).evaluate({});check(!invalid.valid(),"invalid call between successful invocations");
        }
        coefficients(retained.value(),{14+offset,8-offset});
    };
    auto a=std::async(std::launch::async,worker,0.);
    auto b=std::async(std::launch::async,worker,2.);
    a.get();b.get();check(gate.arrived==2,"actual evaluator overlap");
}
int main(int argc,char** argv) {
    try {
        if(argc==2 && std::string_view(argv[1])=="--overlap")overlap();
        else {values_and_shapes();arithmetic();api_errors();limits();numerical();ownership_and_export();operational();overlap();}
        std::cout<<"typed value/expression contracts passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
