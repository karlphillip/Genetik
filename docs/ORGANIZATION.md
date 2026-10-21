# Source, workspace and output organization

Genetik is header-only. Established public headers stay under `genetik/` and install
under `include/Genetik`; include spelling, `Genetik::Genetik`, source compatibility
target `genetik_lib`, executable names and explicit output options remain intact.

| Role | Location |
| --- | --- |
| Public headers, private transitive implementation headers | `genetik/` |
| Small demonstrations and application support | `examples/` |
| Installed compiled scalar/symbolic/typed quickstarts | `examples/quickstarts/` |
| Typed regression/campaign application | `apps/typed_regression.cpp` |
| Generic file regression application | `apps/regression_search.cpp` |
| Compiled campaign applications | `apps/campaigns/` |
| Contract tests, data fixtures, external consumers | `tests/` |
| Performance/replay harnesses | `benchmarks/` |
| Python/shell development and reporting helpers | `scripts/` |
| Package helpers | `cmake/` |
| Contracts, accepted experiment evidence and historical reproductions | `docs/` |
| Fresh configured builds (never moved caches) | `out/build/{release,debug,sanitize,minimum,tsan}` |
| Manual installs and external builds | `out/install/`, `out/consumers/` |
| New experiments, generated data, logs, disposable copies | `out/runs/`, `out/data/`, `out/logs/`, `out/tmp/` |
| Compact retained parent-workspace history and published assets | `../archive/genetik/` |

Current ordinary-tool commands, from the repository root:

```sh
cmake -S . -B out/build/release -DCMAKE_BUILD_TYPE=Release -DGENETIK_WARNINGS_AS_ERRORS=ON
cmake --build out/build/release -j2
cmake -E chdir out/build/release ctest --output-on-failure
mkdir -p out/logs out/data
out/build/release/genetik_demo 42 20 --threads 2 > out/logs/character.txt
out/build/release/genetik_prime_regression --generate --output out/data/prime-new
python3 scripts/check_hygiene.py --workspace
```

Ordinary CMake/CTest 3.16 commands remain supported (`cmake -E chdir`, not the newer
`ctest --test-dir`). On this Jetson use `../.tools/bin/cmake`/`ctest` (4.4.4), or
`../delivery-tools/cmake-min/bin/cmake`/`ctest` (exact 3.16.0). Tool locations stay
stable: `delivery-tools/` includes source/build/install material and is not a scratch
build. No parent/global compiler settings or installed development flags changed.

Tests write only inside their configured binary directory or explicit OS temporary
directories. The installation fixture deliberately retains path names containing
spaces, removes only its own disposable producers, and exercises scalar, symbolic,
nonmathematical OOP and multi-TU usage. Library-only installation needs no Python.

Prime/cadence applications require an explicit `--output DIR`; multivariate data
has an explicit generation destination. Other programs write stdout/stderr: redirect
under `out/logs/` or `out/runs/`. Never overwrite accepted experiment results. Use
managed Git worktrees for future additional source checkouts; disposable delivery
fixtures remain temporary copies under a build/output directory.

`check_hygiene.py` checks root roles and generated caches/objects under source roles;
it permits intentional .txt/.csv/.json evidence and fixtures. `out/` is opaque and
ignored. `--workspace` additionally checks the local parent entry names (`Genetik/`,
`.tools/`, `delivery-tools/`, `archive/`); it does not modify anything or follow links.
The self-test exercises misplaced output and symlink handling. CI runs repository
mode because a hosted parent workspace is owned by the runner. In-source CMake
configuration is rejected with an out-of-source command hint. Root entries are an
explicit allowlist: review and update it when adding a genuine new source role.

All 127 frozen evidence files keep their original bytes and paths. Historical
transcripts are not rewritten: former build paths and source locations refer to
those historical checkouts. The mechanical campaign mappings are `scripts/cadence_campaign.cpp` →
`apps/campaigns/cadence_campaign.cpp` and `scripts/constant_fitting_campaign.cpp` →
`apps/campaigns/constant_fitting_campaign.cpp`; their executable targets are unchanged.
The earlier source-only inventory remains
[qualification-results/starting-inventory.tsv](qualification-results/starting-inventory.tsv).
The Phase 1 action manifest and old-to-new paths are in `out/cleanup/` and copied
to `../archive/genetik/`; see [CLEANUP.md](CLEANUP.md) for completed actions/evidence.
The obsolete original main sketch remains `docs/history/main.cpp`, not an API example.

Typed discovery and fixed-structure fitting use explicit `--output out/runs/NEW_DIR`;
retained small Phase 3 reports/data live in docs/typed-regression-results/. No old
evidence path or output convention changed. See TYPED_REGRESSION.md.

The scientific campaign lives in apps/campaigns/scientific_campaign.cpp. Its
required --data and new --output directory keep generation/reporting under caller
control; recommended output is out/runs/. Independent fixtures live under
tests/fixtures/scientific; accepted new reports under docs/scientific-results/.

Phase 5 adds apps/regression_evaluate.cpp (supplied-formula evaluation, no search),
examples/ExpressionTextFile.hpp (bounded application-layer reading) and the compiled
examples/quickstarts/parsing.cpp. New parser fixtures and evidence follow tests/
and docs/parsing-results/; generated logs stay under out/logs/parsing/.

Phase 6A follows this layout: public scalar-template headers remain in `genetik/`,
compiled installed usage is `examples/quickstarts/templates.cpp`, and the bounded
campaign is `apps/campaigns/scalar_template_campaign.cpp` (target
`genetik_scalar_template_campaign`). It requires an explicit fresh `--output`
directory; use `out/runs/templates/…`. Frozen selected new evidence lives in
`docs/scalar-template-results/`; existing historical evidence is untouched.
