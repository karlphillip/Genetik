# Checked-evaluation workspace: measured invocation-local reuse

## Scope and reference

Starting tree: clean accepted `94f05814afaaf26cc48fa4d22438cf56bb8943bc`.
No search policy, arithmetic, validation rule, budget, cache or public signature
changes. Default cadence remains one. Historical prime, arithmetic and cadence
campaigns are preserved; this is a fixed-work optimization experiment, not a new
search-quality campaign. Production delivery is next.

The baseline source was archived into `../evaluation-reference` before editing
Expression.hpp. The same evidence harness is compiled against that archive and
against current headers, with GCC 13.3.0, `-O3 -DNDEBUG -std=c++20
-fno-fast-math -ffp-contract=off`, no LTO. Runtime: Jetson Orin NX/AArch64,
Linux 6.8.12-1021-tegra, libstdc++ 13, glibc/libm 2.39, default IEEE rounding,
gradual underflow/nontrapping arithmetic. CPU governor schedutil, no affinity or
clock locking; about 8.6 GiB RAM available, no swap, 833 GiB storage free.

## What the accepted path did

Every `RegressionProblem::measure` row called `Expression::evaluate`, which
constructed an empty vector, reserved `nodeCount()*sizeof(double)` operand
storage, traversed postfix nodes, then destroyed/deallocated the vector. Storage
was **not** reused between rows. Reserve did not initialize doubles or make indexed
capacity writable. Terminals used push_back; operations consumed initialized
values. There was no unnecessary whole-vector zero fill to remove.

Each call validated expression usability, exact binding count and every finite
binding before arithmetic. Validated immutable data did not bypass these checks.
Postfix traversal visited left/right/parent in the stored order; each operation
stored binary64, checked domain/finiteness, and invalid children stopped execution.
The loss charged a row before evaluation, checked residual, square, left-to-right
sum and final division, and stopped at the first invalid result. TrainingObjective
charged actual loss/category work before compute and nested exceptions with the
attempt number; Population/GA added preparation context before propagating.

All-row prediction reporting is separate: it continues after invalid predictions,
counts every requested row, and reports no partial-data maximum as a complete one.
Prime's individual decode calls remain independent convenience evaluations; this
change does not introduce persistent storage or alter their diagnostics.

## One change and its bounds

One internal `genetik_detail::EvaluationWorkspace` is owned lexically by each loss
invocation (and each all-row `predictionReport`). It owns only an operand vector;
no expression, input span or reference escapes. Public evaluate creates its own
local vector. Both paths use the **same private interpreter**. There is no public
workspace API, mutable expression scratch, global/thread-local state or result reuse.

Each valid binding call clears logical contents and reserves the validated node
count before interpretation. The first reserve allocates; later rows retain
capacity. push_back still writes each active terminal before use. No raw-capacity
indexing or resize/zero initialization is introduced. Growth precedes node access;
there are no live stack references across growth. Space remains O(N), at most the
expression's validated N nodes per fixed-expression invocation, and execution
remains O(rows*(bindings+nodes)), stopping early as before. No cross-loss reuse.

Empty workspace construction does not allocate. First growth remains **after**
the first attempted-row charge, inside the already charged loss. Allocation
failure preserves its nested bad_alloc cause and consumed loss/row work. The
private generation still fails without publishing population/generation/archive;
RNG/work already consumed stays consumed. Dirty logical contents after invalidity,
input errors or failed growth cannot contaminate a subsequent successful call.
Independent invocations share no writable storage, preserving reentrancy/read-only
concurrency. Fitting acceptance, exact expression/score pairing, local/global
bounds, row order and every floating-point instruction in the interpreter and
loss remain unchanged.

## Declared corpus and measurement method

`benchmarks/EvaluationCorpus.hpp` fixes all expressions, rows and timed request
counts before comparison:

- Seven-node polynomial `x0*x0+x0+1` and the accepted 16-node prime representative,
  each against the existing 1,000 normalized prime fitting rows (2,000 losses/batch).
- Six-node strict-arithmetic target `log(x0)+x1/x0` and a 17-node mixed arithmetic
  composition, each against its 25 fitting rows (20,000 losses/batch).
- Full balanced 31-node addition tree at the campaign node limit, against prime
  rows (1,000 losses/batch). This is a boundary fixture, not a search template.
- Log with invalidity in row 1 (100,000 losses/batch) or row 1,000 (2,000/batch).
- Six-row alternating valid/invalid log bindings including positive subnormal;
  both signed-zero divisors; signed underflow; subnormal ratio; masked overflow;
  underflow followed by log; residual overflow; sum overflow; and a three-input
  higher-index variable. Each uses 10,000 losses/batch. The exact rows/targets and
  faithful expressions are in the corpus/reference artifact.

Ordinary operator-new calls/requested bytes are counted only around one checked
loss in a separately instrumented executable. These are all ordinary C++ new
allocations in that region (the stack in these fixtures), **not** malloc/libm/
process-wide or allocator usable-size/metadata measurements. Counter/fault executables
are serial diagnostics, not thread-safe allocator tooling. The count executable
is never used for final timing. Allocation failures are tested separately. These counts measure allocation traffic,
not peak RSS; peak operand capacity remains O(N).

No row/node clocks or diagnostic storage in timed runs: 32 warm-up losses, five
paired batches/workload, alternating baseline/candidate then candidate/baseline by
pair parity. Batch size is fixed in the corpus. A noinline call plus compiler memory
barrier precedes each request; exact result bits and visited rows feed a printed
checksum. Both modes' checksums and row counts must match every pair. Elapsed time
covers checked loss only, excluding fixture construction/output. Warm-up warms code/
allocator state; every timed loss still creates an empty workspace and pays its
first allocation. Only rows inside that invocation reuse capacity. Short edge batches
are noisy and are reported without generalizing their relative percentages.

Logical stack reset occurs once per visited row in both implementations (new
vector before, clear now). Reserved slots receive no default-value initialization;
terminal pushes and operation stores are unchanged. There is one cold growth per
loss now, no row-to-row growth; no separately timed allocator/interpreter split is
claimed. This measures the contained storage change, not a claim that allocation
accounted for the prior inclusive 97.5% profile region.

## Results

See `evaluation-results/baseline-allocations.csv` and `candidate-allocations.csv`:
columns are case,nodes,visited_rows,new_calls,requested_bytes,fitness_state
(1 Valid, 2 Invalid). Examples per invocation:

| Workload | Rows | Before allocations/bytes | After allocations/bytes |
| --- | ---: | ---: | ---: |
| Polynomial | 1000 | 1000 / 56000 | 1 / 56 |
| Prime representative | 1000 | 1000 / 128000 | 1 / 128 |
| Arithmetic target | 25 | 25 / 1200 | 1 / 48 |
| Mixed | 25 | 25 / 3400 | 1 / 136 |
| 31-node boundary | 1000 | 1000 / 248000 | 1 / 248 |
| Early-invalid log | 1 | 1 / 16 | 1 / 16 |
| Late-invalid log | 1000 | 1000 / 16000 | 1 / 16 |

Five paired batches, microseconds per loss (median [minimum, maximum]):

| Workload | Accepted | Reuse | Median reduction |
| --- | ---: | ---: | ---: |
| Polynomial | 59.112 [57.967,59.286] | 44.478 [43.195,46.599] | 24.76% |
| Prime representative | 104.851 [103.959,120.004] | 98.120 [97.059,99.270] | 6.42% |
| Arithmetic target | 2.039 [1.781,2.058] | 1.624 [1.586,1.698] | 20.39% |
| Mixed | 3.248 [3.094,3.393] | 2.872 [2.635,2.962] | 11.59% |
| 31-node boundary | 137.728 [137.327,142.963] | 130.368 [125.577,136.230] | 5.34% |
| Early-invalid log | 0.053 [0.045,0.062] | 0.062 [0.049,0.063] | no consistent gain |
| Late-invalid log | 53.207 [52.608,55.920] | 38.745 [36.352,40.036] | 27.18% |

All 17 workloads and every raw batch, including noisy/slower one-row cases, are in
`timings.csv` and `timing-summary.csv`. Reuse has a useful measured benefit on the
multirow cases; keep the small production change. It does not eliminate interpreter,
binding validation, libm, per-node checks or loss-reduction work, which remain the
larger part of total evaluation. Further attribution is a separate profiling task.

## Independent exact reference and search replay

`accepted-reference.txt` was produced by the archived implementation, not by two
entry points into the edited kernel. It records every corpus row's classification
and finite uint64 binary64 representation (including signed zero), first invalid
node, exact loss/max error, samples, all existing TrainingWork fields and all-row
reporting. Candidate output compares byte-for-byte, also for wrong arity/nonfinite
bindings, malformed/moved expressions, mismatched loss schema and exhausted row
counter classifications. CTest continues checking this accepted artifact.

Focused workspace tests cover cold growth, larger-expression growth and injected
bad_alloc, repeated alternating invalid/valid calls without allocation, successful
calls after binding/allocation failure, signed-zero retention, independent owners,
one allocation for a multirow loss and unchanged early stop. An injected workspace
allocation failure after two completed fitting losses verifies 3 charged losses,
5 charged rows (2+2+1), original nested cause, committed genomes/fitness/generation/
best preservation, consumed RNG and a later successful loss. Existing tests retain
global budgets, publication, arithmetic, fitter and cadence coverage.

Search replay results and full raw initial/prepared populations, fitness bits,
work/variation/preparation/stop counters, trace and final mt19937 state are saved
in `accepted-prime.txt`, `accepted-arithmetic.txt`, `accepted-interval4.txt`.
Candidate outputs match each byte-for-byte. No candidate from an abandoned attempt
is promoted. Timing includes initialization and search only; data generation,
reporting, baseline models and files are excluded, diagnostics disabled.

| Replay | Loss attempts / visited rows | Published / attempted generations | Unpublished losses / rows | Search seconds accepted → reuse |
| --- | ---: | ---: | ---: | ---: |
| Prime seed 2, cadence 1 | 100000 / 99864635 | 11 / 12 | 5967 / 5949018 | 7.9163 → 6.4173 |
| Arithmetic seed 42, cadence 1 | 51624 / 1287392 | 8 / 8 | 0 / 0 | .17614 → .15982 |
| Short arithmetic seed 42, cadence 4 | 76 / 1828 | 5 / 5 | 0 / 0 | .001093 → .000526 |

Prime normalized MSE remains exactly 1.4636376958468287e-6; arithmetic remains
5.3110430571409987e-16 and stops on training threshold. Prime stops on the actual
loss cap. No new search evidence or extrapolation assessment is inferred. These are
single before/after timing observations, not repeated search speedup estimates;
fixed-work paired batches are the primary timing evidence.

## Reproduce

From the repository (choose fresh build-workspace evidence paths if desired):

```sh
mkdir -p ../evaluation-reference ../evaluation-evidence
# Independent accepted headers/helpers, without modifying this checkout:
git archive 94f05814afaaf26cc48fa4d22438cf56bb8943bc | tar -x -C ../evaluation-reference
CXXFLAGS='-std=c++20 -O3 -DNDEBUG -fno-fast-math -ffp-contract=off'
g++ $CXXFLAGS -I../evaluation-reference/genetik -I../evaluation-reference -I. benchmarks/evaluation_workspace.cpp -o ../evaluation-evidence/baseline
g++ $CXXFLAGS -Igenetik -I. benchmarks/evaluation_workspace.cpp -o ../evaluation-evidence/candidate
g++ $CXXFLAGS -DGENETIK_COUNT_ALLOCATIONS -I../evaluation-reference/genetik -I../evaluation-reference -I. benchmarks/evaluation_workspace.cpp -o ../evaluation-evidence/baseline-count
g++ $CXXFLAGS -DGENETIK_COUNT_ALLOCATIONS -Igenetik -I. benchmarks/evaluation_workspace.cpp -o ../evaluation-evidence/candidate-count
../evaluation-evidence/baseline --reference > ../evaluation-evidence/baseline-reference.txt
../evaluation-evidence/candidate --reference > ../evaluation-evidence/candidate-reference.txt
cmp ../evaluation-evidence/baseline-reference.txt ../evaluation-evidence/candidate-reference.txt
../evaluation-evidence/baseline-count --allocations
../evaluation-evidence/candidate-count --allocations
python3 scripts/measure_evaluation_workspace.py ../evaluation-evidence/baseline ../evaluation-evidence/candidate ../evaluation-evidence
# Run once with each binary, prime/arithmetic; add '4' for the bounded cadence check:
../evaluation-evidence/baseline --replay prime > ../evaluation-evidence/baseline-prime.txt
../evaluation-evidence/candidate --replay prime > ../evaluation-evidence/candidate-prime.txt
cmp ../evaluation-evidence/baseline-prime.txt ../evaluation-evidence/candidate-prime.txt
```

For arithmetic substitute `--replay arithmetic`; the short scheduled/unscheduled
check is `--replay arithmetic 4` (8 slots, five attempts, eight loss calls/fit,
4,096 global cap, zero thresholds). No detailed allocation counters are compiled
into the normal example or timing binaries. Existing downstream/public expression
and regression APIs require no migration. Checksums qualify the stored evidence;
reproducibility is scoped to the declared build/runtime/inputs and execution mode.

Sanitizer qualification initially exposed an error in the **test interceptor**:
libstdc++ stable_sort's nothrow buffer allocation reached ASan's new interceptor
but the test's custom delete used free. Paired nothrow new/delete overrides now
route through the same allocation family. No sanitizer check was disabled and
no library change was required for this repair. The focused fault test then passed;
the full suite was rerun after the repair. Arithmetic interpreter and loss-reduction
source bodies also compare identically with the archived source (provenance.txt).

## Qualification commands

```sh
../.tools/bin/cmake -S . -B ../build-release -DCMAKE_BUILD_TYPE=Release
../.tools/bin/cmake --build ../build-release -j2
../.tools/bin/ctest --test-dir ../build-release --output-on-failure
../.tools/bin/cmake -S . -B ../build-debug -DCMAKE_BUILD_TYPE=Debug
../.tools/bin/cmake --build ../build-debug -j2
../.tools/bin/ctest --test-dir ../build-debug --output-on-failure
../.tools/bin/cmake -S . -B ../build-sanitize -DCMAKE_BUILD_TYPE=Debug -DGENETIK_SANITIZERS=ON
../.tools/bin/cmake --build ../build-sanitize -j2
../.tools/bin/ctest --test-dir ../build-sanitize --output-on-failure
timeout 10s ../build-release/genetik_demo 42 1000
# Full warning check, including the evidence and fault-injection executables:
g++ -std=c++20 -fno-fast-math -ffp-contract=off -Wall -Wextra -Wpedantic -Igenetik -I. -fsyntax-only benchmarks/evaluation_workspace.cpp tests/evaluation_workspace.cpp scripts/cadence_campaign.cpp examples/prime_regression.cpp tests/prime_regression.cpp apps/regression_search.cpp tests/strict_arithmetic.cpp examples/multivariate_regression.cpp tests/multivariate.cpp tests/downstream/main.cpp tests/constant_fitting.cpp scripts/constant_fitting_campaign.cpp tests/symbolic_regression.cpp tests/expressions.cpp tests/generation.cpp tests/tests.cpp examples/demo.cpp examples/polynomial_regression.cpp examples/expression_demo.cpp
git -c core.whitespace=cr-at-eol diff --check
```

All 18 CTest cases pass in Release, Debug and ASan+UBSan, including the independent
reference, workspace faults, standalone-header builds, downstream consumer, negative
strict-FP compilation and bounded character/generic-file/prime workflows. ASan runs
focused fitting/search fixtures, not the full compatibility searches or benchmarks.
Character best-seen remains .892 after 1,000 generations/100,100 calls. Warnings and
whitespace checks are clean. Exact logs and checksums are in evaluation-results.


The final suite durations were Release 2.01 s, Debug 2.35 s and ASan+UBSan
159.26 s. The explicit short `../build-sanitize/genetik_evaluation_workspace
--replay arithmetic 4` also matches the accepted snapshot byte-for-byte. Final
unprofiled batch timings were taken after qualification, without concurrent builds
or tests. Earlier exploratory timings remain in the local build workspace; the
reported five-pair set uses the finalized harness. No setting or workload request
count was adjusted between implementations.


Evidence text uses canonical line endings without trailing separator blanks; this
presentation cleanup changes no reference number or classification. The accepted
archive regenerates that canonical output with the current harness. Focused exact
reference comparisons were rerun in all three configurations after this cleanup.
