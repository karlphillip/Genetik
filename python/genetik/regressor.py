"""scikit-learn-compatible wrapper for the native Genetik scalar regressor."""

from __future__ import annotations

import math
import numbers
from typing import Optional, Sequence

import numpy as np
from sklearn.base import BaseEstimator, RegressorMixin
from sklearn.utils.validation import check_X_y, check_array, check_is_fitted

from . import _native


class GenetikRegressor(RegressorMixin, BaseEstimator):
    """Evolve a checked scalar expression using Genetik's C++ engine.

    ``max_time`` requests cooperative cancellation at the native engine's
    candidate/generation boundaries. It is a soft limit: one in-flight fitting
    preparation can finish after the deadline. Genetik never interrupts a C++
    evaluation or publishes a partial generation.
    """

    def __init__(
        self,
        random_state: Optional[int] = 0,
        n_jobs: int = 1,
        evaluation_budget: int = 100000,
        loss_budget: int = 100000,
        population_size: int = 64,
        max_generations: int = 2000,
        max_time: Optional[float] = None,
        operators: Sequence[str] = ("add", "sub", "mul"),
        max_nodes: int = 31,
        max_depth: int = 6,
        max_abs_constant: float = 8.0,
        fit_constants: bool = True,
        max_fit_loss_attempts: int = 256,
        continuous_probability: float = 0.5,
        training_threshold: float = 1e-14,
        fit_threshold: float = 1e-14,
    ) -> None:
        self.random_state = random_state
        self.n_jobs = n_jobs
        self.evaluation_budget = evaluation_budget
        self.loss_budget = loss_budget
        self.population_size = population_size
        self.max_generations = max_generations
        self.max_time = max_time
        self.operators = operators
        self.max_nodes = max_nodes
        self.max_depth = max_depth
        self.max_abs_constant = max_abs_constant
        self.fit_constants = fit_constants
        self.max_fit_loss_attempts = max_fit_loss_attempts
        self.continuous_probability = continuous_probability
        self.training_threshold = training_threshold
        self.fit_threshold = fit_threshold
        self._native = None

    def fit(self, X, y):
        """Fit from finite numeric observations, copying them into native storage."""
        # A failed refit must never leave a stale model looking like the new fit.
        self._native = None
        for name in (
            "n_features_in_", "feature_names_in_", "expression_", "expression_nodes_",
            "expression_depth_", "training_mse_", "loss_attempts_", "n_generations_",
            "stop_reason_", "timed_out_", "search_stats_", "sympy_expression_",
            "last_fit_attempt_stats_",
        ):
            if hasattr(self, name):
                delattr(self, name)
        self._require_real_numeric(X, "X")
        self._require_real_numeric(y, "y")
        x_checked, y_checked = check_X_y(
            X,
            y,
            dtype=np.float64,
            ensure_2d=True,
            ensure_min_samples=1,
            ensure_min_features=1,
            y_numeric=True,
        )
        seed = self._checked_seed()
        self._validate_configuration()
        feature_names = self._feature_names(X, x_checked.shape[1])
        operators = tuple(self.operators)
        candidate = _native.Regressor(
            seed,
            int(self.n_jobs),
            int(self.evaluation_budget),
            int(self.loss_budget),
            int(self.population_size),
            int(self.max_generations),
            int(self.max_nodes),
            int(self.max_depth),
            float(self.max_abs_constant),
            bool(self.fit_constants),
            int(self.max_fit_loss_attempts),
            float(self.continuous_probability),
            operators,
            self.max_time,
            float(self.training_threshold),
            float(self.fit_threshold),
        )
        # The binding copies array values into std::vector<RegressionRow> before
        # releasing the GIL. The C++ engine never retains NumPy memory.
        try:
            stats = candidate.fit(np.ascontiguousarray(x_checked), np.ascontiguousarray(y_checked))
        except TimeoutError as error:
            # SRBench can deliver SIGALRM while native work has released the GIL.
            # Accept only an already committed native model; never synthesize one.
            if not candidate.fitted:
                error.committed_model = False
                error.search_stats = dict(candidate.stats)
                raise
            stats = candidate.stats
        if not candidate.fitted:
            self.last_fit_attempt_stats_ = dict(stats)
            if stats["timed_out"]:
                error = TimeoutError("Genetik stopped with committed_model=False")
            else:
                error = RuntimeError(
                    "Genetik completed with committed_model=False: " + str(stats["stop_reason"])
                )
            error.search_stats = dict(stats)
            error.committed_model = False
            raise error

        self._native = candidate
        self.n_features_in_ = int(x_checked.shape[1])
        if feature_names is not None:
            self.feature_names_in_ = feature_names
        elif hasattr(self, "feature_names_in_"):
            del self.feature_names_in_
        self.expression_ = candidate.expression
        self.expression_nodes_ = int(stats["nodes"])
        self.expression_depth_ = int(stats["depth"])
        self.training_mse_ = float(stats["training_mse"])
        self.loss_attempts_ = int(stats["loss_attempts"])
        self.n_generations_ = int(stats["published_generations"])
        self.stop_reason_ = str(stats["stop_reason"])
        self.timed_out_ = bool(stats["timed_out"])
        self.search_stats_ = dict(stats)
        names = list(feature_names) if feature_names is not None else [
            "x{}".format(i) for i in range(self.n_features_in_)
        ]
        self.sympy_expression_ = candidate.sympy_expression(names)
        return self

    def predict(self, X):
        """Evaluate the exact selected C++ expression on new rows."""
        check_is_fitted(self, attributes=["_native", "n_features_in_"])
        self._require_real_numeric(X, "X")
        x_checked = check_array(
            X,
            dtype=np.float64,
            ensure_2d=True,
            ensure_min_samples=0,
            ensure_min_features=1,
        )
        if x_checked.shape[1] != self.n_features_in_:
            raise ValueError("X feature count differs from fitted model")
        return self._native.predict(np.ascontiguousarray(x_checked))

    def sympy_expression(self, feature_names=None):
        """Return the selected equation as a SymPy-compatible expression string."""
        check_is_fitted(self, attributes=["_native", "n_features_in_"])
        if feature_names is None:
            if hasattr(self, "feature_names_in_"):
                feature_names = list(self.feature_names_in_)
            else:
                feature_names = ["x{}".format(i) for i in range(self.n_features_in_)]
        return self._native.sympy_expression(list(feature_names))

    def _checked_seed(self):
        if self.random_state is None:
            return 0
        if isinstance(self.random_state, bool) or not isinstance(self.random_state, numbers.Integral):
            raise ValueError("random_state must be an integer in [0, 2**32-1] or None")
        seed = int(self.random_state)
        if seed < 0 or seed > 2**32 - 1:
            raise ValueError("random_state must be in [0, 2**32-1]")
        return seed

    def _validate_configuration(self):
        if isinstance(self.n_jobs, bool) or not isinstance(self.n_jobs, numbers.Integral):
            raise ValueError("n_jobs must be an integer in [1, 256]")
        if int(self.n_jobs) < 1 or int(self.n_jobs) > 256:
            raise ValueError("n_jobs must be in [1, 256]")
        integer_values = {
            "loss_budget": self.loss_budget,
            "evaluation_budget": self.evaluation_budget,
            "population_size": self.population_size,
            "max_generations": self.max_generations,
            "max_nodes": self.max_nodes,
            "max_depth": self.max_depth,
            "max_fit_loss_attempts": self.max_fit_loss_attempts,
        }
        for name, value in integer_values.items():
            if isinstance(value, bool) or not isinstance(value, numbers.Integral) or int(value) <= 0:
                raise ValueError("{} must be a positive integer".format(name))
        if int(self.population_size) < 2:
            raise ValueError("population_size must be at least 2")
        if int(self.loss_budget) < int(self.population_size):
            raise ValueError("loss_budget must admit the initial population")
        if int(self.max_nodes) > 1023 or int(self.max_depth) > 64:
            raise ValueError("expression limits exceed the supported bound")
        if int(self.max_generations) > 2**32 - 1:
            raise ValueError("max_generations exceeds the supported uint32 range")
        if not isinstance(self.fit_constants, (bool, np.bool_)):
            raise ValueError("fit_constants must be boolean")
        if self.max_time is not None and (
            isinstance(self.max_time, bool)
            or not isinstance(self.max_time, numbers.Real)
            or not math.isfinite(float(self.max_time))
            or float(self.max_time) <= 0
            or float(self.max_time) > 1.0e9
        ):
            raise ValueError("max_time must be a positive finite number or None")
        for name, value in (
            ("max_abs_constant", self.max_abs_constant),
            ("continuous_probability", self.continuous_probability),
            ("training_threshold", self.training_threshold),
            ("fit_threshold", self.fit_threshold),
        ):
            if not isinstance(value, numbers.Real) or not math.isfinite(float(value)):
                raise ValueError("{} must be finite".format(name))
        if self.max_abs_constant < 0:
            raise ValueError("max_abs_constant must be nonnegative")
        if not 0 <= self.continuous_probability <= 1:
            raise ValueError("continuous_probability must be in [0, 1]")
        if self.training_threshold < 0 or self.fit_threshold < 0:
            raise ValueError("fitness thresholds must be nonnegative")
        allowed = {"add", "sub", "mul", "div", "log", "sin", "cos", "exp", "sqrt", "powi", "pow"}
        if not isinstance(self.operators, (tuple, list)):
            raise ValueError("operators must be a tuple or list of operator names")
        try:
            operators = tuple(self.operators)
        except TypeError as exc:
            raise ValueError("operators must be a sequence of operator names") from exc
        if not operators or any(not isinstance(op, str) or op not in allowed for op in operators):
            raise ValueError("operators must be nonempty supported operator names")
        if len(set(operators)) != len(operators):
            raise ValueError("operators must not contain duplicates")

    @staticmethod
    def _require_real_numeric(values, name):
        try:
            array = np.asarray(values)
        except Exception as exc:
            raise TypeError("{} must be a real numeric array".format(name)) from exc
        if array.dtype.kind not in "biuf" or array.dtype.itemsize > 8:
            raise TypeError("{} must use bool/integer/float dtypes no wider than 64 bits".format(name))

    @staticmethod
    def _feature_names(X, dimensions):
        columns = getattr(X, "columns", None)
        if columns is None:
            return None
        names = list(columns)
        if len(names) != dimensions or any(not isinstance(name, str) for name in names):
            raise ValueError("DataFrame feature names must be strings matching the feature count")
        if len(set(names)) != len(names):
            raise ValueError("DataFrame feature names must be unique")
        return np.asarray(names, dtype=object)
