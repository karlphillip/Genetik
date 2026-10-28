# I2 — Python interoperability

Genetik 0.6.0 adds an optional Python interface to the existing native
symbolic-regression implementation. The C++ engine remains authoritative for
candidate generation, fitting, scoring, expression evaluation, RNG, candidate
publication, and work accounting. Python does not contain a second evolutionary
algorithm. The generic C++ GA remains usable without Python dependencies.

## Build and install

The core C++ package remains C++20, CMake >= 3.16, and exports
`Genetik::Genetik`. Configure the Python-free native build with
`-DGENETIK_BUILD_PYTHON=OFF` (the default). The optional Python package uses
the pinned build backend and pybind11 in `pyproject.toml` and currently declares
CPython >=3.12,<3.13.

In a clean virtual environment with the documented Python prerequisites:

```sh
python -m pip install build
python -m build --wheel --sdist
python -m pip install dist/genetik-0.6.0-*.whl
```

Use the qualified requirements file in `docs/i2-results/` for reproduction.
Wheel platform support is restricted to the platforms and ABI tags actually
qualified; inspect `docs/I2_QUALIFICATION.md` before redistributing an artifact.
The source distribution includes the native/Python sources, release guides,
example and SRBench adapter, and excludes generated qualification logs/artifacts.

## Estimator

```python
import numpy as np
from genetik import GenetikRegressor

X = np.linspace(-1.0, 1.0, 24).reshape(-1, 1)
y = 2.0 * X[:, 0] + 1.0

model = GenetikRegressor(random_state=42, n_jobs=1, loss_budget=100000)
model.fit(X, y)
prediction = model.predict(X)
print(model.expression_)
```

The estimator exposes `fit`, `predict`, `get_params`, and `set_params`, with
fitted expression and execution statistics. Inputs must be finite, nonempty
numeric arrays with `X` shaped `(n_samples, n_features)` and `y` shaped
`(n_samples,)`. Data is copied into native-owned storage. Prediction evaluates
the selected C++ expression; it does not refit.

`max_time`, when supplied, requests cooperative cancellation. It does not
terminate native computation at a hard deadline. If cancellation occurs, only
a fully prepared and committed candidate can be exposed; model availability
depends on whether such a candidate exists.

## Expressions and SRBench

`expression_` is the canonical Genetik expression. The explicit SymPy export
supports the documented scalar grammar and uses variable identities rather
than textual replacement. Unsupported expression forms are rejected. Domain
and strict arithmetic differences remain documented in the expression API.
Agreement of exported and native evaluation on sampled inputs is a numerical
cross-check, not a general proof of equivalence.

`integrations/srbench/` contains the adapter for the pinned upstream method
layout. Its `est` is the public estimator and its `model(est, X=None)` returns a
SymPy-compatible expression with the supplied dataset column names.

## Compatibility

Adding the regression facade is additive for C++ source/API use. Binaries must
be rebuilt against their chosen package version. The versioned scalar and
discrete checkpoint readers explicitly accept compatible 0.5.0 payloads under
the same codec, runtime, resource and numerical-contract checks; generic GA
checkpoint loading continues to require an exact package version. No payload
is silently reinterpreted. See `docs/I2_MIGRATION.md`.
