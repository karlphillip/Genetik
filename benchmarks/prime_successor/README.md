# Prime successor benchmark

The frozen CSV and Python evaluator are protocol `genetik-prime-successor-benchmark-v1`. Their dataset identity, partitions, baseline results, and Python expression semantics are unchanged. Native Genetik expressions use the distinct `genetik-prime-successor-native-v1` candidate protocol.

## Build the native adapter

The adapter is an optional benchmark/test executable and is built when local tests are enabled:

```sh
cmake -S . -B build/prime-successor -DGENETIK_BUILD_TESTS=ON
cmake --build build/prime-successor --target genetik_prime_successor_native
```

Freeze an expression before scoring it:

```sh
python3 -B benchmarks/prime_successor/benchmark.py \
  --track native_predictive --native-evaluator build/prime-successor/genetik_prime_successor_native \
  --freeze-candidate 'x0 + 6' --output candidate.json
```

Then score the frozen identity. The `--exclude-test` option is available for training/validation-only review:

```sh
python3 -B benchmarks/prime_successor/benchmark.py \
  --track native_predictive --native-evaluator build/prime-successor/genetik_prime_successor_native \
  --frozen-candidate candidate.json
```

Use `--native-target gap` when the expression returns a positive gap; the default target is the successor. Target interpretation is recorded in the frozen manifest and cannot be changed during assessment. The native evaluator's runtime measurement includes process startup and parsing; mathematical work is reported separately from node visits, evaluator operations, iterations, and iteration operations.

## Native predictive language

The adapter parses with `parseDiscreteExpression` and evaluates the resulting owned `DiscreteExpression` through Genetik's checked evaluator. It does not implement another expression engine. The expression has one input, `x0`, and must return `Int64`. The predictive whitelist is integer literals, `x0`, checked addition/subtraction/multiplication/negation/modulo, integer comparisons, and lazy `if` expressions. The parser also recognizes other Genetik constructs, so the adapter rejects iterator and iteration nodes and every advanced integer function after parsing. No prime tables, primality oracle, external calls, or candidate-side successor enumeration are allowed.

Checked overflow and modulo-domain failures are mathematical invalidities. Node visits, operations, and workspace have explicit frozen per-query limits (4,096, 4,096, and 1 MiB). Resource exhaustion is reported distinctly and does not produce predictions. SIGINT/SIGTERM request cooperative cancellation; a cancellation aborts the benchmark with exit status 130 and emits no partial metrics. Work totals include evaluator-reported node visits, operations, iterations, and iteration operations. Iterations are disallowed in this predictive track and therefore remain zero. Operational adapter errors stop evaluation rather than being scored as mathematical invalidity.

The candidate identity binds the native protocol, frozen dataset hash, expression text, target, output policy, parsed node/constant counts, Genetik version, adapter executable SHA-256, parser limits, and evaluation limits. Assessment requires the same adapter binary used at freeze time. Native complexity is the Genetik postfix node count; stored parameters are integer literals. The optional future extrapolation range `[50,000, 100,000)` is reserved for confirmation and is not part of this dataset or development protocol.

The exact next-prime algorithm and bounded trial scan remain in the separate algorithmic track. Native predictive expressions do not gain iteration or search capabilities by sharing Genetik's evaluator.
