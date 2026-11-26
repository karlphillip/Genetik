# Installed-package qualification

Head: `023c24a69fb9d2cafff189941800e308063a2a3b`  
Base: `1344982fe798108abf2993ba204cd399d93ab947`

The isolated command was:

```sh
ctest --test-dir build/release --output-on-failure -V \
  -R '^genetik_installed_package$'
```

CTest applied its normal 1,200-second timeout. The integration script completed
the producer configure/build/install and ordinary consumer configure, then its
ordinary consumer build exceeded the script's 300-second per-command cap. The
exact timed command was:

```text
cmake --build <workspace>/ordinary consumer build -j2
```

It was terminated after 300.030 seconds. CMake had reached 89%, compiling
`custom_scalar_operator_quickstart`; the preceding output showed
`objectives_quickstart`, `selection_strategies_quickstart`, and
`multi_component_templates_quickstart` built successfully. The overall CTest
test returned failure after 301.97 seconds. This is a bounded failure of the
full installed-package matrix; later relocation, C++23, and embedding stages
were not reached.

After the timeout, the artifacts already built against the installed package
were checked directly:

| Installed artifact | Result |
|---|---|
| `installed_consumer` | Exited successfully. |
| `quickstarts/objectives_quickstart` | Exited successfully; asymmetric search and coordinate fit both selected 10, while MSE selected 4. |
| `quickstarts/selection_strategies_quickstart` | Exited successfully; reported 60 vector-objective evaluations and the expected Pareto examples. |

The disposable command log remains under the local Release build directory at
`build/release/tests/genetik-delivery-fixture/commands.log`. This README keeps
the measured failing step and useful partial-consumer checks reviewable without
checking in generated build logs.

## Follow-up on resumed head `54eab33dc9edb914811ef6a3960c94f59bc94022`

The isolated test was rerun with the repository's ordinary CTest timeout
(1,200 seconds) and the script's normal 300-second per-command cap. It failed
again in `cmake --build <workspace>/ordinary consumer build -j2`, after 300.017
seconds. The compiler was building
`quickstarts/complex_coefficient_fitting_quickstart`; Make had completed the
preceding quickstarts and reported 93% progress. The captured command transcript
is retained locally at
`build/release/tests/installed-package-timeout-54eab33.log`.

To complete the checks beyond that cap, the existing producer/install and
partially compiled ordinary-consumer artifacts were resumed with the same
integration procedure and a 1,200-second per-command cap. The ordinary
multi-TU consumer, every installed quickstart and its runtime/dependency checks,
relocation after moving the prefix and deleting producer/build trees, package
metadata and rejected-version/component checks, C++23 consumer, and source
embedding parent project all passed. The final command log and generated
consumer artifacts remain in the ignored Release build fixture directory. The
repository's normal 300-second cap was not changed.
