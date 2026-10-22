# Run observability and checkpoints (format 1)

Implemented API and persistence contract. Executed local evidence is recorded in
[RUN_CHECKPOINTING_RESULTS.md](RUN_CHECKPOINTING_RESULTS.md); hosted qualification
of this operational milestone remains pending.

Checkpointing is synchronous and coordinator-owned, only with an idle executor and
no pending population edits. Generic run snapshots own candidate values; explicit
CandidateCheckpointCodec specializations encode their representations. Arbitrary
external subclasses are not implicitly serializable. Restore creates fresh run
objects, binds application-verified immutable problem/preparation contexts, validates
state, and installs a committed population without evaluation or random draws.
No raw object memory, pointers, vtables or STL layouts are serialized.

The first codec is for unconstrained scalar ExpressionCandidate. It preserves postfix
node kinds, binary64 coefficient bits, variable indices, power metadata, fitness,
preparation origin/generation and selection data. The application must verify its
problem/configuration identity before trusting cached fitness. Checksums detect
accidental corruption, not malicious forgery. Do not resume untrusted files.

The envelope is GENETIK-CHECKPOINT format 1 with payload length and FNV-1a64 checksum;
the payload uses big-endian unsigned integers, IEEE coefficient bit patterns and
length-prefixed strings. Reader/writer limits cap source and fields before allocation.
Library compatibility, codec ID and runtime fingerprint are explicit. The initial
qualification requires the same compiler, standard library, libc/libm, floating-point
environment, execution setting, configuration and input identities. Worker-count
changes are rejected in exact-prime resume. This is not cross-toolchain portability.

State includes committed population/archive/generations, MT19937 state and initial
seed, outer candidate calls, algorithm attempts/publications, variation counters,
training/preparation counters, raw initial best and committed-work watermark.
Normal committed-boundary save/resume must match uninterrupted evolution exactly.
After a failed private generation, population/archive remain at G while actual
attempts, work and post-failure RNG remain consumed. Resume constructs G+1 from that
post-failure state; it does not replay discarded speculative draws or refund work.
No incomplete population or worker-local scratch is serialized. Fitting cadence
continues from committed generation, never failed-attempt count.

On POSIX, write an exclusive temporary file in the destination directory, fsync it,
read/check completion, rename into place, then fsync the directory. A numbered file
is established before latest.gtkc is replaced; pruning follows successful replacement.
Failures before rename preserve the old destination. A directory-fsync error after
rename means durability is uncertain and is reported, not rolled back. Guarantees
remain qualified by filesystem/hardware fsync/rename behavior. Automatic directory
creation does not fsync every newly created ancestor: the replacement durability
contract concerns an established destination directory, not durable creation of a
whole new directory hierarchy. No final checkpoint
can be triggered by SIGKILL, power loss, kernel failure or a process crash.

Pretty and LaTeX formatting are presentation only, separate from canonical export.
Tree grouping and operand order are preserved, including right-associated same-
precedence children. There is no folding, cancellation or algebraic rewriting.
Variable-rendering callbacks supply display text, not executable substitutions.
The prime presentation score is 1-loss/1001; search still minimizes the original
exact-prefix loss. Binary64 presentation can merge adjacent loss values; only zero
loss is displayed as1 (positive losses rounding to1 are capped at nextafter(1,0)).
Canonical text and full objective remain authoritative.

## Public C++ entry points

Include `<RunCheckpoint.hpp>` for the generic envelope, bounded binary field
reader/writer, durable POSIX storage and run-state snapshots. It does not include
Expression or symbolic-regression headers. The explicit execution seams are:

- `RunCheckpointAccess::random(context)` / `restoreRandom(freshContext,state)`.
- `RunCheckpointAccess::algorithm(algorithm)` / `restoreAlgorithm(freshAlgorithm,state)`.
- `RunCheckpointAccess::population(population)` /
  `restorePopulation<Candidate,Ordering>(state,context)`, returning an owned population.
- `writeCommittedPopulation(writer,state)` / `readCommittedPopulation<Candidate>(reader,codecContext)`.

A custom `CandidateCheckpointCodec<C>` supplies `identifier`,
`encode(CheckpointWriter&,const C&)` and `decode(CheckpointReader&,const Context&)`.
Specializations are trusted serialization code, including cache restoration access;
they must construct/validate owned genomes and bind the verified immutable context.
They are not a general writable-genome API. There is no default codec for arbitrary
Creature subclasses. Custom algorithm/variation/evaluator services must explicitly
encode their additional persistent state and identity. Do not assume a captured
base algorithm also captures arbitrary derived virtual-hook state.

`<ScalarExpressionCheckpoint.hpp>` supplies
`CandidateCheckpointCodec<ExpressionCandidate>` and `ScalarCheckpointContext` for
ordinary scalar-expression candidates bound to a verified RegressionProblem and
optional preparation. Template/constrained candidates and derived candidates are
rejected by this codec. TrainingWork, PreparationStatistics and VariationStatistics
have explicit field codecs; fresh objectives/refiners/variation policies have
validated restore methods. The prime adapter assembles these components. The
existing one-call symbolic-regression driver does not gain an implicit resume mode.

`CheckpointDocument` carries codec/context/runtime identity, optional generation and
payload. `encodeCheckpoint` / `decodeCheckpoint` check its version, length and
checksum. `loadCheckpoint(path)` bounds input before accumulating bytes.
`saveCheckpointAtomic(path,document)` supports an optional pre-rename fault hook;
`saveCheckpointSeries(directory,generation,document,keep)` maintains numbered files
and `latest.gtkc`. A run exclusively owns its directory; do not have independent
writers target it. Foreign latest identity and generation rollback are rejected.
Unrelated filenames are not pruned. Retention counts distinct generations; repeated
saves at G replace G atomically. Cleanup happens only after latest is known-good.

Default limits:64MiB source/payload writer,1MiB individual reader strings,100,000
candidates for the generic reader. The prime loader further caps candidate count at
the configured population. These bounds do not cap allocator overhead or process
memory. Every integer field is an explicit unsigned64 big-endian value. Binary64
and binary32 fields encode IEEE bit patterns, not object padding. Strings are
unsigned64 byte length followed by bytes. MT19937 uses its classic-locale logical
stream state (624 words and index, validated), not engine memory. Files are version 1;
older/newer formats and unknown codecs require an explicit migration, never guesswork.

The runtime fingerprint records package version, compiler, GNU header release,
libc major/minor, optimization/NDEBUG mode, size/endian and rounding mode. On Linux
it also fingerprints the actual loaded libc/libm/libstdc++ object bytes, catching
distro patch differences. This is necessary metadata, not proof that arbitrary
client callbacks/build flags are identical. Exact continuation requires the same
qualified revision/build and independently callable deterministic implementations.
The authoritative package version now lives in `genetik/Version.hpp`; CMake reads
it. Its value remains0.2.0; package name, target and version matching are unchanged.
No architecture-dependent library binary or new third-party dependency is installed.
POSIX persistence is qualified locally; other platforms are not newly qualified.

See the [compiled installed example](../examples/quickstarts/checkpoint.cpp).

## Expression formatting

Include `<ExpressionFormatting.hpp>`:

```cpp
formatExpression(expression, ExpressionTextFormat::Canonical); // unchanged export
formatExpression(expression, ExpressionTextFormat::Unicode);
formatExpression(expression, ExpressionTextFormat::LaTeX);
formatExpression(expression, ExpressionTextFormat::Unicode,
    [](std::size_t i, ExpressionTextFormat) {
        return i == 0 ? "n/1000" : "other";
    });
```

A callback is borrowed synchronously and supplies text appropriate for the selected
format, including LaTeX escaping. Canonical mode ignores it. Variable aliases are
parenthesized as opaque display atoms. Default names are x0, x1... (LaTeX x_{0}).
The formatter preserves left-associative infix grouping and explicitly parenthesizes
right children of equal precedence. It retains x·2, rather than reordering x*2 into
2·x. Log becomes ln, multiplication uses · / \\cdot, square root uses √(...) /
\\sqrt{...}, Powi uses superscripts / ^{n}, functions retain argument grouping.
Negative literals and signed zero are retained; coefficients use max_digits10.
Scientific numeric literals use mantissa\\times10^{exponent} in LaTeX. Pretty/LaTeX
are not input languages and do not promise parser round trips. Powi notation still
means Genetik's checked sequential computational operator, including child validation
under exponent zero. Formatting does not evaluate or simplify anything.

Default output/workspace text budget is 1 MiB, node limit 100,000; traversal is iterative.
Temporary concatenation/callback allocation is not a total-memory guarantee.
Operational errors propagate; expressions and returned strings have independent
ownership. Concurrent calls share only immutable expression data.

## Prime CLI and human display

```sh
out/build/release/genetik_prime_regression --search-exact --seed 42 --threads 4 \
  --loss-budget 1000000 --report-seconds 30 \
  --checkpoint out/runs/prime-exact/seed42/checkpoints --checkpoint-seconds 300 \
  --checkpoint-keep 3 --metrics-file out/runs/prime-exact/seed42/metrics.jsonl

out/build/release/genetik_prime_regression \
  --resume out/runs/prime-exact/seed42/checkpoints/latest.gtkc --loss-budget 2000000
```

Default `--report-format pretty` displays one current committed best individual.
Explicit `--top N` adds compact secondary entries without deduplication. The
optional `--report-format raw` retains dense original fields and defaults to top 5.
Both use the same RegressionOrdering and cache; normalized presentation is never
used for ranking, fitting or stopping. Human deltas compare successive displayed
current bests, using up/down/equality accurately. Best-ever fitness/loss/generation
and a distinct archived formula are reported separately when available. Final and
winning displays contain canonical text and LaTeX. Winning pretty output starts
`EXACT PREFIX RECOVERED` and reports the generation/time/seed/threads/work/checkpoint.

Actual binary64 loss can reach1001 when the secondary ratios/sum round upward,
despite the strict mathematical upper bound. The pre-existing objective is unchanged;
that valid endpoint displays fitness 0. For real nonzero exact-prefix losses (>=1),
presentation is native1-loss/1001. Adjacent scores may merge in presentation rounding;
full loss remains authoritative. For tiny synthetic positive inputs which would
round the presentation to1, the helper uses nextafter(1,0) to retain the zero-loss
iff-one-score distinction. This guard never changes real search fitness.

JSONL uses `genetik.prime-progress/1`. Every snapshot includes Unix-millisecond time,
all original raw snapshot/rank keys, canonical/decoded formulas, typed normalized
presentation fitness and checkpoint path. Raw field values are strings, preserving
uint64 precision and optional diagnostic spellings; consumers convert known numeric
fields explicitly. Only the requested displayed individuals are evaluated. A sink
appends records and flushes each snapshot. Reporting rows are counted separately,
never charged against training work. Initial, resumed, periodic and final/winning
records are explicit. Reports and periodic checkpoints use aligned steady-clock
deadlines and wait for the next safe publication; no worker/reporter thread accesses
committed population state while it is mutating.

Resume loads/checks before any worker becomes active. Seed, worker setting, fitting
mode/cadence and previous total budget are inherited unless explicitly provided.
Incompatible seed/threads, problem/reference, scaling/rounding/objective, grammar,
shape/limits, population, rates, elites or fitting policy are rejected. Total budgets
can be extended (or reduced to no less than consumed work), not treated as additional
allowances. The inherited outer candidate-call ceiling also remains enforced.
Generation-attempt ceilings are operational stopping controls. Output paths, interval,
retention, top count, report format and metrics sink can change. New runs refuse a
directory already containing latest.gtkc; resume it or use a fresh directory.

Elapsed time accumulates active run wall time, excluding time between processes.
Reporting counters/prior display state persist; a final save after rendering captures
shutdown diagnostics as well. Cancellation flags, live workers and scratch do not
persist. Cancellation before initialization commits leaves no resumable population;
no checkpoint is fabricated for an incomplete initial population. Extending a budget which previously interrupted preparation continues from
post-failure RNG/work; it is not promised identical to starting with a larger budget.
After an unclean crash, work after the last durable checkpoint is unknown and cannot
be recovered/accounted; budget enforcement resumes from the durable counters. No
write-ahead per-objective journal or exactly-once external side-effect contract is
claimed. Cooperative cancellation drains and saves the actual final counters.

SIGINT and SIGTERM only set a lock-free atomic request in the handler. Normal control
flow drains, discards incomplete staging, writes the final checkpoint and reports it.
Normal bounded/cancelled outcomes retain exit0 and an explicit stopping state; save,
load or metrics I/O failure exits1 with a prominent error. A failure before rename
preserves the old destination; after-rename directory-fsync failure is reported as
uncertain durability. Final shutdown attempts a checkpoint before and after the
final display so both its path and completed reporting counters can be retained.
SIGKILL/power loss/crashes cannot request this final save.

Reproducing all first 1,000 supplied primes would demonstrate an exact fit to that
finite prefix. It would not establish a general closed-form formula for all prime
numbers. No mathematical operator, objective, fitting algorithm, grammar or selection
semantics changed for this milestone. [Qualification and smoke](RUN_CHECKPOINTING_RESULTS.md).
