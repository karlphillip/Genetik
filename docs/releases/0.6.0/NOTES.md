# Genetik 0.6.0 — Python Interoperability Development Pre-release

Genetik 0.6.0 adds an optional Python distribution for the existing C++20
symbolic-regression engine. It is a development pre-release focused on
accessibility and interoperability; it does not change the default search
algorithm or claim improved search performance.

## Included

- Existing general-purpose C++ genetic-algorithm and symbolic-regression APIs.
- A Python-independent C++ regression facade.
- Optional pybind11 extension and `genetik.GenetikRegressor` estimator.
- NumPy inputs, deterministic seed and worker configuration, expression and
  search-statistic inspection, and predictions evaluated by the native model.
- Explicit SymPy export for the supported scalar expression grammar.
- A locally qualified SRBench adapter using the pinned upstream interface.
- Optional Python wheel and source-distribution build configuration.
- Correct cooperative timeout scheduling with a regression test; cancelled
  searches still expose a model only when a complete candidate was committed.

The normal C++ build remains independent of Python, NumPy, pybind11,
scikit-learn, and SymPy. C++ remains C++20, CMake >= 3.16, with the installed
target `Genetik::Genetik`.

## Qualification scope

Only the exact Python, operating-system, architecture, compiler, and wheel
environments listed in `docs/I2_QUALIFICATION.md` are qualified. No support is
claimed for other Python versions or Windows/macOS without separate evidence.
The hosted x86-64 and ARM64 matrix is a separate approval gate until it has
actually run on the final candidate SHA.

## Limitations

- Python `max_time` is cooperative; it is not a hard real-time deadline.
- Cancellation cannot promise a model if no fully committed candidate exists.
- SymPy export is limited to explicitly supported operators and preserves the
  documented strict-domain semantics; export is rejected for unsupported forms.
- Numerical agreement on a finite dataset is not proof of global symbolic
  equivalence.
- Python support is optional and does not make every generic scikit-learn
  estimator check applicable or supported.
- This release makes no claim that Genetik is faster or more accurate than
  version 0.5.0, PySR, PyOperon, or another search system.

See `docs/I2_QUALIFICATION.md`, `docs/I2_HOSTED_QUALIFICATION_PROPOSAL.md`,
`docs/I2_PYTHON_INTEROPERABILITY.md`, and `docs/I2_MIGRATION.md` for the
candidate's verified scope and reproducible procedures.
