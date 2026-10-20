# Supplied-structure aggregate coefficient recovery

Frozen [protocol](TYPED_FITTING_PROTOCOL.md); [raw record](typed-regression-results/fitting/results.txt)
and SHA256.json. Jetson/GCC13 Release, strict native binary64. No randomness or
analytic fit. This fixture uses a known structure to isolate numerical refinement.

Initial model: `(matvec(matrix[2,3](1.0,1.0,1.0,1.0,1.0,1.0),x0) + vector[2](1.0,1.0))`.
Initial equal-component MSE5.286458333333333, independently checked from symmetric
training moments. Final faithful model:

```
(matvec(matrix[2,3](1.5,(-0.75),0.25,(-0.5),2.0,1.25),x0) + vector[2](0.625,(-1.375)))
schema{x0:vector[3]} -> vector[2]
```

Two constant occurrences/eight independent coefficients,5 nodes/depth3. All eight
coefficients recovered exactly for this dyadic identifiable fixture; matching final
MSE0. Maximum component error0 and invalid rows0 for each training27/interior64/
exterior64 set. Freeze accepted model before evaluating either evaluation set.

617 actual loss calls (one baseline plus616 probes),624 proposals,16 accepted
improvements,8 duplicates;16659 actual rows and300479 arithmetic primitives including
loss arithmetic. Stop MinimumStep, improved=true. Limits5000 calls/512 sweeps/20000
proposals, step1 down to1e-10, bounds[-8,8], no target threshold. Recorded elapsed
.00948349s is a single short observation, not a performance claim. Post-fit range
verification is separate reporting work (155 rows); no extra fitting loss call was
used to certify the returned matching pair.

Deterministic tests repeat the fit and matching counters, verify topology/input
ownership, independent equal-valued coefficients, invalid probes after accepted
progress, local budget retention and coupled zero-coordinate stalling. Integrated
workers/cadence/failure tests are distinct from this serial fixture. This does not
establish unique coefficients for arbitrary evolved expressions, a global optimum,
or that fitting is necessary for the operation-discovery problem.
