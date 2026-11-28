#!/usr/bin/env bash
set -euo pipefail
runner=${1:?runner binary required}
out=${2:?output directory required}
mkdir -p "$out/raw"
# Preserve each raw stdout/trace pair. analyze_traces.py owns derived CSV output,
# including the consistently named search_elapsed_seconds field.
for depths in '2 2 original' '4 2 deeper_initialization' '2 3 deeper_mutation' '4 3 library_defaults'; do
    read -r initial mutation setting <<< "$depths"
    for seed in 76001 76002 76003 76004 76005 76006 76007 76008; do
        GENETIK_TRACE_SEARCH=1 "$runner" "$seed" "$initial" "$mutation" \
            > "$out/raw/${setting}_seed${seed}.stdout" \
            2> "$out/raw/${setting}_seed${seed}.trace"
    done
done
