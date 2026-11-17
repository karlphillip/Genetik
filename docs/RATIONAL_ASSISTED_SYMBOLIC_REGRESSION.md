# Rational-assisted symbolic initialization

`symbolicRegression` has an optional final provider argument. The provider gets
only the immutable training `RegressionProblem`, the reserved candidate count,
and a positive initializer work allowance. It returns at most that many
expressions and a reported work-unit total. Before population evaluation, each
expression is checked against the problem schema and hard expression limits,
the enabled operator grammar, power metadata, and constant bounds. Each seeded
slot then follows the ordinary candidate preparation and population publication
path; its objective evaluation and any configured constant refinement use the
ordinary training-loss budget. Unfilled reserved slots and all remaining slots
use ordinary random initialization.

Provider exceptions propagate and fail the search call; they are not converted
into an empty batch. Initializer preparation is synchronous and has no
cooperative cancellation hook, so a cancellation request cannot interrupt a
provider already running. The work allowance is an application-reported bound,
validated after return; providers must enforce it internally. When the search
cannot admit the full initial population under its candidate/loss budgets, it
returns the budget stop before invoking the provider.

The driver caps reserved seeds at `min(8, floor(population/4))`; the remainder
of the population always uses ordinary random initialization.

The empty provider path retains the original construction loop and RNG draw
order. Enabled runs are reproducible for a deterministic provider, fixed
training rows, configuration, and seed. Providers are application-owned and
must honor their passed work allowance; the driver rejects excess reported work.
The driver does not serialize providers in checkpoints. Applications resuming a
run must persist their own provider result and use the existing expression
checkpoint path. The generic GA and variation algorithm contain no rational
regression dependency.

`RationalSymbolicRegression.hpp` supplies the first provider. It requires a
one-input MSE problem, copies only its training rows into the existing bounded
rational fitter, and returns training-ranked executable rational expressions.
It does not read validation/assessment rows or reference coefficients. Its
initializer allowance is the maximum of the fitter's native linear algebra
counter and arithmetic counter (including LM work); these different counters are
reported by the fitter and are not hardware-equivalent operations. Candidate
evaluation work is separately charged through symbolic training-loss budgets.

The fitter family remains

`(p0 + p1*x + ... + pm*x^m) / (1 + q1*x + ... + qn*x^n)`.

Fixing `Q(0)=1` removes scale ambiguity but excludes functions whose reduced
denominator vanishes at zero, including `1/x`. Cross-multiplied least squares is
only the QR starting estimate. Candidate selection inside the rational fitter
uses executable quotient prediction SSE. Optional LM refinement remains off
unless explicitly enabled in `RationalConfig`. Strict division and rational
domain diagnostics are unchanged: no clipping or finite replacement is done.
Near-zero denominators can be invalid or uncertain, and denominator separation
is not an accuracy guarantee.

## Frozen internal comparison (before execution)

Protocol: `docs/rational-assisted-screening/PROTOCOL.md`. The three methods are
ordinary symbolic search, the deterministic rational fitter, and rational-seeded
symbolic search. The fitter is run once per case, not once per stochastic seed.
All candidate rows are generated deterministically, split before runs, and only
training rows reach fitting/search. Validation and assessment are scored after
training-only model selection. Each stochastic arm uses seeds 11, 29, and 47.
Noisy and near-pole cases are excluded from this clean-data comparison.
