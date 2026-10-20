Full suites plus affected final reruns; source identities in SOURCE_SHA256.json.
Full local: Release/Debug/ASan+UBSan/CMake3.16 each33. Portable manifest32 excludes only qualified captured Jetson snapshot. Separate TSan4 selected cases, each3 repetitions.
Initial ASan timeout preserved, not counted as passing. No sanitizer instrumentation was disabled for qualification.
GDB stack is diagnostic only: inferior ran with detect_leaks=0 for debugger compatibility, was interrupted and terminated; this was not qualification. Actual qualification uses default sanitizer options, including stack-use-after-return and leak checking. No ptrace policy was changed.
Original 90s combined typed-contract timeout increased to300s after bounded-work isolation; full case passes in110.48s, latest source forced rebuild109.41s. CLI limit180s, full run49.24s. No fixtures or work were removed.
Shape-incompatible grammar combinations are now filtered using the validator's shared shape rules before construction; no arithmetic/RNG/configuration tuning. Typed replay is exact except timing and new report fields.
All complete logs/builds remain under out/logs/typed-regression and out/build on this checkout. New hosted CI is configured, not executed. No remote actions.
CTest JSON manifests are canonically formatted structured copies; raw emitted JSON remains under out/logs/typed-regression. Test selections/properties are unchanged.
Retained manifests are refreshed from the final configured builds after qualification, including the300s bounded combined-case allowance; original preflight emissions remain in out/logs.
