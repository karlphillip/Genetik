# S1 frozen regression protocol

This protocol is frozen before the evolutionary comparison. S1 does not include
prime or successor data.

## Fixed-structure comparison

The compiled `genetik_s1_fixed_structure_comparison` application compares the
existing coordinate fitter with the opt-in forward-parameter-Jacobian LM fitter.
Both receive the same supplied topology, training rows, coefficient starts,
bounds `[-8,8]`, and target function. Held-out rows are constructed separately
and are only evaluated after fitting. Each task uses two starts:

| Task | Target | Starts |
|---|---|---|
| Fractional power | `3.5*x^1.7` | `(a,b)=(2,1.2),(5,2.1)` |
| Trigonometric | `2*sin(1.5*x+0.3)` | `(1.5,1,0),(3.2,1.8,0.6)` |
| Exponential | `3*exp(-0.7*x)` | `(1.5,0.3),(4,1.1)` |
| Rational | `(x+2)/(x^2+1)` | `(c,d)=(1.3,1.2),(3,2)` |

Training has 30 points. Power uses `x=.2+.1*i`; trigonometric and rational use
`x=-2+.15*i`; exponential uses `x=.05+.1*i`, for `i=0..29`. Held-out sets
have 15 points on interleaved grids: power `.25+.2*i`, exponential
`.1+.2*i`, trigonometric and rational `-1.925+.3*i`. Coordinate fitting is
bounded to 1,024 objective calls, 64 sweeps, 2,048 proposals, initial step
`.4`, and minimum step `1e-10`. LM is bounded to 80 iterations, 128 objective
calls, 128 Jacobian batches, 30,000,000 mathematical operations, loss
tolerance `1e-22`, and the 64 MiB workspace ceiling. Both use the same
coefficient bounds and MSE observations. The app records wall and CPU time.

The coupled oscillator uses the existing D6 system, two channels with scales and
weights `(1,1)` and `(2,1)`, and shared parameters initialized at `(1,1.4)` with
bounds `[0,4]`. Coordinate fitting allows 4,096 objective calls; LM allows 100
iterations, 512 objective and Jacobian evaluations, 30,000,000 mathematical
operations, and 64 MiB workspace. Assessment uses a separately constructed
held-out D6 system.

## Evolutionary comparison

The compiled `genetik_s1_evolutionary_comparison` application runs seeds `701`
and `709`, once under each fitting method. Population size is 10, one elite,
four generation attempts, 50 candidate calls, 20,000 training-loss calls,
crossover `.8`, mutation `.4`, and one worker. The grammar is identical:
operators `Add, Subtract, Multiply, Pow, Log, Exp`; constants `{-2,-1,0,1,2}`;
real power constants `{.5,1.5,2}`; continuous coefficient probability `.4` on
`[-4,4]`; initial depth 4 and mutation depth 3. No target expression or fitted
coefficient is injected.

Both methods use the same per-candidate fitting allowance of 12 loss calls,
8 sweeps, and 64 proposals. LM additionally allows 8 iterations, 12 objective
calls, 8 Jacobian batches, 200,000 mathematical operations, and 64 MiB workspace
per fitting invocation. Training uses 24 points `x=.2+.1*i`; held-out assessment uses 12
points `x=.25+.2*i`. Search sees training observations only. The selected
candidate is frozen before held-out measurement. No optimizer parameters or
grammar probabilities are changed after seeing outcomes.

The comparison measures fixed-topology fitting and the effect of opt-in fitting
inside the existing evolutionary run. It does not claim that a fitting result
demonstrates structural discovery. Both methods use the same population engine,
selection, variation, and run-owned RNG.
