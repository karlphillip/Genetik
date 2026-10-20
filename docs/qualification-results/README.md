# OOP qualification and organization evidence

Starting checkout: 8b633264cd2d5ab464facfcceae45c7826832d2d, clean.
Local platform remains Jetson ARM64, GCC/libstdc++ 13.3.0, glibc/libm 2.39,
CMake 4.4.4 and exact 3.16.0. No hosted CI or new platform claim.

Before source movement, a preserved accepted-checkpoint Release executable
`../evaluation-oop-baseline` was compared to the selection-dispatch repair using:

```
../evaluation-oop-baseline --replay prime
../build-release/genetik_evaluation_workspace --replay prime
../evaluation-oop-baseline --replay arithmetic
../build-release/genetik_evaluation_workspace --replay arithmetic
# Also both executables: --replay arithmetic 4 (short scheduled/unscheduled case)
```

The three stdout captures compare byte-for-byte: prepared initial candidates and
fitness, final expression/fitness, best history, variation/refinement decisions,
counters, stopping outcome and final RNG serialization. Wall times on stderr are
excluded. Original binary came from the accepted build before edits; this is not
a comparison of two entry points using the edited headers. Captures and SHA256
manifest accompany final verification logs here. Historical campaign data is intact.

New deterministic OOP checks run in CTest and as an isolated installed application;
none requires discovering the scheduling optimum. Full post-organization verification
commands/outcomes are recorded in docs/OOP_QUALIFICATION.md.
