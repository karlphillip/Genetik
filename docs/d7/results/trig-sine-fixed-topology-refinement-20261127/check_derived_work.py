#!/usr/bin/env python3
"""Offline consistency checks for the additive derived-work correction."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
EXPECTED_OCCURRENCES = {0: 5, 1: 9}
EXPECTED_ROWS = 160
rows = [json.loads(line) for line in (ROOT / "comparison.jsonl").read_text().splitlines()]
assert len(rows) == 8
for row in rows:
    seed = row["seed"]
    assert seed in EXPECTED_OCCURRENCES
    original = EXPECTED_OCCURRENCES[seed]
    assert row["original_constant_occurrences"] == original
    expected_predictions = 2 * EXPECTED_ROWS * original if row["method"] == "lm_rank_subset" else 0
    assert row["selection_prediction_evaluations"] == expected_predictions
    if row["method"] == "lm_rank_subset":
        assert int(row["lm_forward_evaluations"]) == 3520
    assert "selection_expression_evaluation_work" in row
    assert "finite-difference quotient arithmetic and pivoted rank-selection arithmetic are not counted" in row["selection_work_note"]
    if row["method"] == "lm_rank_subset":
        # This explicit inequality catches the old error of multiplying rows by
        # the three selected directions instead of all original occurrences.
        assert expected_predictions != 2 * EXPECTED_ROWS * row["selected_parameters"]
        assert row["selection_prediction_evaluations"] > 0
print("derived selection-work consistency: PASS (seed 0: 1600; seed 1: 2880)")
