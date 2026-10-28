import numpy as np
import pandas as pd
import pytest
import sympy
from scipy import sparse
import os
import subprocess
import gc
from importlib.metadata import version
from importlib.util import module_from_spec, spec_from_file_location
from pathlib import Path
from sklearn.exceptions import NotFittedError
from sklearn.base import clone

import genetik
from genetik import GenetikRegressor


def test_package_version_matches_distribution():
    assert genetik.__version__ == version("genetik") == "0.6.0"


def small_model(**kwargs):
    options = dict(
        random_state=42,
        n_jobs=1,
        loss_budget=80,
        population_size=8,
        max_generations=4,
        max_nodes=15,
        max_depth=5,
        max_fit_loss_attempts=8,
        max_abs_constant=8.0,
    )
    options.update(kwargs)
    return GenetikRegressor(**options)


def fixture():
    x = np.array([[-1.0], [0.0], [1.0], [2.0]])
    y = np.array([1.0, 1.0, 3.0, 7.0])
    return x, y


def test_import_clone_fit_predict_and_stats():
    x, y = fixture()
    model = small_model()
    assert clone(model).get_params() == model.get_params()
    assert model.fit(x, y) is model
    prediction = model.predict(x)
    assert prediction.shape == y.shape
    assert np.all(np.isfinite(prediction))
    assert model.expression_
    assert model.search_stats_["loss_attempts"] <= model.loss_budget
    assert model.expression_nodes_ >= 1
    with pytest.raises(ValueError, match="feature count"):
        model.predict(np.ones((2, 2)))


def test_repeat_seed_is_deterministic_and_params_are_sklearn_compatible():
    x, y = fixture()
    a, b = small_model(), small_model()
    a.fit(x, y)
    b.fit(x.copy(), y.copy())
    assert a.expression_ == b.expression_
    assert a.search_stats_["rng_state"] == b.search_stats_["rng_state"]
    np.testing.assert_array_equal(a.predict(x), b.predict(x))
    assert a.set_params(max_generations=3) is a
    assert a.get_params()["max_generations"] == 3


def test_successful_refit_replaces_model_and_restarts_from_configured_seed():
    x, y = fixture()
    model = small_model().fit(x, y)
    first_expression = model.expression_
    first_stats = model.search_stats_
    first_predictions = model.predict(x)
    assert model.fit(x, y) is model
    assert model.expression_ == first_expression
    assert model.search_stats_["rng_state"] == first_stats["rng_state"]
    np.testing.assert_array_equal(model.predict(x), first_predictions)


def test_parallel_worker_configuration_is_repeatable():
    x, y = fixture()
    a = small_model(n_jobs=2).fit(x, y)
    b = small_model(n_jobs=2).fit(x, y)
    assert a.expression_ == b.expression_
    assert a.search_stats_["rng_state"] == b.search_stats_["rng_state"]
    assert a.search_stats_["loss_attempts"] == b.search_stats_["loss_attempts"]
    np.testing.assert_array_equal(a.predict(x), b.predict(x))


def test_multivariate_dataframe_names_and_sympy_prediction_agree():
    x = pd.DataFrame({"velocity": [-2.0, -1.0, 1.0, 2.0], "mass": [1.0, 2.0, 2.0, 1.0]})
    y = np.array([1.0, 2.0, 3.0, 4.0])
    model = small_model(population_size=10, loss_budget=100).fit(x, y)
    exported = sympy.sympify(model.sympy_expression(), locals={
        name: sympy.Symbol(name) for name in x.columns
    })
    fn = sympy.lambdify(tuple(sympy.Symbol(name) for name in x.columns), exported, modules="math")
    independent = np.array([fn(*row) for row in x.to_numpy()])
    np.testing.assert_allclose(model.predict(x), independent, rtol=2e-13, atol=2e-13)
    assert model.sympy_expression_ == model.sympy_expression()


def test_sympy_export_keeps_similarly_named_variables_distinct():
    values = np.random.default_rng(11).uniform(-1.0, 1.0, (32, 2))
    frame = pd.DataFrame({"x1": values[:, 0], "x10": values[:, 1]})
    target = values[:, 0] + 2.0 * values[:, 1]
    model = small_model(
        random_state=42,
        n_jobs=1,
        population_size=16,
        max_generations=10,
        evaluation_budget=160,
        loss_budget=2048,
        max_fit_loss_attempts=16,
        max_abs_constant=4.0,
    ).fit(frame, target)
    expression = sympy.sympify(model.sympy_expression(), locals={
        "x1": sympy.Symbol("x1"), "x10": sympy.Symbol("x10")
    })
    assert expression.free_symbols == {sympy.Symbol("x1"), sympy.Symbol("x10")}
    reference = sympy.lambdify((sympy.Symbol("x1"), sympy.Symbol("x10")), expression, modules="math")
    independent = np.asarray([reference(*row) for row in values])
    np.testing.assert_allclose(model.predict(frame), independent, rtol=2e-13, atol=2e-13)


@pytest.mark.parametrize(
    "family,values,target,operators",
    [
        ("polynomial", np.linspace(-2.0, 2.0, 9), lambda x: x**3 - 2*x + 1, ("add", "sub", "mul")),
        ("rational", np.linspace(-2.0, 2.0, 9), lambda x: (x + 2)/(x**2 + 1), ("add", "sub", "mul", "div")),
        ("fractional_power", np.linspace(0.1, 2.0, 9), lambda x: 3.5*x**1.7, ("add", "sub", "mul", "div", "pow")),
        ("trigonometric", np.linspace(-1.0, 1.0, 9), lambda x: 2*np.sin(1.5*x + 0.3), ("add", "sub", "mul", "div", "sin", "cos")),
    ],
)
def test_exported_expression_matches_native_on_reference_domains(family, values, target, operators):
    x = values.reshape(-1, 1)
    y = target(values)
    # The target is used only to create this correctness fixture. The expression
    # is searched normally; this is not a target-expression seed or recovery claim.
    model = small_model(
        random_state=42,
        n_jobs=1,
        population_size=12,
        max_generations=5,
        evaluation_budget=60,
        loss_budget=512,
        max_fit_loss_attempts=12,
        max_abs_constant=8.0,
        operators=operators,
    ).fit(x, y)
    expression = sympy.sympify(model.sympy_expression(), locals={"x0": sympy.Symbol("x0")})
    independent = sympy.lambdify(sympy.Symbol("x0"), expression, modules="math")
    reference = np.asarray([independent(float(row[0])) for row in x])
    np.testing.assert_allclose(model.predict(x), reference, rtol=2e-12, atol=2e-12, err_msg=family)


def test_srbench_adapter_model_and_complexity():
    adapter_path = (
        Path(__file__).parents[2]
        / "integrations/srbench/experiment/methods/genetik/regressor.py"
    )
    spec = spec_from_file_location("genetik_srbench_test_adapter", adapter_path)
    adapter = module_from_spec(spec)
    spec.loader.exec_module(adapter)

    x, y = fixture()
    frame = pd.DataFrame(x, columns=["feature_0"])
    estimator = small_model().fit(frame, y)
    exported = adapter.model(estimator, frame)
    assert isinstance(exported, str)
    assert isinstance(sympy.sympify(exported, locals={"feature_0": sympy.Symbol("feature_0")}), sympy.Expr)
    assert adapter.complexity(estimator) == estimator.expression_nodes_


def test_invalid_shapes_types_nonfinite_and_predict_before_fit():
    model = small_model()
    with pytest.raises(NotFittedError):
        model.predict(np.ones((1, 1)))
    with pytest.raises(ValueError):
        model.fit(np.ones(4), np.ones(4))
    with pytest.raises(ValueError):
        model.fit(np.ones((4, 2)), np.ones(3))
    with pytest.raises(ValueError):
        model.fit(np.array([[1.0], [np.nan]]), np.array([1.0, 2.0]))
    with pytest.raises(TypeError):
        model.fit(np.array([["a"], ["b"]]), np.array([1.0, 2.0]))
    with pytest.raises(ValueError, match="Complex data not supported"):
        model.fit(np.array([[1.0 + 2.0j], [3.0 + 4.0j]]), np.array([1.0, 2.0]))


def test_sklearn_numeric_object_sparse_and_feature_name_conventions():
    x, y = fixture()
    object_model = small_model().fit(x.astype(object), y.astype(object))
    assert np.all(np.isfinite(object_model.predict(x)))

    with pytest.raises(TypeError, match="argument must be a string or a real number"):
        small_model().fit(np.array([[{"not": "numeric"}]], dtype=object), np.array([1.0]))
    with pytest.raises(ValueError, match="Sparse input is not supported"):
        small_model().fit(sparse.csr_matrix(x), y)
    with pytest.raises(ValueError, match="got None"):
        small_model().fit(x, None)

    frame = pd.DataFrame(x, columns=[10])
    model = small_model().fit(frame, y)
    assert not hasattr(model, "feature_names_in_")
    assert "x0" in model.sympy_expression()


def test_failed_refit_does_not_leave_stale_fitted_model():
    x, y = fixture()
    model = small_model().fit(x, y)
    with pytest.raises(ValueError):
        model.fit(np.ones(4), y)
    with pytest.raises(NotFittedError):
        model.predict(x)


def test_invalid_configuration():
    x, y = fixture()
    with pytest.raises(ValueError):
        small_model(random_state=-1).fit(x, y)
    with pytest.raises(ValueError):
        small_model(n_jobs=0).fit(x, y)
    with pytest.raises(ValueError):
        small_model(operators=("hidden",)).fit(x, y)
    with pytest.raises(ValueError):
        small_model(max_time=1.0e100).fit(x, y)


def test_cooperative_deadline_does_not_cancel_search_before_its_deadline():
    x, y = fixture()
    model = small_model(max_time=3.0).fit(x, y)
    assert model.expression_
    assert model.search_stats_["timed_out"] is False
    assert model.search_stats_["stop_reason"] != "cancelled"


def test_training_buffers_can_be_released_after_fit():
    x, y = fixture()
    model = small_model().fit(x, y)
    expected = model.predict(np.array([[0.0], [1.5]]))
    del x, y
    gc.collect()
    np.testing.assert_array_equal(model.predict(np.array([[0.0], [1.5]])), expected)


def test_timeout_reports_no_fabricated_model_when_no_candidate_commits():
    x, y = fixture()
    model = small_model(max_time=1e-12, loss_budget=1000, population_size=64)
    # This is a soft deadline: the initial generation can commit before the
    # timer is serviced. If it stops without a model, fit must say so; if it
    # has one, predictions must come from that committed expression.
    try:
        model.fit(x, y)
    except (TimeoutError, RuntimeError) as error:
        assert getattr(error, "committed_model", False) is False
        assert not hasattr(model, "expression_")
    else:
        assert model.expression_
        assert np.all(np.isfinite(model.predict(x)))


@pytest.mark.parametrize("workers", [1, 2])
def test_python_matches_independent_native_cpp_facade(workers):
    executable = os.environ.get("GENETIK_NATIVE_REFERENCE")
    if not executable:
        pytest.skip("set GENETIK_NATIVE_REFERENCE to the compiled native reference fixture")
    native = subprocess.check_output([executable, str(workers)], text=True).splitlines()
    x, y = fixture()
    model = small_model(
        evaluation_budget=40,
        loss_budget=256,
        max_fit_loss_attempts=8,
        n_jobs=workers,
    ).fit(x, y)
    assert model.expression_ == native[0]
    assert model.search_stats_["rng_state"] == native[1]
    assert [item for item in native[2].split("\t") if item] == model.search_stats_["raw_initial_expressions"]
    assert [item for item in native[3].split("\t") if item] == model.search_stats_["prepared_initial_expressions"]
    expected_counts = list(map(int, native[4].split("\t")[:6]))
    assert expected_counts == [
        model.search_stats_["loss_attempts"],
        model.search_stats_["row_attempts"],
        model.search_stats_["fitting_invocations"],
        model.search_stats_["fitting_loss_attempts"],
        model.search_stats_["candidate_evaluations"],
        model.search_stats_["published_generations"],
    ]
    assert float(native[4].split("\t")[6]) == model.training_mse_
    np.testing.assert_array_equal(model.predict(x), np.array([float(v) for v in native[5].split("\t") if v]))
