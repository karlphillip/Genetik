# I1 — Python interoperability

I1 adds an optional Python package over Genetik's existing C++ symbolic-regression
engine. The generic GA remains the sole evolutionary implementation. The package
name is `genetik`; installable estimators import from `genetik.GenetikRegressor`.

## Build and installation

The ordinary C++ build does not require Python or pybind11. The optional CMake
switch is `GENETIK_BUILD_PYTHON` (default `OFF`). Python wheels use scikit-build-core
and pybind11. The source package keeps the C++20 `Genetik::Genetik` target and its
CMake 3.16 minimum.

A source checkout can be built with the pinned build backend:

```sh
python -m pip install scikit-build-core==0.10.7 pybind11==2.13.6
python -m pip install .
```

A wheel is platform and Python ABI specific. I1 was built and installed on Linux
AArch64, Python 3.12.3, GCC 13.3.0, Ubuntu 24.04.4. This does not qualify Windows,
macOS, other Python ABIs, or other architectures. Runtime dependencies are NumPy,
scikit-learn, and SymPy; `docs/i1-results/requirements-qualified.txt` records the
complete tested lock. The optional package does not alter the standard C++ install.

## Estimator API

```python
from genetik import GenetikRegressor

model = GenetikRegressor(
    random_state=42,
    n_jobs=1,
    evaluation_budget=100_000,
    loss_budget=100_000,
)
model.fit(X_train, y_train)
y_hat = model.predict(X_test)
equation = model.sympy_expression()
```

`fit`, `predict`, inherited sklearn `get_params`/`set_params`, and sklearn fitted
state checks are supported. `fit` copies finite real data to owned C++ rows before
releasing the GIL. It accepts dense two-dimensional `X` and one-dimensional `y`;
real bool/integer/float input dtypes no wider than 64 bits are converted to
binary64. Complex, nonnumeric, nonfinite, sparse, multi-output, empty-training,
and shape-incompatible input is rejected. Prediction runs the exact selected C++
expression and raises when strict mathematical evaluation is invalid at an input
row; it never performs an implicit refit.

`random_state=None` means seed zero, keeping the adapter deterministic rather than
introducing hidden entropy. `n_jobs` selects the existing native worker count.
`evaluation_budget`, `loss_budget`, `max_generations`, expression limits, operator
grammar and the existing coordinate-fitting settings map to
`SymbolicRegressionConfig`; the fitter, loss, grammar defaults, and search policy
are not reimplemented in Python.

Fitted attributes include `expression_`, `expression_nodes_`, `expression_depth_`,
`training_mse_`, `loss_attempts_`, `n_generations_`, `stop_reason_`, `timed_out_`,
and `search_stats_`. A search that has no committed model raises and reports
`committed_model=False`; no synthetic model is produced.

`max_time` is a **cooperative soft deadline**, not a hard interruption guarantee.
A run-owned native timer requests stop at the C++ engine's safe boundaries. An
in-flight candidate preparation/refinement may finish after the deadline. Genetik
does not interrupt a C++ evaluation or publish private partial generations.
SRBench's outer alarm remains a process-level safety net; the configured estimator
time limit normally requests native cancellation first.

## Expression export and domains

`expression_` is Genetik's canonical expression text. `sympy_expression()` exports
the selected postfix tree by operator identity, preserving precedence, variable
indices and round-trip constant precision. DataFrame column names are mapped by
feature index, never by substring replacement, and must be unique Python/SymPy
identifiers. Array inputs use `x0`, `x1`, ... . Supported mappings cover the
qualified scalar grammar (arithmetic, `log`, `sin`, `cos`, `exp`, `sqrt`, integral
`powi` and real `pow`). An operator without a faithful mapping raises an error.

The SymPy form describes the same expression on its real mathematical domain.
Strict C++ arithmetic rejects invalid operations (including division by signed
zero and nonpositive natural-log arguments); SymPy may represent a larger or
complex domain. For variable powers, consumers must restrict comparisons to inputs
where the selected C++ expression is valid. Floating evaluation order can also
differ, so numerical comparison uses a documented tolerance rather than claiming
bit identity or universal mathematical equivalence.

## SRBench adapter

The local adapter is under `integrations/srbench/`. It uses SRBench's pinned
`algorithms/genetik` installer/metadata directory and
`experiment/methods/genetik` method directory. `regressor.py` exposes `est`,
`model(est, X=None)`, and `complexity(est)`. The model function returns a
SymPy-compatible string with the supplied dataframe's actual column names.

The installer requires `CONDA_PREFIX` and `GENETIK_SOURCE_DIR`, installing only
into the active environment without `sudo` or a global package change. This is
intentional: I1 is local and has no stable public source asset to fetch. Before an
upstream SRBench contribution, point the installer to a reviewed, immutable source
revision. The adapter was checked against SRBench commit
`2aabbb60dad41d6e5d52779ce134191a874b756c`; this is integration readiness, not a
full SRBench campaign or a competitive-performance claim.

## Compatibility and limits

- C++20, CMake >= 3.16, and `Genetik::Genetik` remain the native contract.
- Python code is an adapter only; it owns no search RNG, candidate variation,
  fitness objective, fitting optimizer, or expression evaluator.
- The package does not expose checkpoint save/resume. Existing C++ checkpoint
  formats and contracts are unchanged; Python fits start fresh native runs.
- Dense, real, single-output scalar regression is supported. Typed/vector/tensor,
  discrete iterator, coupled-system, and complex regression APIs are not exposed.
- Python 3.12/Linux AArch64 is the qualified environment. The `Requires-Python`
  range follows the pinned build-tool compatibility and SRBench's >=3.7 contract,
  but other interpreters/platforms need their own wheel and runtime qualification.
