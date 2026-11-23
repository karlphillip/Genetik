# Exact odd-sums recurrence discovery

This optional benchmark asks Genetik's public constructive recurrence
synthesizer to recover a recurrence from only the five observations
`0, 1, 4, 9, 16`. It declares fixed structural, evaluation, transition, and
storage budgets in the executable. The recurrence reference and held-out
values are used only after discovery has returned and printed its candidate.

Configure and run it independently of the default build:

```sh
cmake -S . -B build/odd-sums -DCMAKE_BUILD_TYPE=Release \
  -DGENETIK_BUILD_TESTS=OFF -DGENETIK_BUILD_EXAMPLES=OFF \
  -DGENETIK_BUILD_ODD_SUMS_BENCHMARK=ON
cmake --build build/odd-sums --target genetik_odd_sums_exact_discovery
./build/odd-sums/genetik_odd_sums_exact_discovery
```

The executable reports training recovery, an autonomous held-out rollout, and
an independent exact-polynomial induction check as separate outcomes. The
induction check recognizes an exact polynomial identity in the returned
transition and combines it with the initial values; this proves the recurrence
over mathematical integers for every natural index. It does not claim that
Genetik's checked Int64 execution can represent every square: checked execution
is valid only at indices whose intermediate values remain in Int64. A control
recurrence matches through index 2, then fails at index 3 to demonstrate a
finite counterexample separately from an inconclusive proof result.

The target is opt-in and adds no dependency to normal builds.
