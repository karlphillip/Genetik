"""SRBench adapter for the qualified Genetik Python estimator."""

import sympy
from genetik import GenetikRegressor

# SRBench supplies the wall-time policy via this estimator attribute. Genetik's
# native cancellation is cooperative at committed candidate/generation bounds.
est = GenetikRegressor(
    random_state=0,
    n_jobs=1,
    max_time=300.0,
    loss_budget=100000,
)

# SRBench's official -test path honors eval_kwargs.test_params. Keep this
# smoke-only configuration small enough to exercise a complete commit within
# the harness's short adapter test; ordinary evaluations retain the estimator
# above and receive their normal fit_time_limit.
eval_kwargs = {
    "test_params": {
        "evaluation_budget": 128,
        "loss_budget": 1000,
        "population_size": 8,
        "max_generations": 4,
        "max_nodes": 15,
        "max_depth": 5,
        "max_fit_loss_attempts": 8,
    }
}


def model(estimator, X=None):
    """Return the selected native expression with the dataset's exact columns."""
    names = list(X.columns) if X is not None else None
    text = estimator.sympy_expression(names)
    symbols = {name: sympy.Symbol(name) for name in (names or [])}
    return str(sympy.sympify(text, locals=symbols))


def complexity(estimator):
    """Return native AST nodes, or SymPy operation count for an exported tree."""
    if hasattr(estimator, "expression_nodes_"):
        return int(estimator.expression_nodes_)
    if isinstance(estimator, sympy.Basic):
        return int(sympy.count_ops(estimator))
    raise RuntimeError("Genetik estimator is not fitted")
