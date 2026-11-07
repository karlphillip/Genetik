# Constructive multivariate rational regression

Genetik provides an optional deterministic fitter for two independent real-valued inputs. It is separate from symbolic regression and from the genetic algorithm. Calling `genetik::multivariate_rational::fit` is explicit; ordinary GA initialization, evolution, RNG state, checkpoints and defaults are not involved.

## Supported family

The fitted function is `P(x,y) / Q(x,y)` with numerator and denominator total degree at most two and `Q(0,0)=1`. The deterministic graded basis is `1, x, y, x^2, xy, y^2`; a degree limit selects a prefix of this ordering. The full numerator and denominator pair therefore has at most eleven free coefficients because the denominator constant is fixed. The fitter supports proper, improper and polynomial forms within those bounds. It does not support higher degrees, additional inputs, conditionals, arbitrary recursion, or new operators.

The design matrix uses the normalized rational identity `target * (Q - 1) = P - target` and the library's existing column-pivoted Householder QR implementation. Rank, pivot-ratio, coefficient, expression, row, arithmetic, linear-algebra and estimated-workspace limits are checked. The selected model is ranked using training observations only. QR is the default. Existing bounded Levenberg–Marquardt refinement is available only when `Config::refineWithLM` is explicitly set; it has independent work counters and shares the configured overall limits.

## C++ usage

Include `<MultivariateRationalFunctionRegression.hpp>` and link `Genetik::Genetik`. A complete consumer example is [multivariate_rational_regression.cpp](../examples/quickstarts/multivariate_rational_regression.cpp). The public namespace is `genetik::multivariate_rational`; its `Observation` stores `(x, y, target)`, and `Config` owns all limits. The fitter receives only the observations and configuration. It does not receive reference coefficients or held-out data.

A successful result has `Status::Success` and a selected model index. A model exposes the selected degrees and coefficient vectors, ordinary Genetik `Expression` objects, training SSE/score, pivot ratio, and QR/LM counters. Call `modelIsConsistent(model)` before inspecting or exporting a mutable model. `Model::predict` and `assessDomain` perform full consistency validation themselves. Mutating coefficients, bases, normalization, or expressions independently makes the model invalid for those operations.

## Domain assessment

`assessDomain` evaluates the fitted denominator over a closed rectangle using bounded interval subdivision. `DomainAssessmentStatus::Valid` with `DomainFinding::SeparatedFromZero` means the fitted denominator was conservatively enclosed away from zero by the requested margin. `DomainAssessmentStatus::Invalid` with `DemonstratedZero` means certified opposite signs were found within the connected rectangle. `Uncertain` means numerical enclosure or the configured cell/depth budget could not establish either result; `DomainResult::reason` distinguishes resource exhaustion from numerical uncertainty. A malformed model returns `DomainAssessmentStatus::InvalidModel`; malformed assessment inputs return `InvalidConfiguration`.

The certificate concerns only the returned fitted denominator. It says nothing about whether that denominator matches the unknown generating function or whether predictions are accurate. Prediction numerical validity, denominator separation, and predictive accuracy are separate properties. Finite agreement on a finite set of points does not prove mathematical equivalence.

Certification is qualified only for GNU/Clang builds with Genetik's strict floating-point options, native IEEE-754 binary64, nearest rounding and gradual underflow. The header still compiles on other compilers supported by Genetik, but domain assessment returns `Uncertain` there. Genetik's existing expression header rejects fast-math/finite-only compilation. The interval implementation is not a formal proof across arbitrary compilers or hardware.

## Scientific limits and provenance

C2.2 independently confirmed clean reconstruction on four new rational families and useful noisy interpolation/safe-extrapolation accuracy under its frozen degree-two protocol. Noisy data selected a different denominator degree on 2/8 rational tracks even when predictions remained accurate. The near-pole negative controls showed large noisy extrapolation errors: small coefficient shifts can move a denominator zero substantially. A fitted-model domain certificate does not locate the true denominator zero. Rank deficiency, ill-conditioning, measurement noise, input coverage and configured coefficient bounds can prevent or alter recovery. Coefficient bounds may exclude otherwise valid representations. Training error alone is not structural recovery.

This integration follows the S14 prototype, the C2.1 domain-safety repair, and the independently frozen C2.2 confirmation. The research implementation and datasets remain outside the installed package; no Python runtime or new dependency is required. See the retained C2.2 protocol/results in the research record for the exact confirmed operating envelope.
