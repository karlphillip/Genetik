# Genetik 0.4.0 — development pre-release

Proposed; not published. Genetik remains a general-purpose header-only C++20 genetic
algorithm framework with public OOP extension points. Mathematical discovery is
an optional specialization of the shared engine, not the identity of the core.

This candidate adds a separately owned Int64/Bool expression language: checked
signed64 arithmetic, positive-divisor Euclidean modulo, integer comparisons and
genuinely lazy conditionals. There is no implicit conversion through floating-point
operands. Owned parsing/canonical export, Unicode/LaTeX presentation, type-correct
initialization/mutation/crossover and exact integer-observation regression are included.

Bounded inclusive sums/products use explicit lexical iterator identities, checked
accumulation, empty identities0/1 after successful bounds, nested scope validation
and bounded work/storage/cancellation. Free-reference crossover is conservatively
restricted; no unbounded programs or protected arithmetic are introduced.

The discrete prime research application separates primality classification, prime
counting/reference identities and direct n-th-prime regression. Independent1200-prime
references and every original D3 outcome are retained. Best training classification
was56/62, but only one of five models beat composite-only classification on held-out
data and none did on the larger range. Direct discovery reached at most20/1000;
none of the five selected models matched any of200later primes. No selected model
contained iterator-reference nodes. The naive B=7919 nested identity requires
82,830,198,001iterations per query under the documented expansion, not a practical
large-prime result. Representability, evaluation correctness, discovery and cost
are distinct. Finite observations never prove a universal formula.

Generic execution, publication, run-owned randomness, strict continuous mathematics
and existing defaults are preserved. Checkpoints integrate discrete candidates,
scopes, full committed state and exact problem/configuration identity. Explicit
forward loaders support qualified0.3.0 scalar/unchanged discrete profiles while
keeping generic version checks strict. All dependent translation units must be
rebuilt; no stable binary ABI is promised.

The independent post0.3 fitness, optimizer and mutation experiments are excluded
from this clean release branch. They remain on their original local research
branches with negative results intact. No production default is selected from them.
See [integration audit](../../DISCRETE_RELEASE_INTEGRATION.md) and
[migration](MIGRATION.md).

Arithmetic is finite Int64, modulo requires b>0, text parsing has a nesting bound,
iteration and aggregate work are finite, and mathematical invalidity is distinct
from computational exhaustion. Cancellation is cooperative, checkpoint durability
is qualified by actual POSIX fsync/rename behavior, and crash recovery may repeat
work since the most recent durable committed state. No primality/n-th-prime oracle,
integer division, GCD, nested unbounded execution or advanced mathematics was added.

Exact measured local/hosted scope and retained development failures are in
[DISCRETE_HOSTED_QUALIFICATION.md](../../DISCRETE_HOSTED_QUALIFICATION.md).
The initial clean candidate passed71portable tests on Linux GCC13 x64/exactCMake3.16,
Clang18 x64 ASan+UBSan and GCC13 ARM64, plus15TSanselections repeated3and5complete.
The final report tip must receive the same matrix before delivery. GNU standard
library results do not qualify libc++, Windows/MSVC or macOS/AppleClang.
This remains a proposed development pre-release; publication requires separate approval.
