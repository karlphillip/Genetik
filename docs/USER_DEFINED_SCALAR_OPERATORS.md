# User-defined scalar operators

Applications may opt into unary or binary scalar operators by adding immutable
`CustomScalarOperator` descriptors to `ExpressionGrammar::customOperators`.
Each descriptor requires a stable unique ID and an evaluator that returns
`nullopt` for mathematical invalidity. Analytic partials in argument order are
optional: when supplied, Genetik checks finite results and uses them in the
existing derivative and LM fitting path; without them, primal evaluation and
coordinate fitting work while derivative-based fitting reports unavailable
derivatives. The application owns the mathematical domain and
must ensure its evaluator and partials describe the same function. Callbacks
must be deterministic, side-effect-free, and safe for concurrent calls when a
multi-worker execution context is used. Evaluation accounts one operation per
custom primal callback and one derivative-math call per requested partial
callback; work performed internally by application code is not individually
instrumented. Argument spans are invocation-local views and must not be retained
after a callback returns.

Scalar candidate checkpoints store the custom operator ID, not executable code.
Restore requires the application to supply a descriptor with the same ID and
arity through `ScalarCheckpointContext::customOperators`; missing registrations
are rejected. Existing built-in checkpoint records remain unchanged. The
ordinary grammar remains unchanged unless custom descriptors are supplied.

See `examples/quickstarts/custom_scalar_operator.cpp` for a stable
`logaddexp(a,b)` registration and use in symbolic search.

For `logaddexp(a,b)`, the checked direct expression
`log(exp(a)+exp(b))` uses 6 nodes and is invalid at the three tested extreme
pairs because its intermediate exponentials overflow or underflow. The stable
built-in composition `max(a,b)+log1p(exp(0-abs(a-b)))` uses 12 nodes. The
registered primitive uses 3 nodes. All four pairs `(0,0)`, `(1000,1000)`,
`(-1000,-1000)`, and `(1000,-1000)` evaluate to `log(2)`, `1000+log(2)`,
`-1000+log(2)`, and `1000`, respectively, within `1e-12` absolute error for
the stable composition and primitive. The direct expression matches at `(0,0)`
and is rejected by strict checked evaluation for the other pairs.
