# User-defined scalar operators

Applications may opt into unary, binary, or ternary scalar operators by adding immutable
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

The `arity` field may be 1, 2, or 3. Ternary callbacks receive arguments in
postfix tree order, from left to right, and their partial callback must return
one derivative for each argument in that same order. Custom operators remain
opt-in: the default grammar and its random draw sequence are unchanged.

Scalar candidate checkpoints store the custom operator ID, not executable code.
Restore requires the application to supply a descriptor with the same ID and
arity through `ScalarCheckpointContext::customOperators`; missing registrations
are rejected. Existing built-in checkpoint records remain unchanged. The
ordinary grammar remains unchanged unless custom descriptors are supplied.
Full `ScalarMathematicsRun` checkpoints also bind the ordered custom IDs and
arities in the run identity, including when no saved candidate currently uses
those operators. A changed custom grammar is therefore rejected on restore.
Custom IDs may not shadow a built-in operator name or alias. Canonical output
uses the reserved form `custom("ID", arguments...)`, so IDs containing
arithmetic punctuation cannot be reinterpreted by the built-in expression
parser. That parser rejects `custom` calls because it has no operator registry;
applications restore custom expressions from checkpoints with their registered
descriptors. Unicode output prints the registered ID as a call, and LaTeX uses
`\operatorname{ID}` so a custom operation is not displayed as a built-in one.
Second-partial evaluation returns
zero for custom subtrees independent of the requested variables; active custom
nodes report unavailable because the API has no custom Hessian callback.

See `examples/quickstarts/custom_scalar_operator.cpp` for a registered
`example.fma/1` primitive using `std::fma(a,b,c)` with analytic ordered
partials `(b,a,1)`, and its use in symbolic search. The public tests also
exercise LM fitting of a coefficient in the third argument and matching versus
mismatched checkpoint registrations.

At `a=1+2^-27`, `b=1-2^-27`, and `c=-1`, the ordinary tree `(a*b)+c`
evaluates to exactly `0` using 5 nodes because the product rounds before the
addition. `std::fma(a,b,c)` evaluates to exactly `-2^-54` using 4 nodes.
