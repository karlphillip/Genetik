# S2 evidence

- `qualification.json` records the final targeted engineering and mathematical outcomes.
- `release-ctest-excluding-package.log` preserves the 72-test Release rerun after the one-time 479-second installed/relocated package test. The complete Release run passed all 73 tests, including that package test.
- `sha256sums.txt` checksums the S2 source additions, the symbolic regression fixture, result documents, and retained CTest log.

The Boost header tree used for the optional exact-rational build was the previously retained `out/tmp/m1-boost/root/usr/include` tree; it is not copied into S2 evidence and is not a product dependency.
