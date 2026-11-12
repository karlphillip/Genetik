#!/usr/bin/env python3
"""Create deterministic, separate training and sealed assessment artifacts.

Run only as part of machine-protocol preparation. Files are create-exclusive;
this script never overwrites an existing row, manifest, or checksum.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path


def rem(n, d): return n % d

def reference(control, member, n):
    if control == "arithmetic": return [n+2, 2*n+2, n*n+2, 2][member]
    if control == "modular": return [n%3, n%5, (n%3)%2, n%2][member]
    if control == "threshold": return [abs(n), n, abs(n) if n < -1 else n, abs(n)+1 if n < 0 else n][member]
    if control == "conditional-modular":
        q=n*n-2
        return [n+2 if n%3==1 else q, n+3 if n%3==1 else q,
                2*n if n%3==1 else q, q][member]
    if control == "ambiguity": return n
    if control == "outside": return (1 << 40) if n == 0 else n
    raise ValueError(control)

def initial_inputs(control):
    return {"arithmetic": [0], "ambiguity": [0], "outside": [0], "modular": [0,1],
            "threshold": [0,1,2], "conditional-modular": [0,2,3]}[control]

def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def write_content_addressed(path, content):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        if path.read_text() != content: raise RuntimeError("content-address collision")
        return
    with path.open("x", encoding="utf-8", newline="\n") as stream: stream.write(content)

def write_exclusive(path, content):
    path=Path(path); path.parent.mkdir(parents=True,exist_ok=True)
    with path.open("x",encoding="utf-8",newline="\n") as stream: stream.write(content)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--protocol-id", required=True)
    ap.add_argument("--manifest", required=True)
    args=ap.parse_args()
    root=Path(args.root)
    if root.exists() and any(root.iterdir()): raise SystemExit("artifact root must be absent or empty")
    planned=[]
    for control in ("arithmetic","modular","threshold","conditional-modular"):
        for member in range(4):
            for policy in ("ascending","disagreement"):
                planned.append((control,member,policy,0,None))
    for control in ("ambiguity","outside"):
        for policy in ("ascending","disagreement"):
            planned.append((control,0,policy,0,None))
    for policy in ("ascending","disagreement"):
        primary_index=next(i for i,row in enumerate(planned)
                           if row[0]=="threshold" and row[1]==0 and row[2]==policy and row[3]==0)
        planned.append(("threshold",0,policy,1,primary_index))
    training_dir=root/"training"
    assessment_dir=root/"assessment-sealed"
    rows=[]
    run_ids=[f"run_{i+1:03d}" for i in range(len(planned))]
    for index,(control,member,policy,seed,replay_index) in enumerate(planned):
        run_id=run_ids[index]
        replay_of=run_ids[replay_index] if replay_index is not None else None
        training_lines=["genetik-fdi-training-v1"]
        for n in initial_inputs(control):
            training_lines.append(f"{n}\t{reference(control,member,n)}")
        training_content="\n".join(training_lines)+"\n"
        training_hash=hashlib.sha256(training_content.encode()).hexdigest()
        training=training_dir/f"training_{training_hash}.tsv"
        write_content_addressed(training,training_content)
        assessment_lines=["genetik-fdi-assessment-v1"]
        for n in list(range(-17,-8))+list(range(9,18)):
            assessment_lines.append(f"{n}\t{reference(control,member,n)}")
        assessment_content="\n".join(assessment_lines)+"\n"
        assessment_hash=hashlib.sha256(assessment_content.encode()).hexdigest()
        assessment=assessment_dir/f"assessment_{assessment_hash}.tsv"
        write_content_addressed(assessment,assessment_content)
        row={"run_id":run_id,"control":control,"reference_member":member,
                     "reference_candidate_id":f"c{member}","policy":policy,"seed_metadata":seed,
                     "training_path":str(training.resolve()),"training_sha256":training_hash,
                     "assessment_path":str(assessment.resolve()),"assessment_sha256":assessment_hash}
        if replay_of is not None: row["replay_of"]=replay_of
        rows.append(row)
    manifest={"protocol_id":args.protocol_id,"runs":rows}
    write_exclusive(Path(args.manifest),json.dumps(manifest,sort_keys=True,separators=(",",":"))+"\n")
    print(json.dumps({"run_count":len(rows),"training_artifacts":len(rows),
                      "assessment_artifacts":len(rows),"manifest_sha256":digest(Path(args.manifest))},sort_keys=True))

if __name__ == "__main__": main()
