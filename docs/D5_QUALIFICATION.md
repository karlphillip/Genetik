# D5 local qualification

This record covers additive local development from published0.4.0
`c08859e462210a1ef378b0c6c47a2b4c520f7353`. It does not extend the published
release's hosted environment claims. No remote writes or release actions occur.
The generic GA remains independent of the new mathematical representations.

## Tested source and manifests

The final implementation and full qualification source is
`5e44bcfaf83e908d0b79a5eb7b23031fffa00284`. A clean tracked-source archive
is retained and all complete configurations passed after the final ownership
repair. The final documentation/evidence commit changes no executable source,
test data or build configuration beyond this tested revision. Configured caches remain in place; changed dependencies rebuild and no
CMake cache is moved. Every manifest/result records its actual source revision.

Earlier complete Release/Debug/exact3.16 passes at
`cc398131e6480d3263574cedcf58f48e4f1954c8` precede the ownership repair and
are retained as earlier evidence. The initial full ASan+UBSan invocation there
was82/83: the dimensional contract timed out at600 seconds. The unchanged
executable passed separately in889.17 seconds. An instrumented-only1800-second
limit was then introduced. The subsequent full rerun at
`f687be3b95615d1a58ed8943c4fd637d4e5c62e1` was interrupted after the new
ownership regression demonstrated an additional defect; it is not a complete
passing suite. The final-source Release suite passed84/84 in436.10 seconds; Debug passed84/84
in469.72 seconds; exact CMake3.16.0 passed84/84 in317.91 seconds. The complete ASan+UBSan suite passed83/83 in4411.11 seconds, including
the repaired dimensional contract in894.89 seconds. Separate TSan passed all20 selections three times each (60 actual executions)
in90.77 seconds, then all11 complete contracts in94.66 seconds. No race diagnostic
was reported. Actual named manifests, execution counts, compile/link commands
and original-log hashes are in [selected evidence](d5-results/qualification/).

Native manifests contain84 tests; portable manifests contain83. The sole difference
is the explicitly opt-in captured Jetson numerical snapshot. The complete
names are verified, including all existing portable mathematical, concurrency,
checkpoint, installation, generic OOP and accepted replay contracts.

## Preserved development failures

- Source `c1fbb28d67d72cf32259100ba05e4ab931722fb6`: full Release82/84.
  One old parser-negative fixture classified `powi(1,-1)` as malformed; D5 now
  explicitly supports that expression. It was replaced by unsupported exponent-9,
  with positive scalar-negative-power and unchanged typed-parser rejection checks.
  The other failure was the installed-consumer harness's120-second internal
  command timeout. Its command/process-group diagnostics were retained. The cap
  is now300 seconds, still within the600-second outer fixture; no consumer work,
  instrumentation, arithmetic check or assertion was removed.
- Source `c5aa46ac099e70b0e4ae1b4fc7784e4a210b45bc`: full clean Release84/84,
  including the385.33-second installed/relocated fixture. The Debug build completed,
  but the following Debug suite was interrupted to repair newly identified
  resource/terminal coverage defects. This is not a complete Debug pass and the
  Release pass does not qualify the later repairs.
- The first24 mathematical benchmarks retained an energy-generator defect: a
  storage cap incorrectly truncated operator scanning before multiplication.
  The structural regression failed before repair. Later destination node-limit,
  same-unit input coverage and continuous terminal-source fixtures likewise
  failed before their fixes. All earlier protocols/outcomes remain retained.
  The final24 replay keeps the originally frozen data, seeds, grammar and budgets.
- Nested iterator dependency repair could erase a nested body's sole own reference;
  that regression and its repair are retained. Existing frame storage was restored
  by reusing a cursor slot for the third modular-power operand, preserving old
  resource boundaries. The original12 iterator outcomes/counters reproduced.
- A late constraint-check exhaustion originally lost already completed derivative
  work; the new boundary fixture failed before the accounting repair. Symbolic
  CSE/export now translate internal bounded-stop signals to public typed resource
  or cancellation exceptions. Nested-power presentation now preserves grouping.
- Initial warning-as-error compilation failures and their fixes remain in the
  ignored logs. No warning flags, numerical requirements or historical assertions
  were weakened. Boost availability failed because optional headers are absent;
  an arbitrary-precision backend is not implemented or qualified.

A final constructor audit reproduced a dimensional fitting-policy alias:
passing an rvalue vector transferred its allocation, allowing a retained caller
pointer to mutate the supposedly owned unit schema. The independent regression
failed before repair and passed afterward. Unit-schema limits/counts/bytes now
validate before their owned copy; selection always detaches caller storage and
uses the configured inference limits. Factories forward the problem's policy.
The final implementation commit also adds invalid-schema-storage and inference
policy fixtures. This repair changes no operator, mathematical rule, grammar,
RNG policy, fitting defaults or campaign configuration.

The timeout-only qualification patch initially omitted parentheses in CMake's
`endif()`. Configure rejected commit `c85b4dda3f0cc01ee577b351325cce4cf5f7b2b9`
before any tests ran. The repair is commit `f687be3b95615d1a58ed8943c4fd637d4e5c62e1`.
The failed configuration and exact source remain retained. The hosted allowance
is120 minutes because the first full sanitizer invocation alone took4111 seconds
with an interrupted dimensional fixture, before compilation/artifact capture.
This administrative allowance does not change mathematical work or search budgets.

## Required TSan protocol

Each of these20 selections passed three times in a separate instrumented build;
[actual execution counts](d5-results/qualification/tsan-executions.json) verify60
passes, rather than inferring repetitions from the20-test summary:

- `genetik_checkpoint_overlap`
- `genetik_concurrency_contracts`
- `genetik_d5_dimension_overlap`
- `genetik_d5_iteration_overlap`
- `genetik_d5_recurrence_overlap`
- `genetik_d5_scientific_overlap`
- `genetik_d5_symbolic_overlap`
- `genetik_derivative_overlap`
- `genetik_derivative_regression_overlap`
- `genetik_discrete_iteration_overlap`
- `genetik_discrete_overlap`
- `genetik_discrete_prime_overlap`
- `genetik_integral_overlap`
- `genetik_oop_parallel`
- `genetik_parsing_overlap`
- `genetik_prime_exact_overlap`
- `genetik_scientific_contracts`
- `genetik_template_overlap`
- `genetik_typed_overlap`
- `genetik_typed_preparation_overlap`

The additional complete contracts are the three discrete contracts, checkpoint
compatibility, integral contracts, and the six D5 iteration/integer/recurrence/
scientific/symbolic/dimension contracts:11 complete selections. Forced-overlap
fixtures share immutable expressions/models while inputs, scratch, preparation
and fitting remain worker-private. `TSAN_OPTIONS=halt_on_error=1` is required.

## Mathematical and continuation evidence

- 3652 independently generated Python integer fixtures, including full-range
signed boundaries, invalidity and modular arithmetic;70 mpmath100-digit
scientific value/partial references plus19 analytic branch fixtures.
- Symbolic derivatives cross-check independently derived formulas and numerical
forward partials. Polynomial assumptions, exact rational collection, bounded
factorization, unknown equivalence, resource/cancellation atomicity and codec
round trips are separate contracts. They do not establish bit-exact executable
algebraic equivalence.
- First-witness lazy search, Fibonacci throughF92 withF93 checked overflow,
first-order factorial recurrence, resource and cancellation boundaries, explicit
model codecs and complete transition-evolution continuation are checked.
- Dimensions reject incompatible addition and scientific arguments before numeric
loss work. Exact rational unit rules, frozen dimensional exponents, source
ownership, unit-correct variation, optional sampled constraints, signed-zero
coefficient serialization and fitting use the same full objective.
- Full committed population/cache/best/RNG/generation/work state compares exactly
between continuous and restored runs at1/2/4 workers for representative iterator,
recurrence, scientific and dimensional problems. Wrong problem/grammar/policy
contexts reject resume. Existing checkpoint corruption, process recovery,
SIGINT/SIGTERM and atomic replacement contracts remain in the full manifests.
- A separate published0.4-versus-D5 probe compares default scalar/discrete runs
at1/2/4 workers, including cache payload, RNG and counters. Both byte streams
have SHA256 `da6ddcaccff952f932668c91357bc6abd8453ff644afb95c3587281aa0b201f0`.
The probe was rebuilt after the final implementation repairs and again produced
these exact bytes. This within-runtime default check is not cross-platform numerical
qualification. A separate installed two-translation-unit consumer includes the
new recurrence, symbolic, scientific and dimensional headers in both translation
units; exact CMake3.16.0 compilation/link/run passed against the relocated package.

See [the frozen protocols and all outcomes](d5-results/RESULTS.md). There are12
paired iterator runs and24 final mathematical benchmark runs. Original and
intermediate24-run records remain alongside the final records. No new prime
campaign or optimizer comparison was run.

## Scope and compatibility

The measured local environment is Ubuntu24.04.4 LTS, Linux6.8.12-1021-tegra,
ARM64/aarch64, GCC13.3.0 (Ubuntu13.3.0-6ubuntu2~24.04.1), libstdc++13 headers
`__GLIBCXX__=20240904`, runtime `libstdc++.so.6.0.33`, glibc/libm2.39
(Ubuntu2.39-0ubuntu8.8), CMake4.4.4 and exact minimum CMake3.16.0.
The environment, header macros, actual sanitizer runtime linkage and compile/link
commands are retained in the selected JSON records. Linux GCC
success does not establish Windows/MSVC, macOS/AppleClang, Clang/libc++ or other
platform support. Strict floating-point requirements and sanitizers must appear
in both compile/link evidence; development warning flags remain private.

Package metadata remains0.4.0: `Genetik::Genetik`, C++20, minimum CMake3.16,
`Threads::Threads`, transitive strict-FP, source compatibility target and include
spellings remain intact. D5 is not a release. Consistently recompile dependent
translation units; source compatibility does not promise ABI compatibility.
New operator IDs append; old node IDs/default grammar draws and published
scalar/discrete checkpoint identities remain unchanged. New full-model codecs
are explicit and distinct. Earlier unqualified D5 dimensional-run contexts are
rejected after the production-policy correction.

Optional arbitrary precision is incomplete. Other bounded subsets and numerical
limitations are explicit in [the API guide](ADVANCED_MATHEMATICS_D5.md) and
[scientific rules](D5_SCIENTIFIC_RULES.md). D6 has not begun. The127 frozen
historical files, original research refs, published assets, main and tags are
checked independently; exact preservation records remain in the ignored audit.
