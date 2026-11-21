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
Full `ScalarMathematicsRun` checkpoints also bind the ordered custom IDs and
arities in the run identity, including when no saved candidate currently uses
those operators. A changed custom grammar is therefore rejected on restore.
Custom IDs may not shadow a built-in operator name or alias. Unicode output prints the
registered ID as a call, and LaTeX uses `\operatorname{ID}` so a custom
operation is not displayed as a built-in one. Second-partial evaluation returns
zero for custom subtrees independent of the requested variables; active custom
nodes report unavailable because the API has no custom Hessian callback.

See `examples/quickstarts/custom_scalar_operator.cpp` for a stable
`logaddexp(a,b)` registration and use in symbolic search.
Its two analytic partials are evaluated independently from a stable logistic
form; the smaller partial is not computed as one minus the larger partial.

For `logaddexp(a,b)`, the checked direct expression
`log(exp(a)+exp(b))` uses 6 nodes and is invalid at the three tested extreme
pairs because its intermediate exponentials overflow or underflow. The stable
built-in composition `max(a,b)+log1p(exp(0-abs(a-b)))` uses 12 nodes. The
registered primitive uses 3 nodes. All four pairs `(0,0)`, `(1000,1000)`,
`(-1000,-1000)`, and `(1000,-1000)` evaluate to `log(2)`, `1000+log(2)`,
`-1000+log(2)`, and `1000`, respectively, within `1e-12` absolute error for
the stable composition and primitive. The direct expression matches at `(0,0)`
and is rejected by strict checked evaluation for the other pairs.
