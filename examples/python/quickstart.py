"""Fit and inspect a small model through Genetik's native C++ engine."""

import numpy as np

from genetik import GenetikRegressor


X = np.linspace(-1.0, 1.0, 24, dtype=np.float64).reshape(-1, 1)
y = 2.0 * X[:, 0] + 1.0

model = GenetikRegressor(
    random_state=42,
    n_jobs=1,
    evaluation_budget=2_000,
    loss_budget=20_000,
    max_generations=100,
)
model.fit(X, y)

predictions = model.predict(X[:3])
print("expression:", model.expression_)
print("predictions:", predictions)
print(
    "search statistics:",
    {
        key: model.search_stats_[key]
        for key in (
            "training_mse",
            "loss_attempts",
            "published_generations",
            "search_seconds",
            "stop_reason",
        )
    },
)
