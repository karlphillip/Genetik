# Scientific benchmark preflight

Genetik's optional development tooling can validate a declared benchmark before
starting an expensive campaign. It is independent of the genetic algorithm and
does not add reference expressions to a population. The C++ validator uses the
same expression parser, limits, and evaluator as Genetik; the Python manifest
layer checks campaign and fitting configuration and gates the campaign runner.

## What it validates

For each declared task, the validator checks expression syntax and postfix
validity, enabled operators, variable dimension and bindings, node/depth and
arithmetic-operation limits, finite constant and fitting bounds, integer and
real-power grammar restrictions, and mathematical validity at every declared
observation. The manifest layer validates fitting-method settings, consistency
of parameter bounds, and positive bounded campaign resources including worker
count, CPU affinity, wall time, resident memory, and virtual address space.
It emits the exact parsed expression structure and a SHA256 identity of the
declared grammar fields.

The result answers whether the supplied reference representation fits the
declared grammar and declared observations. If it does not fit, preflight rejects
it precisely. It does not prove that every equivalent expression also violates
the grammar, and it does not prove symbolic equivalence between different
representations.

The S11.1 benchmark review caught an invalid frozen task whose supplied
reference required 12 nodes while its grammar allowed 10. Its historical runs
remain excluded from valid recovery claims. The regression suite preserves that
12-node-versus-10-node rejection. It also ensures schema-only errors fail the
overall gate before any campaign process starts.

## Build and use

The tool is built only when examples/development tools are enabled. It is not
installed with the Genetik library and needs no research-data directory.

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DGENETIK_BUILD_EXAMPLES=ON
cmake --build build/release --target genetik_scientific_benchmark_preflight
```

The manifest is standalone JSON. Observation file paths are relative to the
manifest's directory. The tool does not load campaign data implicitly.

```sh
python3 scripts/scientific_benchmark_preflight.py \
  --validator build/release/genetik_scientific_benchmark_preflight \
  --manifest /path/to/frozen-protocol/preflight.json \
  --output /path/to/run/preflight-results.json
```

To make validation a hard gate, use the wrapper:

```sh
python3 scripts/run_validated_campaign.py \
  --validator build/release/genetik_scientific_benchmark_preflight \
  --manifest /path/to/frozen-protocol/preflight.json \
  --preflight scripts/scientific_benchmark_preflight.py \
  --preflight-output /path/to/run/preflight-results.json \
  --runner /path/to/campaign-runner.py -- --protocol /path/to/protocol.json
```

Any invalid task or campaign/fitting configuration returns a nonzero status and
prevents the runner from starting. Keep the manifest, datasets, and protocol
checksums with the experiment. The preflight's grammar identity is not a
substitute for hashing the manifest, data, source, compiler, or runtime.

## Compatibility

The tool is development/testing infrastructure, not a public library API. It
does not modify GA initialization, fitness, variation, selection, RNG state,
checkpoint identity or format, installed consumers, or the v0.7.0 package.
The C++ validator depends only on Genetik's standard-library-only target; the
manifest and campaign wrappers use Python's standard library.
