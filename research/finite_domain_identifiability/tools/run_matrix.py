#!/usr/bin/env python3
"""Create-exclusive bounded executor for a frozen finite-domain run matrix.

Default mode is preflight only. Execution requires --execute and a caller
supplied frozen configuration SHA. The child selector receives training data
and oracle responses, never target identity or assessment labels.
"""
from __future__ import annotations
import argparse, base64, fcntl, hashlib, json, os, subprocess, time
from pathlib import Path
import sys

_repo_root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(_repo_root / "scripts"))
from scientific_pipeline_preflight import verify_admission, PreflightError
from bounded_interactive_session import run_session

PER_RUN = {"cpu_seconds": 2.0, "wall_seconds": 3.0,
           "address_space_bytes": 512 * 1024 * 1024, "output_bytes": 1024 * 1024}
TOTAL = {"runs": 38, "cumulative_cpu_seconds": 76.0, "cumulative_wall_seconds": 114.0}
WORK_LIMIT = 100000
ORACLE_WORK_RESERVE = 64
SELECTOR_WORK_LIMIT = WORK_LIMIT - ORACLE_WORK_RESERVE
RESERVATION_ROOT = Path(os.environ.get(
    "GENETIK_RESEARCH_LEDGER_ROOT", "~/.local/state/genetik/research-program-ledgers"
)).expanduser().resolve()

class Failure(RuntimeError): pass

def sha256_bytes(data): return hashlib.sha256(data).hexdigest()
def sha256_file(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as f:
        for block in iter(lambda:f.read(1024*1024),b""): h.update(block)
    return h.hexdigest()
def create_json(path, value):
    path=Path(path); path.parent.mkdir(parents=True,exist_ok=True)
    with path.open("x",encoding="utf-8",newline="\n") as f:
        json.dump(value,f,sort_keys=True,separators=(",",":")); f.write("\n"); f.flush(); os.fsync(f.fileno())

def validate_interactive_session(session):
    required={"python_executable","python_sha256","helper_path","helper_sha256",
        "peer_path","peer_sha256","runner_runtime_files","peer_runtime_files",
        "max_frame_bytes","max_terminal_frame_bytes","max_protocol_bytes","max_requests","max_cpu_seconds",
        "max_memory_bytes","terminal_type"}
    if not isinstance(session,dict) or set(session)!=required:
        raise Failure("interactive session identity/limits are incomplete")
    if (type(session["max_frame_bytes"]) is not int or not 0 < session["max_frame_bytes"] <= 4096 or
        type(session["max_terminal_frame_bytes"]) is not int or session["max_terminal_frame_bytes"] < session["max_frame_bytes"] or
        type(session["max_protocol_bytes"]) is not int or not session["max_terminal_frame_bytes"] <= session["max_protocol_bytes"] <= 1024*1024 or
        session["max_requests"]!=8 or session["max_cpu_seconds"]!=2.0 or
        session["max_memory_bytes"]!=512*1024*1024 or session["terminal_type"]!="final"):
        raise Failure("interactive session limits differ from frozen caps")
    python=Path(session["python_executable"]).resolve()
    helper=Path(session["helper_path"]).resolve()
    peer=Path(session["peer_path"]).resolve()
    expected_helper=(_repo_root/"scripts"/"bounded_interactive_session.py").resolve()
    expected_peer=Path(__file__).with_name("oracle_peer.py").resolve()
    if helper!=expected_helper or peer!=expected_peer:
        raise Failure("interactive helper/peer path differs from the study implementation")
    if (not python.is_file() or not os.access(python,os.X_OK) or
        sha256_file(python)!=session["python_sha256"] or
        not helper.is_file() or sha256_file(helper)!=session["helper_sha256"] or
        not peer.is_file() or sha256_file(peer)!=session["peer_sha256"]):
        raise Failure("interactive executable/helper/peer hash mismatch")
    def verify_runtime_files(items,label):
        if not isinstance(items,list): raise Failure(f"{label} runtime files must be a list")
        paths=set()
        for item in items:
            if not isinstance(item,dict) or set(item)!={"path","sha256"}: raise Failure(f"{label} runtime file identity malformed")
            path=Path(item["path"]).resolve()
            if path in paths or not path.is_file() or sha256_file(path)!=item["sha256"]:
                raise Failure(f"{label} runtime file missing, duplicate, or hash-mismatched")
            paths.add(path)
    verify_runtime_files(session["runner_runtime_files"],"runner")
    verify_runtime_files(session["peer_runtime_files"],"peer")
    return python

def producer_command(runner,run,training_snapshot):
    """Build only the selector-visible fields; reference and assessment stay outside."""
    return [str(runner),"--run-id",run["run_id"],"--catalogue",run["control"],
        "--policy",run["policy"],"--seed-metadata",str(run["seed_metadata"]),
        "--training-file",str(training_snapshot)]

def peer_command(session,run):
    private=run.get("peer_private",run)
    control=private.get("control",run.get("control"))
    member=private.get("reference_member")
    return [str(Path(session["python_executable"]).resolve()),"-I",str(Path(session["peer_path"]).resolve()),
        "--control",control,"--reference-member",str(member)]

def validate(config, runner, manifest, check_assessment=False):
    try: verify_admission(config)
    except PreflightError as error: raise Failure(f"scientific admission preflight failed: {error}") from error
    if config.get("protocol_id")!="finite-domain-identifiability-v1": raise Failure("protocol ID mismatch")
    expected_limits={**PER_RUN,**TOTAL,"charged_work_per_run":WORK_LIMIT,
                     "cumulative_charged_work":WORK_LIMIT*38,
                     "selector_work_limit":SELECTOR_WORK_LIMIT,
                     "oracle_work_reserve":ORACLE_WORK_RESERVE,
                     "candidate_row_evaluations_per_run":1024,
                     "logical_storage_bytes_per_run":1024*1024}
    if config.get("limits")!=expected_limits: raise Failure("resource limits mismatch")
    runs=config.get("runs")
    if not isinstance(runs,list) or len(runs)!=38: raise Failure("allowlist must contain exactly 38 runs")
    ids=[r.get("run_id") for r in runs]
    if len(set(ids))!=38 or ids!=[f"run_{i+1:03d}" for i in range(38)]:
        raise Failure("opaque ordered run IDs invalid")
    if sum(r.get("replay_of") is None for r in runs)!=36: raise Failure("expected 36 primary runs and two replays")
    if sha256_file(runner)!=config.get("runner_sha256"): raise Failure("runner binary hash mismatch")
    session=config.get("interactive_session")
    validate_interactive_session(session)
    identities=config.get("implementation_files")
    if not isinstance(identities,list) or not identities: raise Failure("implementation sources must be pinned")
    seen=set()
    for identity in identities:
        path=Path(identity.get("path", "")).resolve()
        if path in seen or not path.is_file() or sha256_file(path)!=identity.get("sha256"):
            raise Failure("implementation source identity missing, duplicate, or hash-mismatched")
        seen.add(path)
    required_sources={
        Path(__file__).resolve(),
        Path(__file__).resolve().parents[3]/"scripts"/"bounded_interactive_session.py",
        Path(__file__).with_name("oracle_peer.py").resolve(),
        Path(__file__).with_name("freeze_data.py").resolve(),
        Path(__file__).with_name("score_transcripts.py").resolve(),
        Path(__file__).with_name("verify_matrix.py").resolve(),
        (_repo_root/"research/finite_domain_identifiability/runner.cpp").resolve(),
        (_repo_root/"research/finite_domain_identifiability/FiniteDomainIdentifiability.hpp").resolve(),
    }
    if not required_sources.issubset(seen): raise Failure("protocol does not pin all implementation sources")
    protocol_runs=config.get("run_matrix")
    if not isinstance(protocol_runs,list) or len(protocol_runs)!=38:
        raise Failure("preflight-bound protocol run_matrix is required")
    for execution, frozen in zip(runs,protocol_runs):
        expected_control_id=f"{execution['control']}_m{execution['reference_member']}"
        if (frozen.get("run_id")!=execution["run_id"] or
            frozen.get("control_id")!=expected_control_id or
            frozen.get("method")!=execution["policy"] or frozen.get("seed")!=execution["seed_metadata"]):
            raise Failure(f"execution record differs from preflight-bound run matrix: {execution['run_id']}")
    manifest_runs=manifest.get("runs")
    if not isinstance(manifest_runs,list) or len(manifest_runs)!=38:
        raise Failure("training/assessment mapping must contain the complete 38-run matrix")
    data={r["run_id"]:r for r in manifest_runs}
    if len(data)!=38:
        raise Failure("duplicate run identity in training/assessment mapping")
    if set(data)!=set(ids): raise Failure("training manifest does not match run allowlist")
    for r in runs:
        d=data[r["run_id"]]
        for k in ("run_id","control","reference_member","policy","seed_metadata","replay_of"):
            if r.get(k)!=d.get(k): raise Failure(f"data manifest mismatch at {r['run_id']}:{k}")
        if r.get("training_sha256")!=d.get("training_sha256") or sha256_file(d["training_path"])!=d["training_sha256"]:
            raise Failure(f"training data hash mismatch at {r['run_id']}")
        assessment=Path(d["assessment_path"])
        if not assessment.is_absolute() or not isinstance(d.get("assessment_sha256"),str) or len(d["assessment_sha256"])!=64:
            raise Failure(f"invalid assessment artifact identity at {r['run_id']}")
        # Assessment content is not opened or hashed before candidate decisions
        # are sealed. Only its declared identity/shape is validated here.
    return data

def oracle_operation_count(record,n):
    c,m=record["control"],record["reference_member"]
    if c=="arithmetic": return [1,2,2,0][m]
    if c=="modular": return [1,1,2,1][m]
    if c=="threshold":
        ops=[2 if n<0 else 1,0,2 if n < -1 else 1,3 if n<0 else 1]
        return ops[m]
    if c=="conditional-modular":
        pred=n%3==1
        return ([2+(1 if pred else 2)]*3+[2])[m]
    if c=="ambiguity": return 0
    if c=="outside": return int(n==0)
    raise Failure("unknown control")

def decode_json_lines(encoded, label):
    try:
        raw=base64.b64decode(encoded,validate=True)
        return raw,[json.loads(line) for line in raw.decode("utf-8").splitlines()]
    except (ValueError,UnicodeDecodeError) as error:
        raise Failure(f"malformed {label} byte evidence") from error

def persist_then_parse_streams(evidence, outdir):
    """Persist exact process streams before decoding/parsing any frame."""
    try:
        streams={"wire.stdout":base64.b64decode(evidence.get("producer_stdout",{}).get("base64",""),validate=True),
            "wire.responses":base64.b64decode(evidence.get("peer_stdout",{}).get("base64",""),validate=True),
            "stderr.txt":base64.b64decode(evidence.get("producer_stderr",{}).get("base64",""),validate=True),
            "peer.stderr.txt":base64.b64decode(evidence.get("peer_stderr",{}).get("base64",""),validate=True)}
    except (ValueError,TypeError) as error:
        raise Failure("invalid base64 process stream evidence") from error
    root=Path(outdir)
    for name,raw in streams.items(): (root/name).write_bytes(raw)
    try:
        frames=[json.loads(line) for line in streams["wire.stdout"].decode("utf-8").splitlines()]
        responses=[json.loads(line) for line in streams["wire.responses"].decode("utf-8").splitlines()]
        diagnostic=None
    except (ValueError,UnicodeDecodeError) as error:
        frames=[]; responses=[]; diagnostic=f"malformed raw protocol stream: {error}"
    return streams,frames,responses,diagnostic

def execute_one(command,outdir,record,initial_oracle_operations,config,runner,peer_record=None):
    session=config["interactive_session"]
    peer=Path(session["peer_path"]).resolve()
    python=Path(session["python_executable"]).resolve()
    response_command=peer_command(session,peer_record or record)
    request_identity={"executable":str(Path(runner).resolve()),"sha256":sha256_file(runner),
                      "runtime_files":session["runner_runtime_files"]}
    response_identity={"executable":str(python),"sha256":session["python_sha256"],
                       "runtime_files":[{"path":str(peer),"sha256":session["peer_sha256"]},
                                        *session["peer_runtime_files"]]}
    started=time.monotonic()
    result=run_session(command,response_command,cwd=Path(outdir),
        environment={"PATH":os.environ.get("PATH","/usr/bin:/bin"),"LC_ALL":"C",
                     "PYTHONDONTWRITEBYTECODE":"1"},
        wall_seconds=PER_RUN["wall_seconds"],max_frame_bytes=session["max_frame_bytes"],
        max_terminal_frame_bytes=session["max_terminal_frame_bytes"],
        max_protocol_bytes=session["max_protocol_bytes"],max_requests=session["max_requests"],
        request_identity=request_identity,response_identity=response_identity,
        max_cpu_seconds=session["max_cpu_seconds"],max_memory_bytes=session["max_memory_bytes"],
        terminal_type=session["terminal_type"])
    evidence=result.get("byte_evidence",{})
    streams,producer_frames,peer_frames,parse_error=persist_then_parse_streams(evidence,outdir)
    producer_bytes=streams["wire.stdout"]; peer_bytes=streams["wire.responses"]
    stderr=streams["stderr.txt"]; peer_stderr=streams["peer.stderr.txt"]
    if parse_error:
        result={**result,"status":"invalid_protocol","diagnostic":parse_error}
    finals=[item for item in producer_frames if item.get("type")=="final"]
    queries=[item for item in producer_frames if item.get("type")=="query"]
    if len(finals)>1 or (result.get("status")=="completed" and len(finals)!=1):
        result={**result,"status":"invalid_protocol","diagnostic":"missing or duplicate final frame"}
    report=finals[0].get("transcript") if finals else None
    if report is not None and not isinstance(report,dict):
        result={**result,"status":"invalid_protocol","diagnostic":"final transcript is not an object"}
        report=None
    query_ids=[item.get("id") for item in queries]
    response_ids=[item.get("id") for item in peer_frames]
    if (any(item.get("type")!="query" or not isinstance(item.get("payload"),dict) for item in queries) or
        any(item.get("type")!="response" for item in peer_frames) or query_ids!=response_ids or
        any(type(item.get("payload",{}).get("input")) is not int or
            not -8<=item.get("payload",{}).get("input",-9)<=8 for item in queries) or
        any(type(item.get("value")) is not int or not -(1<<63)<=item.get("value",0)<(1<<63) for item in peer_frames) or
        any(item.get("type") not in {"query","final"} for item in producer_frames)):
        result={**result,"status":"invalid_protocol","diagnostic":"session frame/revision ordering mismatch"}
        report=None
    final=json.dumps(report,sort_keys=True,separators=(",",":" )).encode()+b"\n" if report else b""
    (Path(outdir)/"stdout.json").write_bytes(final)
    oracle_operations=initial_oracle_operations
    oracle_record=record
    if peer_record is not None:
        private=peer_record["peer_private"]
        oracle_record={"control":private["control"],"reference_member":private["reference_member"]}
    for frame in queries:
        oracle_operations+=oracle_operation_count(oracle_record,frame["payload"]["input"])
    status_name="completed" if result.get("status")=="completed" else result.get("status","session_failed")
    observed_output_bytes=sum(item.get("observed_bytes",0) for item in result.get("byte_evidence",{}).values())
    if observed_output_bytes>session["max_protocol_bytes"]:
        status_name="output_limit"
    return {"completion_status":status_name,
      "report_validation_status":"valid_report" if report else "invalid_report",
      "return_code":0 if status_name=="completed" else None,
      "producer_returncode":result.get("producer_returncode"),
      "peer_returncode":result.get("peer_returncode"),
      "wall_seconds":time.monotonic()-started,
      "cpu_seconds":result.get("session_cpu_seconds"),
      "interactive_children_cpu_seconds":result.get("waited_child_cpu_seconds"),
      "individual_child_peak_rss_sum_bytes":result.get("waited_child_peak_rss_bytes"),
      "conservative_session_rss_sum_bytes":result.get("conservative_rss_sum_bytes"),
      "group_memory_measurement":"measured by outer bounded-program supervisor",
      "session_status":result.get("status"),"session_diagnostic":result.get("diagnostic"),
      "wait4_usage_complete":result.get("wait4_usage_complete",False),
      "stream_capture_complete":result.get("stream_capture_complete",False),
      "observed_output_bytes":observed_output_bytes,
      "session_events":result.get("events"),"protocol_bytes":result.get("protocol_bytes"),
      "stdout_bytes":len(producer_bytes)+len(peer_bytes),"wire_stdout_sha256":sha256_bytes(producer_bytes),
      "wire_responses_sha256":sha256_bytes(peer_bytes),"wire_responses_path":str((Path(outdir)/"wire.responses").resolve()),
      "stderr_sha256":sha256_bytes(stderr),
      "transcript_sha256":sha256_bytes(final),"oracle_operations":oracle_operations,
      "peer_stderr_sha256":sha256_bytes(peer_stderr),
      "transcript_path":str((Path(outdir)/"stdout.json").resolve()),"report":report}

def _load_json(path, label):
    try:
        return json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise Failure(f"cannot load {label}: {error}") from error

def _validate_stage_inputs(manifest, bundle, synthetic_flag, runner):
    if not isinstance(manifest, dict) or manifest.get("schema") != "genetik-fdi-run-manifest-v2":
        raise Failure("unsupported run-manifest schema")
    mode=manifest.get("mode")
    if mode not in {"campaign", "synthetic-preflight"} or (mode == "synthetic-preflight") != synthetic_flag:
        raise Failure("synthetic-preflight flag and manifest mode disagree")
    def has_assessment_identity(value):
        if isinstance(value,dict):
            return any("assessment" in str(key).lower() or has_assessment_identity(item)
                       for key,item in value.items())
        if isinstance(value,list): return any(has_assessment_identity(item) for item in value)
        return False
    if has_assessment_identity(manifest):
        raise Failure("runner manifest must not contain assessment identities")
    if not isinstance(manifest.get("protocol_id"), str) or not manifest["protocol_id"]:
        raise Failure("protocol identity is required")
    session=manifest.get("interactive_session")
    validate_interactive_session(session)
    runner=Path(runner).resolve()
    if not runner.is_file() or not os.access(runner,os.X_OK):
        raise Failure("selector runner is missing or not executable")
    runner_identity=manifest.get("selector_runner")
    if runner_identity is not None and (not isinstance(runner_identity,dict) or
        set(runner_identity)!={"path","sha256"} or Path(runner_identity["path"]).resolve()!=runner or
        sha256_file(runner)!=runner_identity["sha256"]):
        raise Failure("selector runner manifest identity mismatch")
    indexed={}
    if bundle is not None:
        if not isinstance(bundle,dict) or bundle.get("schema")!="genetik-fdi-training-bundle-v1":
            raise Failure("unsupported training-bundle schema")
        artifacts=bundle.get("artifacts")
        if not isinstance(artifacts,list): raise Failure("training artifacts must be a list")
        for artifact in artifacts:
            if not isinstance(artifact,dict) or set(artifact)!={"id","sha256","content_base64"}:
                raise Failure("training artifact descriptor malformed")
            if not isinstance(artifact["id"],str) or artifact["id"] in indexed:
                raise Failure("duplicate or malformed training artifact ID")
            try: content=base64.b64decode(artifact["content_base64"],validate=True)
            except (ValueError,TypeError) as error: raise Failure("invalid training artifact encoding") from error
            if sha256_bytes(content)!=artifact["sha256"]: raise Failure("training artifact hash mismatch")
            indexed[artifact["id"]]=(artifact["sha256"],content)
    runs=manifest.get("runs")
    if not isinstance(runs,list) or not runs: raise Failure("run manifest must contain runs")
    if mode=="campaign" and len(runs)!=1:
        raise Failure("each campaign slot must contain exactly one run")
    seen=set()
    catalogue_by_control={}
    if mode=="synthetic-preflight":
        mappings=manifest.get("catalogue_map")
        if not isinstance(mappings,list): raise Failure("synthetic catalogue mapping is required")
        for mapping in mappings:
            if not isinstance(mapping,dict) or set(mapping) not in ({"control_id","catalogue"},
                                                                     {"control_id","catalogue","reference_member"}):
                raise Failure("synthetic catalogue mapping malformed")
            if mapping["catalogue"] not in {"ambiguity","arithmetic"} or mapping["control_id"] in catalogue_by_control:
                raise Failure("synthetic preflight catalogue is outside frozen controls")
            member=mapping.get("reference_member",0)
            if type(member) is not int or member!=0:
                raise Failure("synthetic oracle member must be the frozen independent control")
            catalogue_by_control[mapping["control_id"]]=mapping["catalogue"]
    for row in runs:
        required={"run_id","control_id","method","seed","training_artifact","training_sha256"}
        if mode=="campaign": required.add("peer_private")
        if not isinstance(row,dict) or not required.issubset(row) or set(row)-required-{"training_path"}:
            raise Failure("run record fields do not match frozen schema")
        if not isinstance(row["run_id"],str) or not row["run_id"] or row["run_id"] in seen:
            raise Failure("duplicate or malformed run ID")
        seen.add(row["run_id"])
        if (not isinstance(row["control_id"],str) or not isinstance(row["method"],str) or
            type(row["seed"]) is not int or row["seed"]<0): raise Failure("run identity fields malformed")
        if mode=="campaign":
            private=row["peer_private"]
            if not isinstance(private,dict) or set(private)!={"control","reference_member"}:
                raise Failure("peer-private descriptor malformed")
            if type(private["reference_member"]) is not int or private["reference_member"] not in range(4):
                raise Failure("peer-private reference member malformed")
            if row["control_id"] != f"{private['control']}_m{private['reference_member']}":
                raise Failure("peer-private target identity does not match frozen control ID")
        elif row["control_id"] not in catalogue_by_control:
            raise Failure("synthetic run lacks a public catalogue mapping")
        if row["training_artifact"] in indexed:
            if indexed[row["training_artifact"]][0]!=row["training_sha256"]:
                raise Failure("run training artifact mapping/hash mismatch")
        elif mode=="synthetic-preflight" and "training_path" in row:
            path=Path(row["training_path"]).resolve()
            if not path.is_file() or sha256_file(path)!=row["training_sha256"]:
                raise Failure("synthetic training artifact path/SHA mismatch")
            indexed[row["training_artifact"]]=(row["training_sha256"],path.read_bytes())
        else: raise Failure("run training artifact is not bound to verified bytes")
        if "training_path" in row:
            path=Path(row["training_path"]).resolve()
            if not path.is_file() or sha256_file(path)!=row["training_sha256"]:
                raise Failure("run training path/SHA binding mismatch")
    return runner,indexed

def _training_rows(data):
    try: lines=data.decode("utf-8").splitlines()
    except UnicodeDecodeError as error: raise Failure("training artifact is not UTF-8") from error
    if not lines or lines[0]!="genetik-fdi-training-v1": raise Failure("training artifact header mismatch")
    result=[]
    for line in lines[1:]:
        pieces=line.split("\t")
        if len(pieces)!=2: raise Failure("training artifact row malformed")
        try: x,y=map(int,pieces)
        except ValueError as error: raise Failure("training artifact contains non-integer values") from error
        if not -(1<<63)<=x<(1<<63) or not -(1<<63)<=y<(1<<63): raise Failure("training values outside Int64")
        result.append((x,y))
    return result

def normalize_synthetic_manifest(source):
    """Convert the preflight's training-only manifest to the runner's strict schema."""
    if not isinstance(source,dict) or source.get("schema")!=1 or not isinstance(source.get("preflight_id"),str):
        raise Failure("synthetic preflight manifest is malformed")
    controls=source.get("controls")
    if not isinstance(controls,list): raise Failure("synthetic preflight controls are missing")
    indexed={}
    artifacts=[]
    for control in controls:
        if not isinstance(control,dict) or not isinstance(control.get("id"),str):
            raise Failure("synthetic control mapping malformed")
        train=control.get("training")
        if not isinstance(train,dict) or not {"id","path","sha256"}.issubset(train):
            raise Failure("synthetic training identity malformed")
        catalogue=control.get("catalogue")
        if catalogue not in {"ambiguity","arithmetic"}:
            raise Failure("synthetic preflight catalogue is outside frozen controls")
        path=Path(train["path"]).resolve()
        if not path.is_file() or sha256_file(path)!=train["sha256"]:
            raise Failure("synthetic training fixture missing or hash mismatch")
        if train["id"] in indexed: raise Failure("duplicate synthetic training artifact")
        content=path.read_bytes()
        indexed[control["id"]]={"training_artifact":train["id"],"training_sha256":train["sha256"],
                                 "catalogue":catalogue,"training_path":str(path)}
        artifacts.append({"id":train["id"],"sha256":train["sha256"],
                          "content_base64":base64.b64encode(content).decode("ascii")})
    runs=[]
    catalogue_map=[]
    for control_id,item in indexed.items():
        catalogue_map.append({"control_id":control_id,"catalogue":item["catalogue"],"reference_member":0})
    for record in source.get("runs",[]):
        if not isinstance(record,dict) or set(record)!={"run_id","control_id","method","seed","training_artifact"}:
            raise Failure("synthetic run binding malformed")
        control=indexed.get(record["control_id"])
        if control is None or record["training_artifact"]!=control["training_artifact"]:
            raise Failure("synthetic run is misbound to control training data")
        runs.append({"run_id":record["run_id"],"control_id":record["control_id"],
            "method":record["method"],"seed":record["seed"],
            "training_artifact":control["training_artifact"],"training_sha256":control["training_sha256"],
            "training_path":control["training_path"]})
    return ({"schema":"genetik-fdi-run-manifest-v2","mode":"synthetic-preflight",
        "protocol_id":source["preflight_id"],"runs":runs,"catalogue_map":catalogue_map,
        "interactive_session":source.get("interactive_session"),
        "selector_runner":source.get("selector_runner")},
        {"schema":"genetik-fdi-training-bundle-v1","artifacts":artifacts})

def stage_main(argv=None):
    ap=argparse.ArgumentParser()
    ap.add_argument("--run-manifest",required=True)
    ap.add_argument("--training-bundle")
    ap.add_argument("--selector-runner",required=True)
    ap.add_argument("--output",required=True)
    ap.add_argument("--synthetic-preflight",action="store_true")
    args=ap.parse_args(argv)
    manifest_path=Path(args.run_manifest).resolve()
    manifest=_load_json(manifest_path,"run manifest")
    bundle_path=Path(args.training_bundle).resolve() if args.training_bundle else None
    if args.synthetic_preflight:
        if manifest.get("schema")==1:
            manifest,bundle=normalize_synthetic_manifest(manifest)
        else:
            bundle=_load_json(bundle_path,"synthetic training bundle") if bundle_path else None
    else:
        if bundle_path is None: raise Failure("campaign mode requires the training bundle")
        bundle=_load_json(bundle_path,"training bundle")
    runner,artifacts=_validate_stage_inputs(manifest,bundle,args.synthetic_preflight,args.selector_runner)
    output=Path(args.output).resolve(); evidence_root=output.with_name(output.name+".evidence")
    if output.exists() or evidence_root.exists(): raise Failure("candidate output and evidence are create-exclusive")
    evidence_root.mkdir(parents=True,exist_ok=False)
    candidate_rows=[]
    candidate_index_rows=[]
    synthetic_catalogue={item["control_id"]:item
                         for item in manifest.get("catalogue_map",[])}
    for run_ordinal,raw in enumerate(manifest["runs"]):
        # The producer-visible record is deliberately constructed by allowlist;
        # peer_private, assessment identities and other fields never cross over.
        mapping=synthetic_catalogue.get(raw["control_id"],{})
        private=(raw["peer_private"] if manifest["mode"]=="campaign" else
                 {"control":mapping.get("catalogue"),"reference_member":mapping.get("reference_member",0)})
        control=private["control"]
        producer_run={"run_id":raw["run_id"],"control":control,"policy":raw["method"],
                      "seed_metadata":raw["seed"],"training_artifact":raw["training_artifact"]}
        # Use ordinal-owned directories rather than untrusted run IDs as path
        # components. The index binds each row back to its exact frozen ID.
        run_dir=evidence_root/f"run-{run_ordinal:02d}"
        run_dir.mkdir(exist_ok=False)
        digest,training_bytes=artifacts[raw["training_artifact"]]
        training_path=run_dir/"training.tsv"
        with training_path.open("xb") as stream:
            stream.write(training_bytes); stream.flush(); os.fsync(stream.fileno())
        training_path.chmod(0o444)
        rows=_training_rows(training_bytes)
        # Operation accounting is based only on public training inputs and the
        # oracle's separately pinned private descriptor; no target values enter producer IO.
        initial_ops=sum(oracle_operation_count({"control":control,"reference_member":private["reference_member"]},x)
                        for x,_ in rows)
        command=producer_command(runner,producer_run,training_path)
        measurement=execute_one(command,run_dir,producer_run,initial_ops,manifest,runner,
                                peer_record={"peer_private":private})
        report=measurement.pop("report")
        selector_work=report.get("work",{}).get("charged_work",WORK_LIMIT) if isinstance(report,dict) else WORK_LIMIT
        measurement["selector_charged_work"]=selector_work
        measurement["combined_charged_work"]=selector_work+measurement["oracle_operations"]
        measurement["selector_stop_reason"]=report.get("stop_reason") if isinstance(report,dict) else "no_report"
        if measurement["combined_charged_work"]>WORK_LIMIT: measurement["completion_status"]="mathematical_work_limit"
        if measurement["oracle_operations"]>ORACLE_WORK_RESERVE: measurement["completion_status"]="oracle_reserve_exceeded"
        work=report.get("work",{}) if isinstance(report,dict) else {}
        if (measurement["selector_stop_reason"] not in {"one_finite_behavior_class","empty_compatible_set","query_cap"} or
            work.get("charged_work",WORK_LIMIT+1)>SELECTOR_WORK_LIMIT or
            work.get("candidate_row_evaluations",1025)>1024 or
            work.get("logical_storage_bytes",1024*1024+1)>1024*1024 or
            work.get("catalogue_candidates",0)>8 or work.get("discovered_structures",1)!=0):
            measurement["completion_status"]="selector_incomplete_or_over_limit"
        measurement["training_sha256"]=digest
        receipt={"run_id":raw["run_id"],"control_id":raw["control_id"],"method":raw["method"],
                 "seed":raw["seed"],"training_sha256":digest,**measurement}
        raw_stream_sha256={"producer_stdout":measurement["wire_stdout_sha256"],
            "peer_stdout":measurement["wire_responses_sha256"],
            "producer_stderr":measurement["stderr_sha256"],
            "peer_stderr":measurement["peer_stderr_sha256"]}
        receipt["raw_stream_sha256"]=raw_stream_sha256
        candidate_rows.append({"run_id":raw["run_id"],"control_id":raw["control_id"],
            "method":raw["method"],"seed":raw["seed"],"training_sha256":digest,
            "training_artifact":raw["training_artifact"],
            "completion_status":receipt["completion_status"],"report_validation_status":receipt["report_validation_status"],
            "transcript":report,"receipt":receipt,
            "evidence_directory":".",
            "evidence":{"wire_stdout_sha256":measurement["wire_stdout_sha256"],
                        "wire_responses_sha256":measurement["wire_responses_sha256"],
                        "stderr_sha256":measurement["stderr_sha256"],
                        "peer_stderr_sha256":measurement["peer_stderr_sha256"]},
            "raw_stream_sha256":raw_stream_sha256})
    for row in candidate_rows:
        row["schema"]="genetik-fdi-candidate-output-v2"
        row["protocol_id"]=manifest["protocol_id"]
        row["run_manifest_sha256"]=sha256_file(manifest_path)
        row["training_bundle_sha256"]=sha256_file(bundle_path) if bundle_path else None
    # Full transcripts and process receipts live in bounded per-run sidecars.
    # The stage artifact is a compact ordered index, keeping the top-level
    # output under its original cap without increasing any per-run allowance.
    for ordinal,(raw,row) in enumerate(zip(manifest["runs"],candidate_rows)):
        per_run=evidence_root/f"run-{ordinal:02d}"/"candidate.json"
        row_bytes=(json.dumps(row,sort_keys=True,separators=(",",":"))+"\n").encode("utf-8")
        per_run_cap=manifest["interactive_session"]["max_protocol_bytes"]
        if len(row_bytes)>per_run_cap:
            raise Failure(f"per-run candidate output exceeds existing byte cap for {raw['run_id']}")
        with per_run.open("xb") as stream:
            stream.write(row_bytes); stream.flush(); os.fsync(stream.fileno())
        per_run.chmod(0o444)
        candidate_index_rows.append({"run_id":raw["run_id"],
            "path":str(per_run.relative_to(output.parent)),"sha256":sha256_file(per_run)})
    output_index={"schema":"genetik-fdi-candidate-output-index-v1",
        "protocol_id":manifest["protocol_id"],
        "run_manifest_sha256":sha256_file(manifest_path),"runs":candidate_index_rows}
    output_bytes=(json.dumps(output_index,sort_keys=True,separators=(",",":"))+"\n").encode("utf-8")
    if len(output_bytes)>manifest["interactive_session"]["max_protocol_bytes"]:
        raise Failure("candidate output index exceeds frozen protocol byte cap")
    with output.open("xb") as stream:
        stream.write(output_bytes); stream.flush(); os.fsync(stream.fileno())
    return 0

def main():
    try:
        return stage_main()
    except (Failure,KeyError,ValueError,OSError,subprocess.SubprocessError) as error:
        raise SystemExit(f"run matrix failed: {error}")

if __name__=="__main__":
    try: main()
    except (Failure,KeyError,ValueError,OSError,subprocess.SubprocessError) as error:
        raise SystemExit(f"run matrix failed: {error}")
