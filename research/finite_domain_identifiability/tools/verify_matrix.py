#!/usr/bin/env python3
"""Verify immutable per-run receipts and the hash-chained matrix ledger.

This is a post-execution integrity stage. It does not open assessment labels.
"""
import argparse,hashlib,json
from pathlib import Path
from run_matrix import Failure,sha256_file,validate,create_json,normalize_synthetic_manifest

def canonical(value): return json.dumps(value,sort_keys=True,separators=(",",":")).encode()

def verify_session_events(receipt, producer_wire, peer_wire):
    events=receipt.get("session_events")
    if not isinstance(events,list) or not receipt.get("wait4_usage_complete") or not receipt.get("stream_capture_complete"):
        return False
    if receipt.get("observed_output_bytes",1024*1024+1)>1024*1024:
        return False
    producer_lines=producer_wire.splitlines(keepends=True)
    peer_lines=peer_wire.splitlines(keepends=True)
    if len(producer_lines)!=len(peer_lines)+1 or len(events)!=len(peer_lines)+1:
        return False
    previous="0"*64
    for event_index,event in enumerate(events):
        body={key:value for key,value in event.items() if key!="sha256"}
        digest=hashlib.sha256(canonical(body)).hexdigest()
        if event.get("previous_sha256")!=previous or event.get("sha256")!=digest:
            return False
        if event.get("kind")=="exchange" and (type(event.get("ordinal")) is not int or
            event.get("ordinal")!=event_index+1):
            return False
        previous=digest
    exchanges=[event for event in events if event.get("kind")=="exchange"]
    terminal=events[-1]
    if len(exchanges)!=len(peer_lines) or terminal.get("kind")!="terminal":
        return False
    for ordinal,(event,request,response) in enumerate(zip(exchanges,producer_lines,peer_lines),start=1):
        if (type(event.get("ordinal")) is not int or event.get("ordinal")!=ordinal or
            type(event.get("request_bytes")) is not int or type(event.get("response_bytes")) is not int or
            event.get("request_bytes")!=len(request) or
            event.get("request_sha256")!=hashlib.sha256(request).hexdigest() or
            event.get("response_bytes")!=len(response) or
            event.get("response_sha256")!=hashlib.sha256(response).hexdigest()):
            return False
    final=producer_lines[-1]
    return (terminal.get("request_bytes")==len(final) and
            terminal.get("request_sha256")==hashlib.sha256(final).hexdigest())

def verify_deterministic_replays(runs, candidates):
    """Independently verify the two frozen threshold replay transcripts."""
    if len(runs) != len(candidates):
        raise Failure("replay candidate count differs from frozen run set")
    by_id = {}
    for run, candidate in zip(runs, candidates):
        run_id = run.get("run_id") if isinstance(run, dict) else None
        if not isinstance(run_id, str) or not run_id or run_id in by_id:
            raise Failure("replay run identity is missing or duplicated")
        by_id[run_id] = (run, candidate)
    replays = [(run, candidate) for run, candidate in zip(runs, candidates)
               if run.get("replay_of") is not None]
    if len(replays) != 2:
        raise Failure("frozen protocol requires exactly two deterministic replays")
    seen = set()
    for run, candidate in replays:
        predecessor = by_id.get(run.get("replay_of"))
        if predecessor is None:
            raise Failure("replay predecessor is missing")
        primary_run, primary_candidate = predecessor
        policy = run.get("method")
        if (primary_run.get("replay_of") is not None or run.get("control_id") != "threshold_m0" or
            primary_run.get("control_id") != "threshold_m0" or policy not in {"ascending", "disagreement"} or
            primary_run.get("method") != policy or run.get("seed") != 1 or primary_run.get("seed") != 0 or
            policy in seen):
            raise Failure("replay pair identity differs from frozen protocol")
        seen.add(policy)
        if any(run.get(field) != primary_run.get(field)
               for field in ("control_id", "method", "training_artifact", "training_sha256")):
            raise Failure("replay input identity differs from primary run")
        replay_transcript = candidate.get("transcript") if isinstance(candidate, dict) else None
        primary_transcript = primary_candidate.get("transcript") if isinstance(primary_candidate, dict) else None
        if not isinstance(replay_transcript, dict) or not isinstance(primary_transcript, dict):
            raise Failure("replay transcript is missing")
        if (replay_transcript.get("stop_reason") != "one_finite_behavior_class" or
            primary_transcript.get("stop_reason") != "one_finite_behavior_class"):
            raise Failure("unresolved deterministic replay cannot qualify")
        replay_normalized = dict(replay_transcript)
        primary_normalized = dict(primary_transcript)
        for transcript in (replay_normalized, primary_normalized):
            transcript.pop("run_id", None)
            transcript.pop("seed_metadata", None)
        if canonical(replay_normalized) != canonical(primary_normalized):
            raise Failure("replay transcript, decision, identity, or work differs")
    if seen != {"ascending", "disagreement"}:
        raise Failure("replay policy coverage is incomplete")

def _has_assessment_key(value):
    if isinstance(value,dict):
        return any("assessment" in str(key).lower() or _has_assessment_key(item) for key,item in value.items())
    if isinstance(value,list): return any(_has_assessment_key(item) for item in value)
    return False

def _read_json(path,label):
    try: return json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError,ValueError) as error: raise Failure(f"cannot read {label}: {error}") from error

def _resolve_indexed_file(index_path, relative_path, expected_relative):
    if not isinstance(relative_path,str) or not relative_path or Path(relative_path).is_absolute():
        raise Failure("indexed artifact path must be relative")
    rel=Path(relative_path)
    if ".." in rel.parts or rel!=expected_relative:
        raise Failure("indexed artifact path escapes or differs from the canonical run slot")
    root=Path(str(index_path)+".evidence").resolve()
    candidate=index_path.parent/rel
    # Reject symlinks in every component, including a symlinked evidence root.
    current=index_path.parent
    for component in rel.parts:
        current=current/component
        if current.is_symlink(): raise Failure("indexed artifact path contains a symlink")
    resolved=candidate.resolve(strict=True)
    if not resolved.is_relative_to(root) or not resolved.is_file():
        raise Failure("indexed artifact is outside invocation-owned evidence root")
    return resolved

def expand_candidate_index(candidate_source, runs, index_path):
    if isinstance(candidate_source,dict) and candidate_source.get("schema")=="genetik-fdi-candidate-output-index-v1":
        entries=candidate_source.get("runs")
        if not isinstance(entries,list) or len(entries)!=len(runs):
            raise Failure("candidate output index count differs from frozen run set")
        if candidate_source.get("protocol_id") is None or not isinstance(candidate_source.get("run_manifest_sha256"),str):
            raise Failure("candidate index protocol identity is incomplete")
        candidates=[]; source_parents=[]
        for ordinal,(run,entry) in enumerate(zip(runs,entries)):
            if not isinstance(entry,dict) or entry.get("run_id")!=run.get("run_id"):
                raise Failure("candidate output index is missing, reordered, or misbound")
            output_path=_resolve_indexed_file(index_path,entry.get("path"),
                Path(Path(str(index_path)+".evidence").name)/f"run-{ordinal:02d}"/"candidate.json")
            if not output_path.is_file() or sha256_file(output_path)!=entry.get("sha256"):
                raise Failure("indexed candidate output path/hash mismatch")
            output_rows=_read_json(output_path,"per-run candidate output")
            if not isinstance(output_rows,dict) or output_rows.get("run_id")!=run.get("run_id"):
                raise Failure("indexed runner output identity mismatch")
            candidates.append(output_rows)
            source_parents.append(output_path.parent)
        return candidates,source_parents
    return candidate_source,[None for _ in runs]

def expand_per_run_candidate_indexes(index_paths,runs,protocol_id):
    if not isinstance(index_paths,list) or len(index_paths)!=len(runs):
        raise Failure("one ordered candidate output index is required for each frozen run")
    candidates=[]; source_parents=[]
    for run,path_value in zip(runs,index_paths):
        one_path=Path(path_value).resolve()
        one_source=_read_json(one_path,"per-run candidate output index")
        if (not isinstance(one_source,dict) or one_source.get("protocol_id")!=protocol_id
                or one_source.get("run_manifest_sha256")!=run.get("runner_manifest_sha256")):
            raise Failure("per-run candidate index is detached from its runner identity")
        one_candidates,one_parents=expand_candidate_index(one_source,[run],one_path)
        candidates.extend(one_candidates); source_parents.extend(one_parents)
    return candidates,source_parents

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--run-manifest",required=True); ap.add_argument("--candidate-outputs")
    ap.add_argument("--candidate-output-index",nargs="+")
    ap.add_argument("--output",required=True)
    args=ap.parse_args()
    manifest_path=Path(args.run_manifest).resolve()
    candidate_path=Path(args.candidate_outputs).resolve() if args.candidate_outputs else None
    source_manifest=_read_json(manifest_path,"run manifest")
    source_manifest_sha=sha256_file(manifest_path)
    manifest=source_manifest
    if isinstance(source_manifest,dict) and source_manifest.get("schema")==1:
        manifest,_=normalize_synthetic_manifest(source_manifest)
    if bool(candidate_path)==bool(args.candidate_output_index):
        raise Failure("provide one complete candidate index or ordered per-run indexes")
    if (not isinstance(manifest,dict) or manifest.get("schema")!="genetik-fdi-run-manifest-v2" or
        manifest.get("mode") not in {"campaign","synthetic-preflight"} or _has_assessment_key(source_manifest)):
        raise Failure("run manifest schema invalid or assessment data leaked to verifier")
    runs=manifest.get("runs")
    if not isinstance(runs,list) or not runs or (manifest["mode"]=="campaign" and len(runs) not in {1,38}):
        raise Failure("campaign verifier requires one run or the complete ordered 38-run index")
    if candidate_path is not None:
        candidate_source=_read_json(candidate_path,"candidate output index")
        candidates,source_parents=expand_candidate_index(candidate_source,runs,candidate_path)
        if (not isinstance(candidate_source,dict) or candidate_source.get("protocol_id")!=manifest.get("protocol_id")
                or candidate_source.get("run_manifest_sha256")!=source_manifest_sha):
            raise Failure("candidate index is detached from the exact run manifest")
    else:
        candidates,source_parents=expand_per_run_candidate_indexes(
            args.candidate_output_index,runs,manifest.get("protocol_id"))
    if not isinstance(candidates,list) or len(candidates)!=len(runs):
        raise Failure("candidate output count differs from run manifest")
    verified=[]
    for run,candidate,source_parent in zip(runs,candidates,source_parents):
        if not isinstance(run,dict) or not isinstance(candidate,dict): raise Failure("run/candidate row malformed")
        for key in ("run_id","control_id","method","seed","training_artifact","training_sha256"):
            if candidate.get(key)!=run.get(key): raise Failure(f"candidate run binding mismatch: {key}")
        if type(run.get("seed")) is not int or run["seed"]<0:
            raise Failure("frozen seed must be a nonnegative integer")
        training_path=Path(run.get("training_path",""))
        if not training_path.is_file() or sha256_file(training_path)!=run.get("training_sha256"):
            raise Failure("training-only artifact path/SHA mismatch")
        expected_runner_manifest_sha=run.get("runner_manifest_sha256",source_manifest_sha)
        if (candidate.get("schema")!="genetik-fdi-candidate-output-v2" or
            candidate.get("protocol_id")!=manifest.get("protocol_id") or
            candidate.get("run_manifest_sha256")!=expected_runner_manifest_sha):
            raise Failure("candidate output protocol identity mismatch")
        receipt=candidate.get("receipt")
        if not isinstance(receipt,dict) or receipt.get("run_id")!=run.get("run_id"):
            raise Failure("runner receipt/run identity mismatch")
        if (receipt.get("training_sha256")!=run.get("training_sha256") or
            receipt.get("completion_status")!=candidate.get("completion_status") or
            receipt.get("report_validation_status")!=candidate.get("report_validation_status")):
            raise Failure("runner receipt does not bind output/training/status")
        raw_hashes=candidate.get("raw_stream_sha256")
        expected_raw={"producer_stdout":receipt.get("wire_stdout_sha256"),
            "peer_stdout":receipt.get("wire_responses_sha256"),
            "producer_stderr":receipt.get("stderr_sha256"),
            "peer_stderr":receipt.get("peer_stderr_sha256")}
        if (not isinstance(raw_hashes,dict) or set(raw_hashes)!=set(expected_raw) or
            raw_hashes!=expected_raw or receipt.get("raw_stream_sha256")!=expected_raw):
            raise Failure("raw stream hash receipt is incomplete or inconsistent")
        if (receipt.get("completion_status")!="completed" or receipt.get("report_validation_status")!="valid_report" or
            receipt.get("session_status")!="completed" or receipt.get("wait4_usage_complete") is not True or
            receipt.get("stream_capture_complete") is not True):
            raise Failure("incomplete execution cannot produce verified receipt")
        for field in ("cpu_seconds","wall_seconds","interactive_children_cpu_seconds"):
            value=receipt.get(field)
            if type(value) not in {int,float} or not (0 <= value < float("inf")):
                raise Failure(f"invalid resource measurement: {field}")
        for field in ("individual_child_peak_rss_sum_bytes","conservative_session_rss_sum_bytes","observed_output_bytes"):
            value=receipt.get(field)
            if type(value) is not int or value < 0: raise Failure(f"invalid resource counter: {field}")
        if (receipt["individual_child_peak_rss_sum_bytes"]>receipt["conservative_session_rss_sum_bytes"] or
            receipt["observed_output_bytes"]>1024*1024): raise Failure("resource/output cap exceeded")
        for field,cap in (("selector_charged_work",99936),("oracle_operations",64),("combined_charged_work",100000)):
            value=receipt.get(field)
            if type(value) is not int or value<0 or value>cap: raise Failure(f"work budget invalid: {field}")
        evidence_rel=candidate.get("evidence_directory")
        if not isinstance(evidence_rel,str) or Path(evidence_rel).is_absolute() or ".." in Path(evidence_rel).parts:
            raise Failure("evidence directory must be a safe relative path")
        evidence_dir=(source_parent or candidate_path.parent)/evidence_rel
        streams={}
        for filename,field in (("wire.stdout","wire_stdout_sha256"),("wire.responses","wire_responses_sha256"),
                               ("stderr.txt","stderr_sha256"),("peer.stderr.txt","peer_stderr_sha256")):
            stream_path=evidence_dir/filename
            if not stream_path.is_file() or sha256_file(stream_path)!=receipt.get(field):
                raise Failure(f"raw process evidence missing/hash mismatch: {filename}")
            streams[filename]=stream_path.read_bytes()
        if not verify_session_events(receipt,streams["wire.stdout"],streams["wire.responses"]):
            raise Failure("interactive receipt does not verify against exact protocol bytes")
        transcript=candidate.get("transcript")
        if not isinstance(transcript,dict) or transcript.get("run_id")!=run["run_id"]:
            raise Failure("candidate transcript is malformed or wrong run")
        expected_control=(run.get("peer_private",{}).get("control") if manifest["mode"]=="campaign"
                          else {item["control_id"]:item["catalogue"] for item in manifest.get("catalogue_map",[])}.get(run["control_id"]))
        if (transcript.get("control")!=expected_control or
            transcript.get("policy")!=run["method"] or transcript.get("seed_metadata")!=run["seed"]):
            raise Failure("transcript differs from frozen run specification")
        try:
            frames=[json.loads(line) for line in streams["wire.stdout"].decode("utf-8").splitlines()]
            responses=[json.loads(line) for line in streams["wire.responses"].decode("utf-8").splitlines()]
        except (ValueError,UnicodeDecodeError) as error: raise Failure("raw protocol stream malformed") from error
        queries=[frame for frame in frames if frame.get("type")=="query"]
        finals=[frame.get("transcript") for frame in frames if frame.get("type")=="final"]
        steps=transcript.get("steps")
        if (not isinstance(steps,list) or len(queries)!=len(responses) or len(finals)!=1 or finals[0]!=transcript or
            len(frames)!=len(queries)+1 or any(frame.get("type") not in {"query","final"} for frame in frames) or
            frames[-1].get("type")!="final" or frames[-1].get("id")!="final" or
            any(not isinstance(query.get("id"),str) or type(query.get("payload",{}).get("input")) is not int
                for query in queries) or
            any(response.get("type")!="response" or not isinstance(response.get("id"),str) or
                type(response.get("value")) is not int or not -(1<<63)<=response.get("value",0)<(1<<63)
                for response in responses) or
            [q.get("id") for q in queries]!=[s.get("query_id") for s in steps] or
            [q.get("payload",{}).get("input") for q in queries]!=[s.get("input") for s in steps] or
            [r.get("id") for r in responses]!=[s.get("query_id") for s in steps] or
            [r.get("value") for r in responses]!=[s.get("response",{}).get("value") for s in steps]):
            raise Failure("query/response/transcript frames are not identically ordered")
        verified.append({"schema":"genetik-fdi-verified-receipt-v2","run_id":run["run_id"],
            "control_id":run["control_id"],"method":run["method"],"seed":run["seed"],
            "candidate_output_sha256":hashlib.sha256(canonical(candidate)).hexdigest(),
            "run_manifest_sha256":expected_runner_manifest_sha,
            "verification_index_sha256":source_manifest_sha,
            "training_sha256":run["training_sha256"],
            "completion_status":candidate["completion_status"],
            "report_validation_status":candidate["report_validation_status"],
            "transcript":transcript,"execution_receipt":receipt,
            "candidate_identity":{key:candidate.get(key) for key in (
                "schema","protocol_id","run_id","control_id","method","seed",
                "training_artifact","training_sha256","completion_status","report_validation_status")},
            "verified":True})
    if manifest["mode"] == "campaign" and len(runs) == 38:
        verify_deterministic_replays(runs, candidates)
    output=Path(args.output).resolve()
    evidence_root=Path(str(output)+".evidence")
    if output.exists() or evidence_root.exists(): raise Failure("verified receipt output is create-exclusive")
    evidence_root.mkdir(parents=True,exist_ok=False)
    receipt_index=[]
    for ordinal,receipt in enumerate(verified):
        run_dir=evidence_root/f"run-{ordinal:02d}"
        run_dir.mkdir(exist_ok=False)
        receipt_path=run_dir/"receipt.json"
        receipt_bytes=(json.dumps(receipt,sort_keys=True,separators=(",",":"))+"\n").encode()
        per_run_cap=manifest.get("interactive_session",{}).get("max_protocol_bytes",1024*1024)
        if len(receipt_bytes)>per_run_cap:
            raise Failure(f"per-run verified receipt exceeds frozen byte cap: {receipt['run_id']}")
        with receipt_path.open("xb") as stream:
            stream.write(receipt_bytes); stream.flush()
        receipt_path.chmod(0o444)
        receipt_index.append({"run_id":receipt["run_id"],
            "path":str(receipt_path.relative_to(output.parent)),"sha256":sha256_file(receipt_path)})
    index={"schema":"genetik-fdi-verified-receipt-index-v1","runs":receipt_index}
    index_bytes=(json.dumps(index,sort_keys=True,separators=(",",":"))+"\n").encode()
    if len(index_bytes)>manifest.get("interactive_session",{}).get("max_protocol_bytes",1024*1024):
        raise Failure("verified receipt index exceeds frozen protocol byte cap")
    with output.open("xb") as stream:
        stream.write(index_bytes); stream.flush()

def expand_verified_index(receipt_source,runs,index_path):
    if not isinstance(receipt_source,dict) or receipt_source.get("schema")!="genetik-fdi-verified-receipt-index-v1":
        raise Failure("verified receipt index schema invalid")
    entries=receipt_source.get("runs")
    if not isinstance(entries,list) or len(entries)!=len(runs):
        raise Failure("verified receipt index count differs from frozen run set")
    result=[]
    for ordinal,(run,entry) in enumerate(zip(runs,entries)):
        if not isinstance(entry,dict) or entry.get("run_id")!=run.get("run_id"):
            raise Failure("verified receipt index is missing, reordered, or misbound")
        path=_resolve_indexed_file(index_path,entry.get("path"),
            Path(Path(str(index_path)+".evidence").name)/f"run-{ordinal:02d}"/"receipt.json")
        if sha256_file(path)!=entry.get("sha256"):
            raise Failure("verified receipt path/hash mismatch")
        value=_read_json(path,"verified per-run receipt")
        if not isinstance(value,dict) or value.get("run_id")!=run.get("run_id"):
            raise Failure("verified receipt identity mismatch")
        result.append(value)
    return result

if __name__=="__main__":
    try: main()
    except (Failure,KeyError,ValueError,OSError) as error: raise SystemExit(f"receipt verification failed: {error}")
