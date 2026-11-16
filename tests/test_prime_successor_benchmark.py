import json
import tempfile
import unittest
import types
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
bench = types.ModuleType("prime_successor_benchmark")
source = ROOT / "benchmarks/prime_successor/benchmark.py"
bench.__file__ = str(source)
exec(compile(source.read_text(encoding="utf-8"), str(source), "exec"), bench.__dict__)


class PrimeSuccessorBenchmarkTests(unittest.TestCase):
    def test_frozen_dataset_identity_and_partition_counts(self):
        rows, digest = bench.load_dataset()
        manifest = json.loads((ROOT / "benchmarks/prime_successor/prime_successor_1000_50000.manifest.json").read_text())
        self.assertEqual(digest, manifest["sha256"])
        self.assertEqual(digest, bench.FROZEN_DATASET_SHA256)
        self.assertEqual(len(rows), 4965)
        self.assertEqual(rows[0], (1009, 1013, 4))
        self.assertEqual(rows[-1], (49999, 50021, 22))
        self.assertEqual([sum(lo <= p < hi for p,_,_ in rows) for _,lo,hi in bench.PARTITIONS], [3077,958,930])
        for p,q,g in rows:
            self.assertTrue(bench.is_prime_trial(p))
            self.assertTrue(bench.is_prime_trial(q))
            self.assertEqual(q-p,g)
            self.assertFalse(any(bench.is_prime_trial(n) for n in range(p+1,q)))

    def test_generation_matches_frozen_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            generated = Path(directory) / "dataset.csv"
            result = bench.generate_dataset(generated)
            self.assertEqual(generated.read_bytes(), bench.DATA.read_bytes())
            self.assertEqual(result["sha256"], bench.load_dataset()[1])

    def test_modified_dataset_is_rejected_by_frozen_hash(self):
        with tempfile.TemporaryDirectory() as directory:
            altered=Path(directory)/"altered.csv"
            altered.write_bytes(bench.DATA.read_bytes()+b"\n")
            with self.assertRaisesRegex(ValueError,"SHA-256 mismatch"):
                bench.load_dataset(altered)

    def test_baselines_and_train_only_constant(self):
        rows,_=bench.load_dataset()
        train=[row for row in rows if row[0]<30000]
        gap=(sum(row[2] for row in train)/len(train))
        fitted=__import__("math").floor(gap+0.5)
        self.assertEqual(fitted,9)
        report=bench.score_candidate(rows,"constant_gap",constant_gap=fitted)
        self.assertEqual(report["training"]["queries"],3077)
        self.assertEqual(report["validation"]["exact_successors"],0)
        self.assertEqual(report["test"]["exact_successors"],0)
        exact=bench.score_candidate(rows,"exact")
        for split in ("training","validation","test"):
            self.assertEqual(exact[split]["exact_successors"],exact[split]["queries"])
            self.assertEqual(exact[split]["prime_output_validity"],1.0)

    def test_modal_residue_and_parity_baselines_are_train_fitted(self):
        rows,_=bench.load_dataset()
        train=[row for row in rows if 1000<=row[0]<30000]
        self.assertEqual(bench.fit_modal_gap(train),6)
        self.assertEqual(bench.fit_modal_gap([(3,5,2),(7,11,4)]),2)  # deterministic tie
        self.assertEqual(bench.parity_adjusted_log_gap(1009)%2,0)
        expected={6:226,30:299,210:359}
        for modulus,count in expected.items():
            model=bench.fit_residue_gaps(train,modulus)
            result=bench.score_candidate(rows,"residue_gap",residue_modulus=modulus,residue_gaps=model)
            self.assertEqual(result["test"]["exact_successors"],count)
            self.assertEqual(result["expression_complexity_nodes"]["stored_parameters"],len(model))
        self.assertTrue(all(bench.parity_adjusted_log_gap(p)%2==0 for p,_,_ in rows if p>=40000))

    def test_boundary_invalid_prediction_and_exact_gap_scoring(self):
        rows,_=bench.load_dataset()
        self.assertEqual([row[0] for row in rows if row[0] in (30000,40000,50000)],[])
        self.assertEqual(next(p for p,_,_ in rows if p>=30000),30011)
        self.assertEqual(next(p for p,_,_ in rows if p>=40000),40009)
        for partition,lo,hi in bench.PARTITIONS:
            subset=[row for row in rows if lo<=row[0]<hi]
            self.assertEqual(len(subset),{"training":3077,"validation":958,"test":930}[partition])
            self.assertTrue(all(lo<=row[0]<hi for row in subset))
        bad,_,_,_=bench.predict([(1009,1013,4)],"predictive","p + 100001")
        self.assertFalse(isinstance(bad[0],int))
        valid,_,_,_=bench.predict([(1009,1013,4)],"predictive","p + 4")
        scored=bench.metrics([(1009,1013,4)],valid,[1],0)
        self.assertEqual(scored["exact_successor_accuracy"],1.0)
        self.assertEqual(scored["exact_gap_accuracy"],1.0)

    def test_candidate_freeze_and_track_separation(self):
        import subprocess
        import sys
        with tempfile.TemporaryDirectory() as directory:
            frozen=Path(directory)/"candidate.json"
            command=[sys.executable,"-B",str(ROOT/"benchmarks/prime_successor/benchmark.py"),
                     "--track","predictive","--freeze-candidate","p + 2","--output",str(frozen)]
            subprocess.run(command,check=True,capture_output=True,text=True)
            scored=subprocess.run([sys.executable,"-B",str(ROOT/"benchmarks/prime_successor/benchmark.py"),
                "--track","predictive","--frozen-candidate",str(frozen),"--exclude-test"],check=True,capture_output=True,text=True)
            result=json.loads(scored.stdout)
            self.assertNotIn("test",result["candidate"]["metrics"])
            wrong_track=subprocess.run([sys.executable,"-B",str(ROOT/"benchmarks/prime_successor/benchmark.py"),
                "--track","algorithmic","--frozen-candidate",str(frozen)],capture_output=True,text=True)
            self.assertNotEqual(wrong_track.returncode,0)

    def test_predictive_language_excludes_oracles_and_iteration(self):
        tree,nodes=bench.validate_expr("p + 2 * log(p)")
        self.assertGreater(nodes,0)
        for source in ("next_prime(p)", "prime_lookup(p)", "p if p > 2 else 3", "__import__('os')"):
            with self.assertRaises((ValueError, SyntaxError)):
                bench.validate_expr(source)

    def test_algorithmic_bounded_track_charges_divisibility_work(self):
        kind,bounds=bench.parse_algorithmic("bounded_trial_scan(max_gap=128,max_divisor=250)")
        self.assertEqual(kind,"bounded_scan")
        report=bench.score_candidate(bench.load_dataset()[0],kind,algorithmic_limits=bounds)
        for split in ("training","validation","test"):
            self.assertEqual(report[split]["exact_successors"],report[split]["queries"])
            self.assertGreater(report[split]["mathematical_work_per_query"],0)
        with self.assertRaises(ValueError):
            bench.parse_algorithmic("bounded_trial_scan(max_gap=0,max_divisor=250)")


if __name__ == "__main__":
    unittest.main()
