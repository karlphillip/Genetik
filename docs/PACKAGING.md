# CMake package and source inclusion

`find_package(Genetik 0.1 CONFIG REQUIRED)` provides `Genetik::Genetik`. This is the
same canonical target available through add_subdirectory. The old unnamespaced
`genetik_lib` remains a source-build compatibility name, not an installed alias.
Enable CXX before requesting the package. The public platform dependency is Threads::Threads, discovered by the package
with find_dependency(Threads) before exports load. No manual thread flags or
third-party mathematical dependencies are needed.

The library is entirely header-only: INTERFACE include roots and cxx_std_20 allow
consumers to select newer standards. Build includes use the genetik directory;
installed includes use `<prefix>/<includedir>/Genetik`. Public include spelling
remains `<Expression.hpp>`, `<SymbolicRegression.hpp>`, etc. All 16 public headers
and detail/ScopedTiming.hpp are installed. Do not rely on detail names as public APIs.

Default install layout (GNUInstallDirs may choose another libdir):

```
include/Genetik/*.hpp
include/Genetik/detail/ScopedTiming.hpp
lib/cmake/Genetik/GenetikTargets.cmake
lib/cmake/Genetik/GenetikConfig.cmake
lib/cmake/Genetik/GenetikConfigVersion.cmake
lib/cmake/Genetik/GenetikFloatingPoint.cmake
share/licenses/Genetik/LICENSE
```

No tests, tools, campaign reports, datasets or binaries are installed. Standard
GNUInstallDirs **relative** destinations support relocation; overriding destinations
with paths outside the prefix forfeits that property. No absolute producer include
paths, machine-local CMake paths or development flags are exported. Config/version
files use CMakePackageConfigHelpers; the architecture-independent version file
matches within the same 0.x minor series, not across all major-zero versions.
There are no package components; unknown REQUIRED components fail with a diagnostic.

## Floating-point usage requirements

Strict numerical options are required by the header implementation. The source
interface uses BUILD_INTERFACE options; installed Config imports the target and
selects options using the **consuming** compiler/frontend, never the producer's
compiler choice. Both paths call the same small helper, with COMPILE_LANGUAGE:CXX
conditions. GNU/Clang/AppleClang GNU-style frontends select -fno-fast-math and
-ffp-contract=off; MSVC-style frontends select /fp:strict. The latter is selection
logic, not a Windows qualification claim. Unknown compilers warn that equivalent
settings and qualification are required. These conditions use CMake 3.16 facilities;
newer frontend generator expressions are unnecessary.

These options affect every C++ translation unit linking the header-only target.
They do not apply to unrelated C compilation. Preserve signed-zero, finite-result
and intermediate-node semantics; do not add unsafe reciprocal/reassociation/FMA
options. Runtime floating-point requirements remain in EXPRESSIONS.md and
STRICT_ARITHMETIC.md. Header diagnostics reject known unsafe builds. No sanitizer,
warning, profiling or coverage option is an installed usage requirement.

## Development options and parent behavior

| Option | Top-level default | Embedded default | Meaning |
| --- | --- | --- | --- |
| GENETIK_BUILD_TESTS | BUILD_TESTING | OFF | Bounded correctness/integration checks; Python 3.9+ required. |
| GENETIK_BUILD_EXAMPLES | ON | OFF | Examples and opt-in campaign/measurement executables. No full campaign runs in CTest. |
| GENETIK_INSTALL | ON | OFF | Development-package installation/export rules. |
| GENETIK_SANITIZERS | OFF | OFF | ASan+UBSan on this directory's development targets only. |
| GENETIK_THREAD_SANITIZER | OFF | OFF | Separate development-only TSan build; mutually exclusive with ASan. |
| GENETIK_WARNINGS_AS_ERRORS | OFF | OFF | Werror on qualified development compilers. |
| GENETIK_RECORDED_REFERENCE | OFF | OFF | Opt-in captured Jetson/GCC13/libm2.39 bit reference. |

BUILD_TESTING=OFF prevents tests. Genetik includes CTest only as the top-level
project; it never forces a parent's BUILD_TESTING value or enables parent testing.
An embedding parent that explicitly opts into GENETIK_BUILD_TESTS owns its CTest/
enable_testing setup. Existing explicit cache choices take priority over defaults.
Warnings are enabled for Genetik's own GNU-compatible development targets and
can become errors; none change sibling targets or public interface requirements.
C++ extensions/standard, compiler and build type are not overridden.

## Integration proof

The CTest `genetik_installed_package` runs tests/package_integration.py. It copies
current intended library/CMake sources (including uncommitted intended edits) to a
disposable source directory. It installs with producer sanitizer/Werror options ON
and development extras OFF, verifies normal consumption, moves prefix A to prefix B,
deletes the disposable producer source/build, and builds/runs a fresh consumer.
The active checkout remains available and untouched. Paths contain spaces.

Package registries are disabled and the consumer searches only its explicit prefix;
resolved Genetik_DIR must be inside B. Installed metadata and compile commands are
inspected for source/build/development leaks. Two overlapping public-header C++
translation units plus C verify ODR/linkage, target-provided C++20 and language-specific
FP options. Both actual quickstart sources compile/run as separate installed targets.
Version requests, unsupported required components, synthetic frontend-selection
logic and a C++23 consumer (with newer CMake) are checked. A fresh parent fixture
checks default-off extras, the canonical/legacy source target and an unrelated
C++14 sibling. Its global settings and instrumentation remain untouched.

Reproduce only the fixture without running the complete suite:

```sh
python3 tests/package_integration.py --source . --workspace ../delivery-check/genetik-delivery-fixture --cmake "$(command -v cmake)" --cxx "$(command -v g++)"
```

The fixture is Linux GNU-frontend test tooling, not qualification of every frontend
for which the package can select options. CMake 3.16 is the retained minimum; exact
3.16.0 and workspace 4.4.4 have been exercised locally. Public instructions use
`cmake -E chdir ... ctest` because CTest 3.16 predates --test-dir.

## Generic extension qualification

The isolated fixture also installs/builds/runs tests/oop_consumer as an external
permutation application. Compiler-emitted dependencies using its actual usage flags
verify no symbolic headers are needed. This probe works with the exact CMake 3.16.0
Makefile generator, which need not retain .o.d files. The quickstart source location
is now examples/quickstarts; installed header/package layout and target names do
not change. See OOP_QUALIFICATION.md for repeated final-tree delivery evidence.

GENETIK_THREAD_SANITIZER is a development-only TSan option, OFF by default and
mutually exclusive with GENETIK_SANITIZERS. Its flags are private. Execution.hpp
is installed alongside the existing public headers. The library remains header-only
and version matching remains architecture-independent. See CONCURRENCY_RESULTS.md
for repeated minimum-CMake/relocation/multi-TU consumption with parallel work.


## Source distribution

The planned first development release ships a Git source archive, not platform
binaries. It includes the MIT license, public/transitive headers, CMake package
helpers, compiled quickstarts, external OOP consumer and accepted evidence. Ignored
build/output trees and .git are absent. From an extracted source root, the same
README install/consumer commands work; no Git metadata or Jetson tooling is needed.
Only archive generation needs Git; verification tooling additionally needs Python
3.9+ and the selected supported C++ compiler/CMake.

Maintainer verification uses `scripts/check_source_archive.py`; exact candidate,
asset checksum and commands are recorded in the release-review output, not baked
into a document that would change its own candidate hash. See
[the release proposal](RELEASE_PROPOSAL.md).
