import numpy as np
import pandas as pd
import pytest
import sympy
import os
import subprocess
import gc
from sklearn.exceptions import NotFittedError
from sklearn.base import clone

from genetik import GenetikRegressor


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


def test_srbench_adapter_model_and_complexity():
    from integrations.srbench.experiment.methods.genetik.regressor import (
        complexity,
        model as srbench_model,
    )

    x, y = fixture()
    frame = pd.DataFrame(x, columns=["feature_0"])
    estimator = small_model().fit(frame, y)
    exported = srbench_model(estimator, frame)
    assert isinstance(exported, str)
    assert isinstance(sympy.sympify(exported, locals={"feature_0": sympy.Symbol("feature_0")}), sympy.Expr)
    assert complexity(estimator) == estimator.expression_nodes_


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
    with pytest.raises(TypeError):
        model.fit(np.array([[1.0 + 2.0j], [3.0 + 4.0j]]), np.array([1.0, 2.0]))


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


def test_python_matches_independent_native_cpp_facade():
    executable = os.environ.get("GENETIK_NATIVE_REFERENCE")
    if not executable:
        pytest.skip("set GENETIK_NATIVE_REFERENCE to the compiled native reference fixture")
    native = subprocess.check_output([executable], text=True).splitlines()
    x, y = fixture()
    model = small_model(
        evaluation_budget=40,
        loss_budget=256,
        max_fit_loss_attempts=8,
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
