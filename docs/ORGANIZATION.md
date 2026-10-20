# Source and output organization

The starting checkpoint 8b633264 was clean. Before moving sources we inventoried
tracked/untracked files; there was no unrelated local work. Public headers and all
accepted campaign reports, raw logs, prediction tables and reproduction evidence
are preserved. The root held nine active demonstration/application/harness .cpp
files plus one obsolete unbuilt main.cpp sketch. The sketch calls nonexistent
Settings/GeneticAlgorithm::run APIs and is retained as historical source, not a
supported example or defect reproduction. No compiled library src directory is
needed: Genetik is header-only.

| Purpose | Location |
| --- | --- |
| Public headers and their detail include | genetik/ (installation remains include/Genetik) |
| Small runnable demonstrations and application support headers | examples/ |
| Installed-package compiled quickstarts | examples/quickstarts/ |
| Generic numerical-file search executable | apps/regression_search.cpp |
| Contract tests, downstream and installed consumers | tests/ |
| Evaluation performance/replay harness and corpus | benchmarks/ |
| Campaign runners and Python development/report helpers | scripts/ |
| CMake package helpers | cmake/ |
| Public contracts and immutable accepted experiment evidence | docs/ |
| Obsolete unbuilt original main sketch | docs/history/main.cpp |
| New generated output | out/ (ignored) or explicit external/build directories |

The prime generator/search remains examples/prime_regression.cpp: it is an example
application with explicit --output, not part of the reusable core. Existing
executable names, command-line options and public includes are unchanged. Only
source paths changed. Source filenames referenced in historical campaign reports,
raw evidence and reproduction records describe those historical checkouts; those
files intentionally remain unchanged. For current compilation use CMake targets.
Manual historical compilation commands map root *.cpp into the locations above;
Python helper paths change from examples/ to scripts/.

All tests pass temporary output paths under their build directory, including file
fixtures and disposable installation/relocation producers. The package integration
fixture copies only intended header/package sources and tested consumers. It does
not package application or evidence artifacts. Quickstart/permutation programs
write stdout only. Regression file search writes stdout; explicit evaluation files
are inputs. Prime/cadence campaigns require --output DIR; multivariate generation
uses an explicit output path. Other small demos/campaigns and replay tools write
stdout/stderr: redirect them explicitly. Avoid overwriting accepted docs results.

Recommended new output destinations:

```sh
mkdir -p out/prime-new
../build-release/genetik_prime_regression --generate --output out/prime-new
../build-release/genetik_demo 42 20 >out/character.txt
```

out/, build directories, and Python bytecode caches are narrowly ignored. We do not
ignore every .log or .csv: accepted evidence stays visible and new misplaced output
should remain detectable. Executing a routine build/test must not add files at the
source root. Use the existing external ../build-* workspaces on the Jetson.

[The file-by-file checkpoint inventory](qualification-results/starting-inventory.tsv)
classifies every tracked starting file and records its current path. Initial Git
status was clean (no untracked files); newly authored qualification files are
separate from that historical inventory. Historical evidence locations are retained
intentionally, not mistaken for new generated output.
