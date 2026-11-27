#!/usr/bin/env bash
set -euo pipefail
runner=${1:?runner binary required}
out=${2:?output directory required}
mkdir -p "$out/raw"
printf 'setting,seed,initial_depth,mutation_depth,candidate_evaluations,train_mse,heldout_mse,numerical_recovery,objective_calls,fitting_loss_calls,lm_math_ops,lm_linear_ops,lm_objective_evals,lm_jacobian_evals,rank_selector_evals,search_cpu_seconds,stop,equation\n' > "$out/results.csv"
for depths in '2 2 original' '4 2 deeper_initialization' '2 3 deeper_mutation' '4 3 library_defaults'; do
    read -r initial mutation setting <<< "$depths"
    for seed in 76001 76002 76003 76004 76005 76006 76007 76008; do
        GENETIK_TRACE_SEARCH=1 "$runner" "$seed" "$initial" "$mutation" \
            > "$out/raw/${setting}_seed${seed}.stdout" \
            2> "$out/raw/${setting}_seed${seed}.trace"
        line=$(cat "$out/raw/${setting}_seed${seed}.stdout")
        printf '%s,%s\n' "$setting" "$line" >> "$out/results.csv"
    done
done
