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
