# Migrating to the Genetik 0.5.0 development API

Genetik 0.5.0 is an unreleased development pre-release. It combines the D5
and D6 additions with the published 0.4.0 API. It requires C++20 and CMake
>= 3.16 and preserves the `Genetik::Genetik` CMake target.

## Existing applications

Keep generic GA consumers on their existing headers and configuration. The D5
and D6 mathematical interfaces remain optional includes and do not change the
default generic search grammar. Rebuild every dependent translation unit
consistently when updating: public headers changed, so binary layout and
cross-minor ABI compatibility are not promised.

## Checkpoints

Generic checkpoint loading remains exact-version. To migrate a supported
published v0.4.0 scalar-expression or discrete-evolution/2 checkpoint, call the
explicit v0.5 multi-policy reader/loader documented in the checkpoint API.
Only those recorded v0.4.0 and v0.3.0 codec contracts are accepted. The loader
does not reinterpret D5/D6 scientific experimental state or unknown payloads.
Validate candidate codec, configuration identity, dataset identity and
resource-limit identity before resuming. See the checkpoint quickstart and
qualification report.

## Scientific APIs

Add only the domain headers used by the application. D5 supports checked
integer/recurrence evaluation, scalar symbolic transformations, physical
dimensions and bounded derivative-based regression under its documented
limits. D6 supports bounded equation systems, residuals on supplied ODE/PDE
observations, bounded tensor/matrix operations and the documented restricted
complex/statistical/special-function expressions. These are not general ODE
or PDE solvers, arbitrary tensor networks, unrestricted complex regression,
or proofs of expression equivalence. Numerical agreement on sampled inputs
does not establish universal equivalence.

## Build and install

```sh
cmake -S . -B out/build/consumer -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/consumer
cmake --install out/build/consumer --prefix out/install/consumer
```

Installed consumers should use `find_package(Genetik 0.5 CONFIG REQUIRED)` and
link `Genetik::Genetik`. Use a fresh build directory when changing versions or
compiler/sanitizer options. The release candidate qualification report records
the tested toolchain and any optional dependency limitation.
