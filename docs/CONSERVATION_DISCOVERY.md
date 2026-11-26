# Grouped trajectory conservation scoring

`ConservationDiscovery.hpp` adapts the ordinary scalar symbolic-regression
engine to observed `(y,v)` states grouped by trajectory. Candidate expressions
receive only the ordered state coordinates. The grouping is retained for
scoring; time, trajectory identifiers, initial-condition labels, equations of
motion, and reference invariant values are not candidate inputs.

For each candidate, all finite predictions are divided by their largest
absolute value. Let `W` be the sum of squared deviations from each trajectory's
mean, `B` the sample-count-weighted sum of squared deviations of trajectory
means from the grand mean, and `T = W + B`. The scale-free score is
`W/T + lambda * node_count`; lower is better. The objective rejects invalid
predictions, zero or sub-threshold magnitude, zero total variation, and
`B/T` below the configured minimum. The default minimum between-trajectory
fraction is 0.01 and default magnitude floor is `1e-12`. Metrics expose the
within fraction, between fraction, and normalized separation
`sqrt(B / observation_count)`.

Deterministic contracts use three analytically sampled harmonic circles with
radii 1, 2, and 3. They show that `y^2+v^2` has zero within fraction, that
`y^2` is worse, that nonzero scaling leaves the normalized component
unchanged, and that constants and tiny candidates are rejected. A bounded
ordinary-search smoke test verifies that the objective runs through the public
symbolic search hooks. These tests do not establish recovery on held-out
trajectories. The harmonic and quartic oscillator discovery campaigns, frozen
search protocol, selected equations, and independent Lie-derivative verifier
remain to be qualified.
