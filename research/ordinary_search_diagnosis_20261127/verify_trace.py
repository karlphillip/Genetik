#!/usr/bin/env python3
"""Offline integrity checks for the saved ordinary-search diagnostic trace."""
import csv
from collections import Counter
from pathlib import Path

rows = list(csv.DictReader(Path(__file__).with_name("candidate_trace.csv").open()))
assert len(rows) == 100
for arm, calls, selectors in (("ordinary_lm", 294, 0), ("rank_aware_lm", 419, 255)):
    selected = [r for r in rows if r["arm"] == arm]
    assert len(selected) == 50
    assert len({r["before"] for r in selected}) == 32
    assert sum(int(r["objective_calls"]) for r in selected) == calls
    assert sum(int(r["selector_evaluations"]) for r in selected) == selectors
    assert all(r["published"] == "yes" for r in selected)
    assert Counter(r["origin"] for r in selected) == {
        "initialization": 10,
        "mutation": 17,
        "crossover": 13,
        "unchanged offspring": 6,
        "elite reevaluation": 4,
    }
    best = [r for r in selected if r["generation"] == "4" and r["best_seen_after_generation"] == "yes"]
    assert len(best) == 1
    assert best[0]["after"] == "(x0 + 2.9999999999292117)"
    assert abs(float(best[0]["training_mse"]) - 2.0) < 1e-8
print("candidate trace rows, arms, source counts, work, and final selections: OK")
