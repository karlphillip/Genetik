# Frozen scientific campaign protocol

Recorded before search: feature/scientific-operators, based on ef0f580.
Independent targets: scripts/generate_scientific_references.py, mpmath1.2.1 at
200 and320 decimal digits, exact binary64 rational inputs; both precisions give
identical rounded binary64 targets. Data and identities: tests/fixtures/scientific.
Generation requires mpmath only; normal builds/search read retained numerical data.
Rows are increasing x. Decay: train k/8,k0..32; interior(k+.5)/8,k0..31;
exterior4.25+k/4,k0..7. Oscillation: train -4+k/4,k0..32;
interior -4+(k+.5)/4,k0..31; exterior -6+k/4 then4.25+k/4,k0..7.
Root: train -3+k/4,k0..24; interior -3+(k+.5)/4,k0..23;
exterior -5+k/4 then3.25+k/4,k0..7.

Cases/grammar, in selection order:
- decay exp(-x): add,sub,mul,exp.
- oscillation sin(x)+cos(2*x): add,sub,mul,sin,cos.
- root sqrt(1+x*x): add,sub,mul,sqrt,powi; exponent subset{2,3}.

Seeds0,1,2,3,42 sequential, population128, actual loss/candidate allowance10000,
generation ceiling2000, nodes15, depth5 (leaf1), constant bound8, terminals
{-2,-1,0,1,2} discrete only, no fitting, cadence1, serial CPU. Rank selection,
elites2, crossover.9, mutation.35; init depth4, mutation depth3; full probability.5,
grow.65, proposals8. Uniform feasible operator entries; power exponent uniform
among the configured feasible subset. A selected power mutation root has probability
.5 of a discrete exponent edit; otherwise ordinary subtree replacement. No draws
for that path when no power node is selected. No target injection or validity retries.
Stop MSE<=1e-24 or exact0; max-error recovery<=1e-6 reported independently on each
set. All seeds run once, no adjustments. Mean baseline fitted on training only.
Each run keeps committed training-best; representative minimizes training MSE,
nodes, faithful text, then first seed in declared order. Freeze all models and
representatives before evaluation. No claim of algebraic equivalence from samples.

Preselected file reproduction: decay, seed2, identical configuration, allowance10000.
Comparison excludes runtime and paths; expression, fitness and deterministic counters
must match. Ordinary work counts loss starts and actual visited rows. The exported
model's full valid primitive cost is not a measured search-wide primitive total;
scalar search has no search-wide primitive counter. Invalid expressions can stop early.
All-row reporting is outside search work and inspects every supplied row.
