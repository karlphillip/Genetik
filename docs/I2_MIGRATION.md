# Genetik 0.6.0 migration notes

## C++ users

The existing C++20 API and `Genetik::Genetik` target remain available. Rebuild
consumers against 0.6.0 and keep the existing CMake minimum of 3.16. The new
Python regression facade is independently usable C++ code, but including it
does not require Python or pybind11. The Python extension is opt-in at configure
time and is not a dependency of the ordinary C++ build.

Scalar-expression and discrete run-checkpoint readers add explicit forward
compatibility for the corresponding 0.5.0 codec contracts. Compatibility still
requires runtime, numerical, configuration and resource identity validation.
Generic GA checkpoints remain exact-version. Existing checkpoint data is never
silently upgraded or rewritten by loading it.

## Python users

Install a wheel qualified for the exact Python ABI and platform in
`docs/I2_QUALIFICATION.md`. Construct `GenetikRegressor`, call `fit(X, y)`, and
then use `predict(X_new)`. Inputs must be finite numeric arrays with the
documented two-dimensional `X` and one-dimensional `y` shapes. The estimator
owns a copy of inputs used during fitting.

The estimator's `max_time` parameter is cooperative and is not a hard timeout.
After cancellation, a model is available only if a fully prepared candidate
was committed. Prediction uses the selected native C++ model. SymPy export is
explicit and limited to the faithfully supported scalar operators.

The release does not promise full scikit-learn estimator-check compatibility,
universal symbolic export, or support for unqualified Python/OS combinations.
