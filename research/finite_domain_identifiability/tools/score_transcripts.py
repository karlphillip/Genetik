#!/usr/bin/env python3
"""Post-selection exact scorer for finite-domain identifiability transcripts.

This tool is deliberately not imported by runner.cpp or the selector. It takes
sealed transcript files plus a separate adjudication manifest and is run once
only after selection/transcript sealing.
"""
from __future__ import annotations
import argparse
import base64
import hashlib
import json
from pathlib import Path
import re

TOKEN = re.compile(r"\s*(?:(-?[0-9]+)|(x[0-9]+)|(if|true|false)|(%|==|<=|>=|<|>|\+|-|\*|,|\(|\)))")
I64_MIN, I64_MAX = -(1 << 63), (1 << 63) - 1

class Invalid(Exception):
    pass

class Parser:
    def __init__(self, text: str):
        self.tokens = []
        pos = 0
        while pos < len(text):
            match = TOKEN.match(text, pos)
            if not match:
                raise Invalid(f"unrecognized expression at byte {pos}")
            self.tokens.append(next(g for g in match.groups() if g is not None))
            pos = match.end()
        self.i = 0
    def peek(self): return self.tokens[self.i] if self.i < len(self.tokens) else None
    def take(self, wanted=None):
        value = self.peek()
        if value is None or (wanted is not None and value != wanted):
            raise Invalid(f"expected {wanted!r}, got {value!r}")
        self.i += 1
        return value
    def expression(self):
        if self.peek() == "if":
            self.take("if"); self.take("("); cond = self.expression(); self.take(",")
            yes = self.expression(); self.take(","); no = self.expression(); self.take(")")
            return ("if", cond, yes, no)
        if self.peek() == "(":
            self.take("("); left = self.expression(); op = self.take()
            if op not in {"+", "-", "*", "%", "==", "<", "<=", ">", ">="}:
                raise Invalid(f"unsupported operator {op}")
            right = self.expression(); self.take(")")
            return (op, left, right)
        token = self.take()
        if token == "true": return ("bool", True)
        if token == "false": return ("bool", False)
        if token.startswith("x"):
            if token != "x0": raise Invalid("only x0 is supported")
            return ("var", 0)
        try: return ("int", int(token))
        except ValueError as e: raise Invalid(f"unexpected token {token}") from e
    def parse(self):
        tree = self.expression()
        if self.peek() is not None: raise Invalid("trailing tokens")
        return tree

def complexity(tree):
    if tree[0] in {"int", "bool", "var"}: return 1, 1
    children = tree[1:]
    metrics = [complexity(child) for child in children]
    return 1 + sum(n for n, _ in metrics), 1 + max((d for _, d in metrics), default=0)

def checked(value):
    if not I64_MIN <= value <= I64_MAX: raise Invalid("checked Int64 overflow")
    return value

def evaluate(tree, n):
    tag = tree[0]
    if tag == "int": return tree[1]
    if tag == "bool": return tree[1]
    if tag == "var": return checked(n)
    if tag == "if": return evaluate(tree[2], n) if evaluate(tree[1], n) else evaluate(tree[3], n)
    left, right = evaluate(tree[1], n), evaluate(tree[2], n)
    if tag == "+": return checked(left + right)
    if tag == "-": return checked(left - right)
    if tag == "*": return checked(left * right)
    if tag == "%":
        if right <= 0: raise Invalid("modulo divisor must be positive")
        return left % right
    if tag == "==": return left == right
    if tag == "<": return left < right
    if tag == "<=": return left <= right
    if tag == ">": return left > right
    if tag == ">=": return left >= right
    raise Invalid(f"unknown AST node {tag}")

def reference(control, member, n):
    if control == "arithmetic": return [n+2, 2*n+2, n*n+2, 2][member]
    if control == "modular": return [n%3, n%5, (n%3)%2, n%2][member]
    if control == "threshold": return [abs(n), n, abs(n) if n < -1 else n, abs(n)+1 if n < 0 else n][member]
    if control == "conditional-modular":
        q = n*n-2
        return [n+2 if n%3==1 else q, n+3 if n%3==1 else q,
                2*n if n%3==1 else q, q][member]
    if control == "ambiguity": return n
    if control == "outside": return (1 << 40) if n == 0 else n
    raise Invalid(f"unknown control {control}")

def sha256(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def sha256_bytes(data): return hashlib.sha256(data).hexdigest()

def verify_harness(record, ledger_receipts):
    receipt_path=Path(record["receipt_path"])
    if not receipt_path.is_file() or sha256(receipt_path)!=record["receipt_sha256"]:
        raise Invalid(f"execution receipt hash mismatch for {record['run_id']}")
    receipt=json.loads(receipt_path.read_text())
    ledger_event=ledger_receipts.get(record["run_id"])
    if (receipt.get("run_id")!=record["run_id"] or not isinstance(ledger_event,dict) or
        ledger_event.get("receipt_sha256")!=record["receipt_sha256"] or
        ledger_event.get("cpu_seconds")!=receipt.get("cpu_seconds") or
        ledger_event.get("wall_seconds")!=receipt.get("wall_seconds") or
        ledger_event.get("charged_work")!=receipt.get("combined_charged_work") or
        ledger_event.get("completion_status")!=receipt.get("completion_status") or
        ledger_event.get("report_validation_status")!=receipt.get("report_validation_status")):
        raise Invalid(f"receipt is not bound to the matrix ledger for {record['run_id']}")
    if (receipt.get("completion_status")!=record.get("completion_status") or
        receipt.get("report_validation_status")!=record.get("report_validation_status")):
        raise Invalid(f"receipt status mismatch for {record['run_id']}")
    for key in ("selector_charged_work","oracle_operations","combined_charged_work",
                "cpu_seconds","wall_seconds","interactive_children_cpu_seconds",
                "individual_child_peak_rss_sum_bytes","conservative_session_rss_sum_bytes",
                "session_status","wait4_usage_complete","stream_capture_complete","observed_output_bytes",
                "session_events"):
        if receipt.get(key)!=record.get(key): raise Invalid(f"receipt resource mismatch for {record['run_id']}:{key}")
    if receipt.get("completion_status")!="completed" or receipt.get("report_validation_status")!="valid_report":
        return receipt, None
    if (receipt.get("session_status")!="completed" or receipt.get("wait4_usage_complete") is not True or
        receipt.get("stream_capture_complete") is not True or
        type(receipt.get("individual_child_peak_rss_sum_bytes")) is not int or
        type(receipt.get("conservative_session_rss_sum_bytes")) is not int or
        receipt["conservative_session_rss_sum_bytes"]<receipt["individual_child_peak_rss_sum_bytes"] or
        receipt.get("observed_output_bytes",1048577)>1048576):
        raise Invalid(f"interactive resource receipt is incomplete for {record['run_id']}")
    transcript_path=Path(record["transcript_path"])
    if (receipt.get("transcript_path")!=str(transcript_path.resolve()) or
        receipt.get("transcript_sha256")!=record["transcript_sha256"] or
        not transcript_path.is_file() or sha256(transcript_path)!=record["transcript_sha256"]):
        raise Invalid(f"runner report hash/path mismatch for {record['run_id']}")
    transcript=json.loads(transcript_path.read_text())
    if transcript.get("run_id")!=record["run_id"]: raise Invalid("runner report identity mismatch")
    wire_path=Path(record["wire_stdout_path"])
    if not wire_path.is_file() or sha256(wire_path)!=record["wire_stdout_sha256"] or \
       receipt.get("wire_stdout_sha256")!=record["wire_stdout_sha256"]:
        raise Invalid(f"selector protocol-output hash mismatch for {record['run_id']}")
    try: messages=[json.loads(line) for line in wire_path.read_text().splitlines()]
    except ValueError as error: raise Invalid(f"invalid selector wire output for {record['run_id']}") from error
    queries=[message for message in messages if message.get("type")=="query"]
    finals=[message.get("transcript") for message in messages if message.get("type")=="final"]
    if ([message.get("payload",{}).get("input") for message in queries] !=
        [step["input"] for step in transcript["steps"]] or
        [message.get("id") for message in queries] != [step.get("query_id") for step in transcript["steps"]] or
        finals != [transcript]):
        raise Invalid(f"runner report differs from wire transcript for {record['run_id']}")
    response_path=Path(record["wire_responses_path"])
    if (not response_path.is_file() or sha256(response_path)!=record["wire_responses_sha256"] or
        receipt.get("wire_responses_path")!=str(response_path.resolve()) or
        receipt.get("wire_responses_sha256")!=record["wire_responses_sha256"]):
        raise Invalid(f"oracle response transcript hash/path mismatch for {record['run_id']}")
    for output_name,field in (("stderr.txt","stderr_sha256"),("peer.stderr.txt","peer_stderr_sha256")):
        output_path=receipt_path.parent/output_name
        if (not output_path.is_file() or sha256(output_path)!=record.get(field) or
            receipt.get(field)!=record.get(field)):
            raise Invalid(f"diagnostic output hash mismatch for {record['run_id']}:{output_name}")
    try: responses=[json.loads(line) for line in response_path.read_text().splitlines()]
    except ValueError as error: raise Invalid("invalid oracle response wire output") from error
    if ([message.get("id") for message in responses] != [step.get("query_id") for step in transcript["steps"]] or
        any(message.get("type")!="response" for message in responses)):
        raise Invalid(f"oracle response/query ordering mismatch for {record['run_id']}")
    producer_lines=wire_path.read_bytes().splitlines(keepends=True)
    response_lines=response_path.read_bytes().splitlines(keepends=True)
    events=record.get("session_events")
    if (not isinstance(events,list) or len(events)!=len(response_lines)+1 or
        receipt.get("wait4_usage_complete") is not True or receipt.get("stream_capture_complete") is not True or
        receipt.get("observed_output_bytes",1048577)>1048576):
        raise Invalid(f"interactive resource/event receipt is incomplete for {record['run_id']}")
    previous="0"*64
    for event in events:
        body={key:value for key,value in event.items() if key!="sha256"}
        digest=hashlib.sha256(json.dumps(body,sort_keys=True,separators=(",",":")).encode()).hexdigest()
        if event.get("previous_sha256")!=previous or event.get("sha256")!=digest:
            raise Invalid(f"interactive event hash chain mismatch for {record['run_id']}")
        previous=digest
    exchanges=[event for event in events if event.get("kind")=="exchange"]
    if len(exchanges)!=len(response_lines) or events[-1].get("kind")!="terminal":
        raise Invalid(f"interactive event count mismatch for {record['run_id']}")
    for ordinal,(event,request,response) in enumerate(zip(exchanges,producer_lines,response_lines),start=1):
        if (event.get("ordinal")!=ordinal or event.get("request_bytes")!=len(request) or
            event.get("request_sha256")!=sha256_bytes(request) or event.get("response_bytes")!=len(response) or
            event.get("response_sha256")!=sha256_bytes(response)):
            raise Invalid(f"interactive exchange evidence mismatch for {record['run_id']}:{ordinal}")
    terminal=events[-1]
    if (len(producer_lines)!=len(response_lines)+1 or terminal.get("request_bytes")!=len(producer_lines[-1]) or
        terminal.get("request_sha256")!=sha256_bytes(producer_lines[-1])):
        raise Invalid(f"interactive terminal evidence mismatch for {record['run_id']}")
    for response, step in zip(responses, transcript["steps"]):
        expected_value=reference(record["control"],record["reference_member"],step["input"])
        if response.get("value")!=expected_value or response.get("value")!=step["response"].get("value"):
            raise Invalid(f"oracle response value mismatch for {record['run_id']}:{step['query_id']}")
    return receipt, transcript

def verify_matrix_chain(manifest):
    ledger=Path(manifest["matrix_ledger_path"])
    summary=Path(manifest["matrix_summary_path"])
    if sha256(ledger)!=manifest["matrix_ledger_sha256"] or sha256(summary)!=manifest["matrix_summary_sha256"]:
        raise Invalid("matrix ledger/summary hash mismatch")
    events=[]; previous="0"*64
    for line in ledger.read_text().splitlines():
        event=json.loads(line); stated=event.pop("sha256",None)
        if (event.get("previous_sha256")!=previous or hashlib.sha256(json.dumps(
            event,sort_keys=True,separators=(",",":")).encode()).hexdigest()!=stated):
            raise Invalid("matrix event hash chain mismatch")
        previous=stated; events.append(event)
    run_events=[event for event in events if event.get("event")=="run_completed"]
    complete=manifest.get("matrix_complete") is True
    if (len(run_events)>38 or (complete and len(run_events)!=38) or
        events[0].get("event")!="matrix_started" or events[-1].get("event")!="matrix_finished"):
        raise Invalid("matrix ledger run count/status is malformed")
    if [e.get("run_id") for e in run_events] != [f"run_{i+1:03d}" for i in range(len(run_events))]:
        raise Invalid("matrix ledger violates the ordered run allowlist")
    summary_data=json.loads(summary.read_text())
    finish=events[-1]
    if (summary_data.get("complete") is not complete or finish.get("complete") is not complete or
        finish.get("charged_cpu_seconds")!=summary_data.get("cpu_seconds") or
        finish.get("charged_wall_seconds")!=summary_data.get("wall_seconds") or
        finish.get("charged_work")!=summary_data.get("charged_work") or
        (complete and (summary_data.get("resource_measurements_complete") is not True or
                       not isinstance(summary_data.get("cpu_seconds"),(int,float)) or
                       summary_data.get("cpu_seconds",77)>76)) or
        summary_data.get("wall_seconds",115)>114 or
        summary_data.get("charged_work",3800001)>3800000):
        raise Invalid("matrix cumulative resource receipt mismatch")
    return {event["run_id"]:event for event in run_events}

def expected_catalogue(control):
    base = {
        "arithmetic": ["(x0 + 2)", "((2 * x0) + 2)", "((x0 * x0) + 2)", "2"],
        "modular": ["(x0 % 3)", "(x0 % 5)", "((x0 % 3) % 2)", "(x0 % 2)"],
        "threshold": ["if((x0 < 0), (0 - x0), x0)", "x0",
                      "if((x0 < -1), (0 - x0), x0)",
                      "if((x0 < 0), ((0 - x0) + 1), x0)"],
        "conditional-modular": [
            "if(((x0 % 3) == 1), (x0 + 2), ((x0 * x0) - 2))",
            "if(((x0 % 3) == 1), (x0 + 3), ((x0 * x0) - 2))",
            "if(((x0 % 3) == 1), (2 * x0), ((x0 * x0) - 2))",
            "((x0 * x0) - 2)"],
        "ambiguity": ["x0", "(x0 + 0)", "if((x0 > 8), (x0 + 1), x0)"],
    }
    key = "arithmetic" if control == "outside" else control
    if key not in base: raise Invalid(f"unknown catalogue {control}")
    result = []
    for i, expr in enumerate(base[key]):
        result.append((f"c{i}", expr))
        if control != "ambiguity": result.append((f"c{i}_plus0", f"({expr} + 0)"))
    return result

def initial_inputs(control):
    return {"arithmetic": [0], "ambiguity": [0], "outside": [0],
            "modular": [0, 1], "threshold": [0, 1, 2],
            "conditional-modular": [0, 2, 3]}[control]

def specification_identity(rows):
    parts = [f"discrete-spec-v1;d=1;rows={len(rows)};"]
    for x, y in rows:
        parts.append(f"[{x},]=i{y};")
    return "".join(parts)

def verify_revision_chain(record, transcript, expected_ids, observed):
    """Recompute the ordered finite-spec chain from initial rows and counterexamples."""
    initial_count=len(transcript["initial_observations"])
    if len(observed)!=initial_count+len(transcript["steps"]):
        raise Invalid(f"missing or extra specification revisions for {record['run_id']}")
    initial_rows=[(row.get("input"),row.get("expected",{}).get("value"))
                  for row in transcript["initial_observations"]]
    if observed[:initial_count]!=initial_rows:
        raise Invalid(f"initial revision observations changed for {record['run_id']}")
    predecessor=specification_identity(observed[:initial_count])
    retired=set()
    for ordinal, step in enumerate(transcript["steps"], start=1):
        query_id=f"q{ordinal}"
        if (type(step.get("revision")) is not int or step.get("revision")!=ordinal or
            step.get("revision_id")!=f"revision_{ordinal}" or
            step.get("query_id")!=query_id or step.get("predecessor_specification_identity")!=predecessor):
            raise Invalid(f"revision predecessor/order mismatch for {record['run_id']}:{ordinal}")
        expected_counterexample={"query_id":query_id,"observation":{
            "inputs":[step.get("input")],"expected":{"type":"int64","value":step.get("response",{}).get("value")}}}
        if step.get("counterexample")!=expected_counterexample:
            raise Invalid(f"counterexample is not bound to its query for {record['run_id']}:{ordinal}")
        if observed[initial_count+ordinal-1]!=(step["input"],step["response"]["value"]):
            raise Invalid(f"revision observation order mismatch for {record['run_id']}:{ordinal}")
        recomputed=specification_identity(observed[:initial_count+ordinal])
        if step.get("specification_identity")!=recomputed:
            raise Invalid(f"revision identity mismatch for {record['run_id']}:{ordinal}")
        checks=step.get("validations")
        if not isinstance(checks,list) or [item.get("candidate_id") for item in checks]!=expected_ids:
            raise Invalid(f"revision candidate checks missing or reordered for {record['run_id']}:{ordinal}")
        for check in checks:
            status=check.get("status")
            candidate_id=check["candidate_id"]
            if status not in {"ValidatedFiniteDomain","CounterexampleFound"}:
                raise Invalid(f"unresolved candidate outcome in {record['run_id']}:{ordinal}")
            if candidate_id in retired and status!="CounterexampleFound":
                raise Invalid(f"eliminated candidate resurrected in {record['run_id']}:{ordinal}")
            if status=="CounterexampleFound": retired.add(candidate_id)
        predecessor=recomputed
    if predecessor!=transcript["specification_identity"]:
        raise Invalid(f"final specification identity differs from revision chain for {record['run_id']}")
    return predecessor

def require_nonnegative_int(value, label):
    if type(value) is not int or value < 0:
        raise Invalid(f"{label} must be a nonnegative integer")
    return value

def read_artifact(path, digest, header, width):
    path = Path(path)
    if sha256(path) != digest: raise Invalid(f"artifact hash mismatch: {path}")
    lines = path.read_text().splitlines()
    if not lines or lines[0] != header: raise Invalid(f"invalid artifact header: {path}")
    result = []
    for line in lines[1:]:
        cols = line.split("\t")
        if len(cols) != width: raise Invalid(f"invalid artifact row: {path}")
        result.append(tuple(int(col) for col in cols))
    if sha256(path) != digest: raise Invalid(f"artifact changed while being read: {path}")
    return result

def check_validation_event(event, cid, tree, rows, rid):
    if event["candidate_id"] != cid: raise Invalid(f"validation candidate order mismatch in {rid}")
    for key in ("rows_attempted","rows_matched","node_visits","operations","iterations",
                "duplicate_tuple_comparisons","specification_comparisons"):
        require_nonnegative_int(event.get(key),f"validation counter {key} in {rid}")
    mismatch = next(((ordinal, x, expected, evaluate(tree, x))
                     for ordinal, (x, expected) in enumerate(rows)
                     if evaluate(tree, x) != expected), None)
    if mismatch is None:
        expected_status, attempted, matched = "ValidatedFiniteDomain", len(rows), len(rows)
        expected_fields = (None, None, None, None)
    else:
        ordinal, x, expected, actual = mismatch
        expected_status, attempted, matched = "CounterexampleFound", ordinal + 1, ordinal
        expected_fields = (ordinal, x, expected, actual)
    actual_fields = (event["mismatch_ordinal"], event["mismatch_input"],
                     event["mismatch_expected"], event["mismatch_actual"])
    for value in actual_fields:
        if value is not None and type(value) is not int:
            raise Invalid(f"validation mismatch counter/value has invalid type in {rid}")
    if (event["status"] != expected_status or event["rows_attempted"] != attempted or
        event["rows_matched"] != matched or actual_fields != expected_fields):
        raise Invalid(f"validation/counterexample record mismatch for {rid}:{cid}")

def verify_transcript(record, transcript):
    rid, control, member = record["run_id"], record["control"], record["reference_member"]
    if not isinstance(rid, str) or not rid or transcript.get("run_id") != rid:
        raise Invalid("run identity is malformed or mismatched")
    if type(member) is not int or member < 0 or member >= 4:
        raise Invalid(f"reference member is malformed for {rid}")
    if record.get("reference_candidate_id") != f"c{member}":
        raise Invalid(f"reference candidate metadata mismatch for {rid}")
    expected = expected_catalogue(control)
    actual = list(zip(transcript["candidate_ids"], transcript["candidate_expressions"]))
    if actual != expected: raise Invalid(f"candidate catalogue mismatch for {rid}")
    ids = [cid for cid, _ in expected]
    trees = [Parser(expr).parse() for _, expr in expected]
    metrics = {cid: complexity(tree) for cid, tree in zip(ids, trees)}
    if any(nodes > 21 or depth > 6 for nodes, depth in metrics.values()):
        raise Invalid(f"candidate exceeds frozen structural limits for {rid}")
    domain = list(range(-8, 9))
    if (not isinstance(transcript.get("domain"),list) or
        any(type(value) is not int for value in transcript["domain"]) or transcript["domain"] != domain):
        raise Invalid(f"query domain mismatch for {rid}")
    signatures = {cid: [evaluate(tree, x) for x in domain] for cid, tree in zip(ids, trees)}
    classes = []
    candidate_class = {}
    for cid in ids:
        sig = tuple(signatures[cid])
        if sig not in [entry[1] for entry in classes]:
            class_id = f"b{len(classes)}"
            classes.append((class_id, sig))
        candidate_class[cid] = classes[[entry[1] for entry in classes].index(sig)][0]
    observed = []
    expected_initial = initial_inputs(control)
    initial = transcript["initial_observations"]
    if [row["input"] for row in initial] != expected_initial:
        raise Invalid(f"initial input set/order mismatch for {rid}")
    for row in initial:
        if (type(row.get("input")) is not int or not isinstance(row.get("expected"), dict) or
            type(row["expected"].get("value")) is not int or
            not I64_MIN <= row["expected"]["value"] <= I64_MAX):
            raise Invalid(f"initial observation contains malformed Int64 data in {rid}")
        x, y = row["input"], row["expected"]["value"]
        if row["expected"].get("type") != "int64" or y != reference(control, member, x):
            raise Invalid(f"initial oracle mismatch for {rid}")
        observed.append((x, y))
    training_rows = read_artifact(record["training_path"], record["training_sha256"],
                                  "genetik-fdi-training-v1", 2)
    if training_rows != observed:
        raise Invalid(f"training artifact disagrees with transcript for {rid}")
    # Verify every selector decision and finite certificate from the transcript only.
    compatible = [cid for cid in ids if all(evaluate(trees[ids.index(cid)], x) == y for x, y in observed)]
    viable_classes = []
    for class_id, signature in classes:
        members = [cid for cid in ids if candidate_class[cid] == class_id]
        if any(cid in compatible for cid in members): viable_classes.append(class_id)
    if [e["candidate_id"] for e in transcript["initial_compatibility"]] != ids:
        raise Invalid(f"initial validation order mismatch for {rid}")
    for event, cid in zip(transcript["initial_compatibility"], ids):
        check_validation_event(event, cid, trees[ids.index(cid)], observed, rid)
    if [e["candidate_id"] for e in transcript["signature_checks"]] != [cid for cid in ids for _ in domain]:
        raise Invalid(f"signature validation order mismatch for {rid}")
    signature_rows = [(x, evaluate(tree, x)) for tree in trees for x in domain]
    for event_index, event in enumerate(transcript["signature_checks"]):
        cid = ids[event_index // len(domain)]
        x, value = signature_rows[event_index]
        check_validation_event(event, cid, trees[ids.index(cid)], [(x, value)], rid)
    selected_queries = []
    if not isinstance(transcript.get("steps"), list):
        raise Invalid(f"query steps malformed in {rid}")
    if len(transcript["steps"]) > 8:
        raise Invalid(f"query count exceeds limit in {rid}")
    for step_index, step in enumerate(transcript["steps"]):
        if len(viable_classes) <= 1:
            raise Invalid(f"query continued after one finite behavior class in {rid}")
        if not isinstance(step, dict) or type(step.get("input")) is not int:
            raise Invalid(f"query input must be an integer in {rid}")
        if not domain[0] <= step["input"] <= domain[-1]:
            raise Invalid(f"query input outside finite domain in {rid}")
        response = step.get("response")
        if (not isinstance(response, dict) or response.get("type") != "int64" or
            type(response.get("value")) is not int or not I64_MIN <= response["value"] <= I64_MAX):
            raise Invalid(f"query response must be a checked Int64 in {rid}")
        unobserved = [x for x in domain if x not in {row[0] for row in observed}]
        if not viable_classes: raise Invalid(f"query after empty compatible set in {rid}")
        if transcript["policy"] == "ascending":
            chosen = unobserved[0]
        elif transcript["policy"] == "disagreement":
            choices = []
            for x in unobserved:
                buckets = {}
                for class_id in viable_classes:
                    signature = classes[int(class_id[1:])][1]
                    buckets.setdefault(signature[x - domain[0]], []).append(class_id)
                choices.append((max(map(len, buckets.values())), x, buckets))
            _, chosen, _ = min(choices, key=lambda item: (item[0], item[1]))
        else: raise Invalid(f"unknown policy in {rid}")
        if step["input"] != chosen: raise Invalid(f"selected input violates policy in {rid}")
        signature_buckets = {}
        for class_id in viable_classes:
            signature = classes[int(class_id[1:])][1]
            signature_buckets.setdefault(signature[chosen - domain[0]], []).append(class_id)
        expected_partitions = [{"value": {"type": "int64", "value": val}, "classes": members}
                               for val, members in signature_buckets.items()]
        if step["partitions"] != expected_partitions:
            raise Invalid(f"partition mismatch in {rid}")
        largest = max(map(len, signature_buckets.values()))
        if step["largest_partition"] != largest: raise Invalid(f"largest partition mismatch in {rid}")
        answer = reference(control, member, chosen)
        if step["response"].get("type") != "int64" or step["response"]["value"] != answer:
            raise Invalid(f"oracle response mismatch in {rid}")
        before = compatible[:]
        counterexample = step.get("counterexample")
        expected_observation = {"query_id": f"q{step_index+1}", "observation": {
            "inputs": [chosen], "expected": {"type": "int64", "value": answer}}}
        observed_expected = (counterexample.get("observation", {}).get("expected", {})
                             if isinstance(counterexample, dict) else {})
        observed_inputs = (counterexample.get("observation", {}).get("inputs")
                           if isinstance(counterexample, dict) else None)
        if (not isinstance(observed_inputs, list) or len(observed_inputs) != 1 or
            type(observed_inputs[0]) is not int or type(observed_expected.get("value")) is not int):
            raise Invalid(f"counterexample has malformed integer fields in {rid}")
        if counterexample != expected_observation:
            raise Invalid(f"counterexample value is malformed or mismatched in {rid}")
        observed.append((chosen, answer)); selected_queries.append(chosen)
        compatible = [cid for cid in ids if all(evaluate(trees[ids.index(cid)], x) == y for x, y in observed)]
        eliminated = [cid for cid in before if cid not in compatible]
        classes_after = [class_id for class_id, _ in classes
                         if any(candidate_class[cid] == class_id for cid in compatible)]
        if step["eliminated_candidate_ids"] != eliminated or step["compatible_candidate_ids"] != compatible:
            raise Invalid(f"candidate elimination/compatibility mismatch in {rid}")
        if step["compatible_behavior_class_ids"] != classes_after:
            raise Invalid(f"behavior class compatibility mismatch in {rid}")
        if step["specification_identity"] != specification_identity(observed):
            raise Invalid(f"step specification identity mismatch in {rid}")
        if [e["candidate_id"] for e in step["validations"]] != ids:
            raise Invalid(f"step validation order mismatch in {rid}")
        for event, cid in zip(step["validations"], ids):
            check_validation_event(event, cid, trees[ids.index(cid)], observed, rid)
        viable_classes = classes_after
    if transcript["final_compatible_candidate_ids"] != compatible:
        raise Invalid(f"final candidate set mismatch in {rid}")
    classes_after = [class_id for class_id, _ in classes
                     if any(candidate_class[cid] == class_id for cid in compatible)]
    if transcript["final_behavior_class_ids"] != classes_after:
        raise Invalid(f"final behavior classes mismatch in {rid}")
    if transcript["stop_reason"] == "empty_compatible_set":
        expected_stop = "empty_compatible_set" if not compatible else None
    elif transcript["stop_reason"] == "one_finite_behavior_class":
        expected_stop = "one_finite_behavior_class" if len(classes_after) <= 1 and compatible else None
    elif transcript["stop_reason"] == "query_cap":
        expected_stop = "query_cap" if len(selected_queries) == 8 and len(classes_after) > 1 and compatible else None
    else:
        expected_stop = None
    if expected_stop != transcript["stop_reason"]:
        raise Invalid(f"stopping decision mismatch/incomplete run in {rid}")
    work = transcript["work"]
    if not isinstance(work, dict): raise Invalid(f"work counters malformed in {rid}")
    required_work = {"candidate_row_evaluations", "charged_work", "logical_storage_bytes",
                     "node_visits", "operations", "iterations", "oracle_queries",
                     "catalogue_candidates", "discovered_structures"}
    if not required_work.issubset(work): raise Invalid(f"work counters incomplete in {rid}")
    for key, value in work.items():
        require_nonnegative_int(value, f"work counter {key} in {rid}")
    if (work["candidate_row_evaluations"] > 1024 or work["charged_work"] > 100000 or
        work["logical_storage_bytes"] > 1024 * 1024 or work["node_visits"] > 100000 or
        work["operations"] > 100000 or work["iterations"] > 100000 or
        work["oracle_queries"] > 8 or work["catalogue_candidates"] != len(ids) or
        work["discovered_structures"] != 0):
        raise Invalid(f"resource accounting limit/structure mismatch in {rid}")
    if work["oracle_queries"] != len(transcript["steps"]):
        raise Invalid(f"oracle query count disagrees with transcript in {rid}")
    verify_revision_chain(record, transcript, ids, observed)
    return {"ids": ids, "trees": trees, "metrics": metrics, "compatible": compatible,
            "observed": observed, "queries": selected_queries, "classes": classes_after}

def score_record(record, transcript, verified, assessment_rows):
    rid, control, member = record["run_id"], record["control"], record["reference_member"]
    assessment = list(range(-17, -8)) + list(range(9, 18))
    if [x for x, _ in assessment_rows] != assessment:
        raise Invalid(f"assessment domain mismatch for {rid}")
    for x, y in assessment_rows:
        if reference(control, member, x) != y:
            raise Invalid(f"assessment label mismatch for {rid} at {x}")
    ids, trees, metrics = verified["ids"], verified["trees"], verified["metrics"]
    assessment_scores = {}
    training_exact = []
    for cid in verified["compatible"]:
        tree = trees[ids.index(cid)]
        train_ok = all(evaluate(tree, x) == reference(control, member, x) for x, _ in verified["observed"])
        assess_values = [evaluate(tree, x) for x, _ in assessment_rows]
        assess_exact = all(value == y for value, (_, y) in zip(assess_values, assessment_rows))
        assessment_scores[cid] = {"exact": assess_exact, "nodes": metrics[cid][0], "depth": metrics[cid][1]}
        if train_ok: training_exact.append(cid)
    assessment_exact = [cid for cid, score in assessment_scores.items() if score["exact"]]
    return {"run_id": rid, "control": control, "policy": transcript["policy"],
            "assessment_artifact": record.get("assessment_artifact", record.get("assessment_path")),
            "assessment_sha256": record.get("assessment_sha256"),
            "assessment_observation_count": len(assessment_rows),
            "runner_stop_reason": transcript["stop_reason"],
            "training_exact_candidate_ids": training_exact,
            "assessment_exact_candidate_ids": assessment_exact,
            "candidate_finite_assessment": assessment_scores,
            "finite_identifiability_only": transcript["stop_reason"] == "one_finite_behavior_class",
            "finite_certificate_scope": ("query-domain[-8,8]" if
                transcript["stop_reason"]=="one_finite_behavior_class" else None),
            "outside_catalogue_empty_set": transcript["stop_reason"] == "empty_compatible_set",
            "reference_catalogue_member_present": f"c{member}" in ids,
            "candidate_count": len(ids), "queried_inputs": verified["queries"],
            "work":{"selector_charged_work":record["selector_charged_work"],
                    "oracle_operations":record["oracle_operations"],
                    "combined_charged_work":record["combined_charged_work"],
                    "cpu_seconds":record["cpu_seconds"],"wall_seconds":record["wall_seconds"],
                    "individual_child_peak_rss_sum_bytes":record["individual_child_peak_rss_sum_bytes"],
                    "conservative_session_rss_sum_bytes":record["conservative_session_rss_sum_bytes"],
                    "group_memory_measurement":"outer bounded-program supervisor; session sum is not simultaneous PG RSS"}}

def complete_stop_outcomes(records,scores):
    positive_controls={"arithmetic","modular","threshold","conditional-modular"}
    return all((score["runner_stop_reason"]=="one_finite_behavior_class"
                if record["control"] in positive_controls or record["control"]=="ambiguity"
                else score["runner_stop_reason"]=="empty_compatible_set")
               for record,score in zip(records,scores))

def replay_transcript_gate(records, transcripts):
    """Authenticate frozen replay identities/transcripts before assessment access."""
    if len(records) != len(transcripts):
        return {"passed": False, "replay_count": 0, "failures": ["row_count_mismatch"]}
    by_id = {}
    for index, record in enumerate(records):
        run_id = record.get("run_id")
        if not isinstance(run_id, str) or not run_id or run_id in by_id:
            return {"passed": False, "replay_count": 0, "failures": ["invalid_or_duplicate_run_id"]}
        by_id[run_id] = index
    replays = [(index, record) for index, record in enumerate(records)
               if record.get("replay_of") is not None]
    failures = []
    expected_policies = {"ascending", "disagreement"}
    seen_policies = set()
    if len(replays) != 2:
        failures.append("expected_exactly_two_replays")

    def normalize_transcript(value):
        if not isinstance(value, dict):
            return None
        result = dict(value)
        result.pop("run_id", None)
        result.pop("seed_metadata", None)
        return result

    for replay_index, replay in replays:
        replay_id = replay.get("run_id")
        predecessor_index = by_id.get(replay.get("replay_of"))
        if predecessor_index is None:
            failures.append(f"{replay_id}:missing_predecessor")
            continue
        primary = records[predecessor_index]
        policy = replay.get("method", replay.get("policy"))
        primary_policy = primary.get("method", primary.get("policy"))
        replay_seed = replay.get("seed", replay.get("seed_metadata"))
        primary_seed = primary.get("seed", primary.get("seed_metadata"))
        aliases_conflict = any(
            ("method" in item and "policy" in item and item["method"] != item["policy"]) or
            ("seed" in item and "seed_metadata" in item and item["seed"] != item["seed_metadata"])
            for item in (primary, replay))
        if (primary.get("replay_of") is not None or replay.get("control") != "threshold" or
            replay.get("reference_member") != 0 or primary.get("control") != "threshold" or
            primary.get("reference_member") != 0 or replay.get("reference_candidate_id") != "c0" or
            primary.get("reference_candidate_id") != "c0" or policy not in expected_policies or
            primary_policy != policy or primary_seed != 0 or replay_seed != 1 or aliases_conflict):
            failures.append(f"{replay_id}:replay_identity_mismatch")
            continue
        if policy in seen_policies:
            failures.append(f"{replay_id}:duplicate_policy_replay")
            continue
        seen_policies.add(policy)
        identity_fields = ("control", "reference_member", "reference_candidate_id",
                           "training_artifact", "training_sha256", "assessment_sha256")
        primary_assessment_id = primary.get("assessment_artifact", primary.get("assessment_path"))
        replay_assessment_id = replay.get("assessment_artifact", replay.get("assessment_path"))
        if (not isinstance(primary.get("training_artifact"), str) or not primary["training_artifact"] or
            not isinstance(primary.get("training_sha256"), str) or
            not re.fullmatch(r"[0-9a-f]{64}", primary["training_sha256"]) or
            not isinstance(primary_assessment_id, str) or not primary_assessment_id or
            not isinstance(primary.get("assessment_sha256"), str) or
            not re.fullmatch(r"[0-9a-f]{64}", primary["assessment_sha256"]) or
            primary.get("assessment_sha256") != replay.get("assessment_sha256") or
            replay_assessment_id != primary_assessment_id or
            ("assessment_path" in replay or "assessment_path" in primary) and
                replay.get("assessment_path") != primary.get("assessment_path") or
            any(replay.get(key) != primary.get(key) for key in identity_fields)):
            failures.append(f"{replay_id}:frozen_input_identity_mismatch")
            continue
        # Some frozen manifests carry the checked observation count explicitly;
        # when present it is part of the replay's assessment identity.
        if (("assessment_observation_count" in replay or "assessment_observation_count" in primary) and
            replay.get("assessment_observation_count") != primary.get("assessment_observation_count")):
            failures.append(f"{replay_id}:assessment_count_mismatch")
            continue
        replay_transcript = normalize_transcript(transcripts[replay_index])
        primary_transcript = normalize_transcript(transcripts[predecessor_index])
        if replay_transcript is None or primary_transcript is None:
            failures.append(f"{replay_id}:missing_transcript")
            continue
        if (replay_transcript.get("stop_reason") != "one_finite_behavior_class" or
            primary_transcript.get("stop_reason") != "one_finite_behavior_class"):
            failures.append(f"{replay_id}:unresolved_outcome")
            continue
        if json.dumps(replay_transcript, sort_keys=True, separators=(",", ":")) != \
                json.dumps(primary_transcript, sort_keys=True, separators=(",", ":")):
            failures.append(f"{replay_id}:transcript_mismatch")
    if seen_policies != expected_policies:
        failures.append("replay_policy_coverage_mismatch")
    return {"passed": not failures, "replay_count": len(replays), "failures": failures}

def load_assessment_data_after_replay_gate(loaded, loader):
    """Call a label loader only after frozen transcript replay qualification."""
    records = [item[0] for item in loaded]
    transcripts = [item[1] for item in loaded]
    result = replay_transcript_gate(records, transcripts)
    if not result["passed"]:
        raise Invalid("deterministic replay failed before assessment access: " + ";".join(result["failures"]))
    return loader(loaded)

def deterministic_replay_assessment(records, transcripts, scores):
    """Check replayed scores/work after the pre-assessment transcript gate.

    Replay identity is deliberately narrow: only the run ID and seed metadata
    differ. The complete selector transcript (including decisions, identities,
    and deterministic work counters) must otherwise match byte-for-byte after
    canonical JSON serialization. Timing and process-resource measurements
    are not part of that transcript.
    """
    if not (len(records) == len(transcripts) == len(scores)):
        return {"passed": False, "replay_count": 0, "failures": ["row_count_mismatch"]}
    transcript_result = replay_transcript_gate(records, transcripts)
    if not transcript_result["passed"]:
        return transcript_result
    by_id = {}
    for index, record in enumerate(records):
        run_id = record.get("run_id")
        if not isinstance(run_id, str) or not run_id or run_id in by_id:
            return {"passed": False, "replay_count": 0, "failures": ["invalid_or_duplicate_run_id"]}
        by_id[run_id] = index

    replays = [(index, record) for index, record in enumerate(records)
               if record.get("replay_of") is not None]
    failures = []

    def deterministic_score(value):
        if not isinstance(value, dict):
            return None
        required = ("control", "policy", "assessment_artifact", "assessment_sha256",
                    "assessment_observation_count", "runner_stop_reason",
                    "training_exact_candidate_ids", "assessment_exact_candidate_ids",
                    "candidate_finite_assessment", "finite_identifiability_only",
                    "finite_certificate_scope", "outside_catalogue_empty_set",
                    "reference_catalogue_member_present", "candidate_count", "queried_inputs")
        if any(key not in value for key in required):
            return None
        work = value.get("work")
        if not isinstance(work, dict):
            return None
        deterministic_work = {key: work.get(key) for key in
                              ("selector_charged_work", "oracle_operations", "combined_charged_work")}
        if any(type(item) is not int or item < 0 for item in deterministic_work.values()):
            return None
        return {key: value[key] for key in required} | {
            "work": deterministic_work,
            "status": value.get("status"),
            "exact": value.get("exact"),
        }

    for replay_index, replay in replays:
        replay_id = replay.get("run_id")
        predecessor_id = replay.get("replay_of")
        predecessor_index = by_id.get(predecessor_id)
        if predecessor_index is None:
            failures.append(f"{replay_id}:missing_predecessor")
            continue
        primary = records[predecessor_index]
        policy = replay.get("policy")
        # Unresolved or invalid outcomes are never deterministic success.
        replay_score, primary_score = scores[replay_index], scores[predecessor_index]
        if any(not isinstance(row, dict) or row.get("status") != "scored" or
               # This replay is a positive threshold control: a capped or
               # otherwise unresolved selector outcome cannot qualify merely
               # because the same unresolved outcome repeated deterministically.
               row.get("runner_stop_reason") != "one_finite_behavior_class"
               for row in (replay_score, primary_score)):
            failures.append(f"{replay_id}:unresolved_outcome")
            continue
        replay_projection = deterministic_score(replay_score)
        primary_projection = deterministic_score(primary_score)
        if replay_projection is None or primary_projection is None:
            failures.append(f"{replay_id}:incomplete_deterministic_score")
            continue
        if replay_projection != primary_projection:
            failures.append(f"{replay_id}:decision_or_work_mismatch")

    return {"passed": not failures, "replay_count": len(replays), "failures": failures}

def summarize(records, transcripts, scores):
    primary = [(record, transcript, score) for record, transcript, score in
               zip(records, transcripts, scores) if record.get("replay_of") is None]
    means = {}
    for control in ("arithmetic", "modular", "threshold", "conditional-modular"):
        means[control] = {}
        for policy in ("ascending", "disagreement"):
            values = [len(transcript["steps"]) for record, transcript, _ in primary
                      if record["control"] == control and transcript["policy"] == policy]
            means[control][policy] = sum(values) / len(values) if values else None
    comparisons = {
        "modular_disagreement_strictly_fewer": (means["modular"]["disagreement"] is not None and
            means["modular"]["ascending"] is not None and
            means["modular"]["disagreement"] < means["modular"]["ascending"]),
        "threshold_disagreement_strictly_fewer": (means["threshold"]["disagreement"] is not None and
            means["threshold"]["ascending"] is not None and
            means["threshold"]["disagreement"] < means["threshold"]["ascending"]),
        "arithmetic_disagreement_no_higher": (means["arithmetic"]["disagreement"] is not None and
            means["arithmetic"]["ascending"] is not None and
            means["arithmetic"]["disagreement"] <= means["arithmetic"]["ascending"]),
        "conditional_disagreement_no_higher": (means["conditional-modular"]["disagreement"] is not None and
            means["conditional-modular"]["ascending"] is not None and
            means["conditional-modular"]["disagreement"] <= means["conditional-modular"]["ascending"]),
    }
    required_members = all(any(record["control"] == control and record["reference_member"] == member and
                               score["reference_catalogue_member_present"]
                               for record, _, score in primary)
                          for control in ("arithmetic", "modular", "threshold", "conditional-modular")
                          for member in range(4))
    unresolved_caps=sum(score.get("runner_stop_reason")=="query_cap" for _,_,score in primary)
    ambiguity_ok = all(score["runner_stop_reason"] == "one_finite_behavior_class" and
                       len(score["training_exact_candidate_ids"]) >= 2
                       for _, _, score in primary if score["control"] == "ambiguity")
    outside_ok = all(score["runner_stop_reason"] == "empty_compatible_set" and
                     not score["training_exact_candidate_ids"]
                     for _, _, score in primary if score["control"] == "outside")
    positive_controls={"arithmetic","modular","threshold","conditional-modular"}
    no_incomplete = complete_stop_outcomes(records,scores)
    zero_false_certificates=all(
        (score.get("finite_identifiability_only") is True and
         score.get("finite_certificate_scope")=="query-domain[-8,8]")
        if score.get("runner_stop_reason")=="one_finite_behavior_class"
        else (score.get("finite_identifiability_only") is False and score.get("finite_certificate_scope") is None)
        for score in scores)
    replay_assessment = deterministic_replay_assessment(records, transcripts, scores)
    gate = (all(comparisons.values()) and required_members and ambiguity_ok and outside_ok and
            no_incomplete and zero_false_certificates and replay_assessment["passed"])
    return {"mean_queries_by_control_policy": means, "criteria": comparisons,
            "all_positive_reference_members_present": required_members,
            "zero_false_finite_certificates": zero_false_certificates,
            "unresolved_query_cap_runs": unresolved_caps,
            "finite_ambiguity_outcome_correct": ambiguity_ok,
            "outside_catalogue_outcome_correct": outside_ok,
            "all_positive_controls_identified_one_finite_behavior_class": all(
                score["runner_stop_reason"]=="one_finite_behavior_class"
                for record,_,score in zip(records,transcripts,scores)
                if record["control"] in positive_controls),
            "no_incomplete_run_counted": no_incomplete,
            "deterministic_replays_passed": replay_assessment["passed"],
            "deterministic_replay_count": replay_assessment["replay_count"],
            "deterministic_replay_failures": replay_assessment["failures"],
            "acceptance_gate_passed": gate,
            "interpretation": "finite catalogue identifiability only; not synthesis recovery or global equivalence"}

def _load_json(path,label):
    try: return json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError,ValueError) as error: raise Invalid(f"cannot read {label}: {error}") from error

def create_json(path,value):
    path=Path(path)
    data=(json.dumps(value,sort_keys=True,separators=(",",":"))+"\n").encode("utf-8")
    try:
        with path.open("xb") as stream:
            stream.write(data); stream.flush()
    except OSError as error:
        raise Invalid(f"cannot create score output: {error}") from error

def _expand_index(index_value,index_path,records,kind):
    if kind=="candidate":
        schema="genetik-fdi-candidate-output-index-v1"; filename="candidate.json"
    else:
        schema="genetik-fdi-verified-receipt-index-v1"; filename="receipt.json"
    if not isinstance(index_value,dict) or index_value.get("schema")!=schema:
        raise Invalid(f"{kind} index schema invalid")
    entries=index_value.get("runs")
    if not isinstance(entries,list) or len(entries)!=len(records):
        raise Invalid(f"{kind} index does not cover the frozen run set")
    index_path=Path(index_path).resolve()
    expected_root=Path(str(index_path)+".evidence").resolve()
    result=[]
    for ordinal,(record,entry) in enumerate(zip(records,entries)):
        if not isinstance(entry,dict) or entry.get("run_id")!=record.get("run_id"):
            raise Invalid(f"{kind} index rows are missing, reordered, or misbound")
        relative=entry.get("path")
        expected=Path(index_path.name+".evidence")/f"run-{ordinal:02d}"/filename
        if not isinstance(relative,str) or Path(relative).is_absolute() or Path(relative)!=expected or ".." in Path(relative).parts:
            raise Invalid(f"{kind} index path is outside its canonical invocation slot")
        current=index_path.parent
        for part in Path(relative).parts:
            current=current/part
            if current.is_symlink(): raise Invalid(f"{kind} index path contains a symlink")
        path=(index_path.parent/relative).resolve(strict=True)
        if not path.is_relative_to(expected_root) or not path.is_file():
            raise Invalid(f"{kind} indexed artifact escapes invocation-owned root")
        if not isinstance(entry.get("sha256"),str) or hashlib.sha256(path.read_bytes()).hexdigest()!=entry["sha256"]:
            raise Invalid(f"{kind} indexed artifact hash mismatch")
        value=_load_json(path,f"per-run {kind}")
        if not isinstance(value,dict) or value.get("run_id")!=record.get("run_id"):
            raise Invalid(f"{kind} indexed artifact identity mismatch")
        result.append(value)
    return result

def load_sealed_assessments(manifest_path,loaded):
    """Open sealed files only after the caller has authenticated all 38 runs."""
    bundle=_load_json(manifest_path,"sealed assessment identity manifest")
    if not isinstance(bundle,dict) or bundle.get("schema")!="genetik-fdi-sealed-assessment-manifest-v1":
        raise Invalid("unsupported sealed assessment identity manifest")
    descriptors=bundle.get("artifacts")
    if not isinstance(descriptors,list): raise Invalid("assessment artifact mapping malformed")
    index={}
    for descriptor in descriptors:
        if not isinstance(descriptor,dict) or set(descriptor)!={"id","path","sha256"}:
            raise Invalid("assessment artifact descriptor malformed")
        if (not isinstance(descriptor["id"],str) or descriptor["id"] in index or
            not isinstance(descriptor["path"],str) or not Path(descriptor["path"]).is_absolute() or
            not isinstance(descriptor["sha256"],str) or not re.fullmatch(r"[0-9a-f]{64}",descriptor["sha256"])):
            raise Invalid("assessment artifact identity malformed or duplicated")
        index[descriptor["id"]]=descriptor
    expected={}
    for record,*_ in loaded:
        artifact_id,sha_value=record.get("assessment_artifact"),record.get("assessment_sha256")
        if not isinstance(artifact_id,str) or not isinstance(sha_value,str):
            raise Invalid("frozen run lacks an assessment artifact identity")
        if expected.setdefault(artifact_id,sha_value)!=sha_value:
            raise Invalid("one assessment artifact ID is associated with multiple hashes")
    if set(index)!=set(expected) or any(index[item]["sha256"]!=sha_value for item,sha_value in expected.items()):
        raise Invalid("sealed assessment manifest does not exactly match the frozen control mapping")
    result={}
    for record,*_ in loaded:
        descriptor=index[record["assessment_artifact"]]
        try: payload=Path(descriptor["path"]).read_bytes()
        except OSError as error: raise Invalid("sealed assessment artifact cannot be read") from error
        if hashlib.sha256(payload).hexdigest()!=record["assessment_sha256"]:
            raise Invalid(f"assessment payload hash mismatch for {record['run_id']}")
        try: lines=payload.decode("utf-8").splitlines()
        except UnicodeDecodeError as error: raise Invalid("assessment artifact is not UTF-8") from error
        if not lines or lines[0]!="genetik-fdi-assessment-v1":
            raise Invalid("assessment artifact header mismatch")
        rows=[]
        for line in lines[1:]:
            cols=line.split("\t")
            if len(cols)!=2: raise Invalid("assessment row malformed")
            try: x,y=map(int,cols)
            except ValueError as error: raise Invalid("assessment observation is not integer") from error
            if not I64_MIN<=x<=I64_MAX or not I64_MIN<=y<=I64_MAX:
                raise Invalid("assessment observation outside Int64")
            rows.append((x,y))
        result[record["run_id"]]=rows
    return result

def candidates_from_verified_receipts(receipts):
    result=[]
    for row in receipts:
        identity=row.get("candidate_identity") if isinstance(row,dict) else None
        if (not isinstance(identity,dict) or not isinstance(row.get("transcript"),dict) or
            row.get("verified") is not True or not isinstance(row.get("execution_receipt"),dict)):
            raise Invalid("verified receipt lacks candidate identity, transcript, or execution record")
        result.append({**identity,"run_manifest_sha256":row.get("run_manifest_sha256"),
            "training_sha256":row.get("training_sha256"),"completion_status":row.get("completion_status"),
            "report_validation_status":row.get("report_validation_status"),"transcript":row["transcript"],
            "receipt":row["execution_receipt"],"candidate_output_sha256":row.get("candidate_output_sha256")})
    return result

def score_synthetic_preflight(manifest,candidates,receipts,assessment_bundle_path,output_path,manifest_path,
                              derived_candidates=False):
    if (not isinstance(manifest,dict) or manifest.get("schema") not in {
            "genetik-fdi-run-manifest-v2","genetik-fdi-scorer-manifest-v2"} or
        manifest.get("mode")!="synthetic-preflight"):
        raise Invalid("synthetic scorer requires a synthetic-preflight manifest")
    records=manifest.get("runs")
    if (not isinstance(records,list) or not records or not isinstance(candidates,list) or
        not isinstance(receipts,list) or len(candidates)!=len(records) or len(receipts)!=len(records)):
        raise Invalid("synthetic output/receipt counts differ from run manifest")
    run_ids=[row.get("run_id") for row in records]
    if (any(not isinstance(rid,str) or not rid for rid in run_ids) or len(set(run_ids))!=len(run_ids) or
        [row.get("run_id") for row in candidates if isinstance(row,dict)]!=run_ids or
        [row.get("run_id") for row in receipts if isinstance(row,dict)]!=run_ids):
        raise Invalid("synthetic run identities are missing, duplicated, or reordered")
    controls={row.get("id"):row for row in manifest.get("controls",[]) if isinstance(row,dict)}
    if len(controls)!=len(manifest.get("controls",[])) or not controls:
        raise Invalid("synthetic scorer control map malformed")
    manifest_sha=sha256(Path(manifest_path))
    runner_manifest_sha=manifest.get("runner_manifest_sha256")
    if not isinstance(runner_manifest_sha,str) or not re.fullmatch(r"[0-9a-f]{64}",runner_manifest_sha):
        raise Invalid("scorer manifest must bind the exact training-only runner manifest SHA256")
    loaded=[]
    for record,candidate,receipt in zip(records,candidates,receipts):
        if candidate.get("schema")!="genetik-fdi-candidate-output-v2" or candidate.get("run_id")!=record["run_id"]:
            raise Invalid("synthetic candidate output identity mismatch")
        if (candidate.get("protocol_id")!=manifest.get("protocol_id") or
            candidate.get("run_manifest_sha256")!=runner_manifest_sha or
            any(candidate.get(key)!=record.get(key) for key in ("run_id","control_id","method","seed","training_artifact","training_sha256"))):
            raise Invalid("synthetic candidate differs from frozen runner manifest")
        if receipt.get("schema")!="genetik-fdi-verified-receipt-v2" or receipt.get("verified") is not True:
            raise Invalid("synthetic execution receipt unresolved")
        candidate_sha=hashlib.sha256(json.dumps(candidate,sort_keys=True,separators=(",",":")).encode()).hexdigest()
        hash_mismatch=(not re.fullmatch(r"[0-9a-f]{64}",str(receipt.get("candidate_output_sha256")))
                       if derived_candidates else receipt.get("candidate_output_sha256")!=candidate_sha)
        if (hash_mismatch or receipt.get("run_id")!=record["run_id"] or
            receipt.get("run_manifest_sha256")!=candidate.get("run_manifest_sha256")):
            raise Invalid("synthetic receipt is not bound to candidate output")
        if derived_candidates and (receipt.get("candidate_output_sha256")!=candidate.get("candidate_output_sha256") or
                receipt.get("candidate_identity")!={key:candidate.get(key) for key in (
                    "schema","protocol_id","run_id","control_id","method","seed","training_artifact",
                    "training_sha256","completion_status","report_validation_status")} or
                receipt.get("transcript")!=candidate.get("transcript")):
            raise Invalid("synthetic receipt-only candidate replay differs from verifier-authenticated fields")
        control=controls.get(record.get("control_id"))
        if not isinstance(control,dict) or control.get("catalogue") not in {"ambiguity","arithmetic"}:
            raise Invalid("synthetic control catalogue is outside frozen fixtures")
        training_path=Path(record.get("training_path",control.get("training",{}).get("path","")))
        training_sha=record.get("training_sha256",control.get("training",{}).get("sha256"))
        if not training_path.is_file() or sha256(training_path)!=training_sha:
            raise Invalid("synthetic training artifact missing or changed")
        transcript=candidate.get("transcript")
        if candidate.get("completion_status")!="completed" or transcript is None:
            raise Invalid("synthetic runner output incomplete")
        member=record.get("reference_member",control.get("reference_member",0))
        if type(member) is not int or member!=0: raise Invalid("synthetic reference member differs from frozen fixture")
        scorer_record={**record,"control":control["catalogue"],"reference_member":member,
            "reference_candidate_id":f"c{member}","training_path":str(training_path),"training_sha256":training_sha}
        verified=verify_transcript(scorer_record,transcript)
        require_synthetic_fixture_transcript(control["catalogue"],transcript)
        loaded.append((record,candidate,receipt,verified,control))
    # The bundle is opened only after all synthetic candidate/receipt/training
    # artifacts have been authenticated and their transcripts replayed.
    bundle=_load_json(assessment_bundle_path,"synthetic assessment bundle")
    if not isinstance(bundle,dict) or bundle.get("schema")!="genetik-fdi-assessment-bundle-v1":
        raise Invalid("synthetic assessment bundle schema invalid")
    assessment_index={}
    for item in bundle.get("artifacts",[]):
        if not isinstance(item,dict) or set(item)!={"id","sha256","content_base64"} or item["id"] in assessment_index:
            raise Invalid("synthetic assessment identity malformed or duplicate")
        assessment_index[item["id"]]=item
    scores=[]
    for record,candidate,receipt,verified,control in loaded:
        assessment=control.get("assessment",{})
        descriptor=assessment_index.get(assessment.get("id"))
        if not isinstance(descriptor,dict) or descriptor.get("sha256")!=assessment.get("sha256"):
            raise Invalid("synthetic scorer assessment mapping mismatch")
        try: data=base64.b64decode(descriptor["content_base64"],validate=True)
        except (ValueError,TypeError) as error: raise Invalid("synthetic assessment encoding invalid") from error
        if hashlib.sha256(data).hexdigest()!=assessment["sha256"]:
            raise Invalid("synthetic assessment content hash mismatch")
        lines=data.decode("utf-8").splitlines()
        if not lines or lines[0]!="genetik-fdi-assessment-v1": raise Invalid("synthetic assessment header invalid")
        rows=[]
        for line in lines[1:]:
            cols=line.split("\t")
            if len(cols)!=2: raise Invalid("synthetic assessment row malformed")
            try: x,y=map(int,cols)
            except ValueError as error: raise Invalid("synthetic assessment is not exact integer data") from error
            if not -(1<<63)<=x<(1<<63) or not -(1<<63)<=y<(1<<63): raise Invalid("synthetic assessment outside Int64")
            rows.append((x,y))
        assessment_by_candidate={}
        training_by_candidate={}
        exact=[]
        ids,trees=verified["ids"],verified["trees"]
        for cid in verified["compatible"]:
            tree=trees[ids.index(cid)]
            predictions=[evaluate(tree,x) for x,_ in rows]
            assessment_by_candidate[cid]=predictions
            training_by_candidate[cid]=[evaluate(tree,x) for x,_ in verified["observed"]]
            if predictions==[y for _,y in rows]: exact.append(cid)
        scores.append({"run_id":record["run_id"],"control_id":record["control_id"],
            "method":record["method"],"seed":record["seed"],"status":"scored",
            "training_exact_candidate_ids":verified["compatible"],
            "assessment_exact_candidate_ids":exact,
            "predictions":{cid:{"training":training_by_candidate[cid],
                                  "assessment":assessment_by_candidate[cid]}
                           for cid in assessment_by_candidate},
            "assessment_artifact":assessment["id"],"assessment_sha256":assessment["sha256"],
            "checked_observations":len(rows),"exact":bool(exact)})
    expected_by_control={"ambiguity":(["c0","c1","c2"],["c0","c1"],
                                         {"c0":{"training":[0],"assessment":[9]},
                                          "c1":{"training":[0],"assessment":[9]},
                                          "c2":{"training":[0],"assessment":[10]}}),
                         "arithmetic":(["c0","c1"],["c0","c1"],
                                         {"c0":{"training":[2,-6],"assessment":[11]},
                                          "c1":{"training":[2,-6],"assessment":[11]}})}
    expected_fixture=True
    for row in scores:
        catalogue=controls[row["control_id"]]["catalogue"]
        expected=expected_by_control.get(catalogue)
        if expected is None:
            expected_fixture=False; continue
        expected_training,expected_assessment,expected_predictions=expected
        expected_fixture=(expected_fixture and row["training_exact_candidate_ids"]==expected_training and
            row["assessment_exact_candidate_ids"]==expected_assessment and row["predictions"]==expected_predictions)
        if catalogue=="arithmetic":
            transcript=next(item[1]["transcript"] for item in loaded if item[0]["run_id"]==row["run_id"])
            # The arithmetic fixture must exercise authenticated adaptive refinement.
            steps=transcript.get("steps",[])
            expected_fixture=(expected_fixture and len(steps)==1 and steps[0].get("input")==-8 and
                steps[0].get("query_id")=="q1" and steps[0].get("response",{}).get("type")=="int64" and
                steps[0].get("response",{}).get("value")==-6 and type(steps[0].get("revision")) is int and
                steps[0].get("revision")==1)
    summary={"synthetic_preflight":True,"run_count":len(scores),"scored_run_count":len(scores),
        "all_scored":all(row["status"]=="scored" for row in scores),
        "all_exact":all(row["exact"] is True for row in scores),
        "acceptance_gate_passed":bool(scores) and expected_fixture and all(row["exact"] is True for row in scores),
        "interpretation":"synthetic pipeline qualification only; not scientific evidence"}
    create_json(Path(output_path),{"schema":"genetik-fdi-scores-v2","protocol_id":manifest["protocol_id"],
        "run_manifest_sha256":manifest_sha,"summary":summary,"scores":scores})

def require_synthetic_fixture_transcript(catalogue,transcript):
    if catalogue=="ambiguity":
        if transcript.get("stop_reason")!="one_finite_behavior_class" or transcript.get("steps"):
            raise Invalid("ambiguity synthetic fixture must resolve without oracle queries")
        return
    if catalogue=="arithmetic":
        steps=transcript.get("steps",[])
        if (transcript.get("stop_reason")!="one_finite_behavior_class" or len(steps)!=1 or
            steps[0].get("input")!=-8 or steps[0].get("response",{}).get("value")!=-6 or
            type(steps[0].get("revision")) is not int or steps[0].get("revision")!=1 or
            steps[0].get("query_id")!="q1"):
            raise Invalid("arithmetic fixture failed its frozen adaptive refinement case")
        return
    raise Invalid("unsupported synthetic fixture catalogue")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-manifest", required=True)
    parser.add_argument("--candidate-outputs")
    parser.add_argument("--verified-receipts", required=True)
    parser.add_argument("--assessment-bundle", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    manifest=_load_json(args.run_manifest,"run manifest")
    receipts=_load_json(args.verified_receipts,"verified receipt index")
    records=manifest.get("runs") if isinstance(manifest,dict) else None
    if not isinstance(records,list): raise Invalid("run/scorer manifest has no ordered run records")
    receipts=_expand_index(receipts,args.verified_receipts,records,"receipt")
    derived_candidates=args.candidate_outputs is None
    if not derived_candidates:
        candidate_index=_load_json(args.candidate_outputs,"candidate output index")
        candidates=_expand_index(candidate_index,args.candidate_outputs,records,"candidate")
    else:
        candidates=candidates_from_verified_receipts(receipts)
    if isinstance(manifest,dict) and manifest.get("mode")=="synthetic-preflight":
        return score_synthetic_preflight(manifest,candidates,receipts,args.assessment_bundle,args.output,args.run_manifest,
            derived_candidates=derived_candidates)
    if (not isinstance(manifest,dict) or manifest.get("schema") not in {
            "genetik-fdi-run-manifest-v2","genetik-fdi-scorer-manifest-v2"} or
        manifest.get("mode") not in {"campaign","campaign-scoring"}):
        raise Invalid("scorer requires a frozen campaign manifest")
    records=manifest.get("runs")
    if not isinstance(records,list) or len(records)!=38:
        raise Invalid("final scoring requires all 38 frozen run records")
    expected_ids=[row.get("run_id") for row in records]
    if any(not isinstance(value,str) or not value for value in expected_ids) or len(set(expected_ids))!=38:
        raise Invalid("run IDs must be unique nonempty strings")
    if not isinstance(candidates,list) or not isinstance(receipts,list) or len(candidates)!=38 or len(receipts)!=38:
        raise Invalid("candidate/receipt artifacts do not cover the complete run set")
    if [row.get("run_id") for row in candidates if isinstance(row,dict)]!=expected_ids:
        raise Invalid("candidate outputs are missing, duplicated, or reordered")
    if [row.get("run_id") for row in receipts if isinstance(row,dict)]!=expected_ids:
        raise Invalid("verified receipts are missing, duplicated, or reordered")
    output=Path(args.output)
    if output.exists(): raise Invalid("scorer output must be create-exclusive")
    loaded=[]
    # First independently validate every selection transcript, receipt, and
    # training artifact. No assessment payload is decoded before this completes.
    for record,candidate,receipt_row in zip(records,candidates,receipts):
        private=record.get("peer_private")
        if not isinstance(private,dict) or set(private)!={"control","reference_member"}:
            raise Invalid(f"private scoring identity malformed for {record.get('run_id')}")
        if type(private["reference_member"]) is not int or private["reference_member"] not in range(4):
            raise Invalid("reference member is malformed")
        if candidate.get("schema")!="genetik-fdi-candidate-output-v2" or candidate.get("run_id")!=record["run_id"]:
            raise Invalid(f"candidate output identity mismatch for {record['run_id']}")
        if receipt_row.get("schema")!="genetik-fdi-verified-receipt-v2" or receipt_row.get("verified") is not True:
            raise Invalid(f"execution receipt is unresolved for {record['run_id']}")
        if (receipt_row.get("run_id")!=record["run_id"] or
            receipt_row.get("run_manifest_sha256")!=candidate.get("run_manifest_sha256") or
            (not derived_candidates and receipt_row.get("candidate_output_sha256")!=hashlib.sha256(
                json.dumps(candidate,sort_keys=True,separators=(",",":")).encode()).hexdigest()) or
            (derived_candidates and not re.fullmatch(r"[0-9a-f]{64}",str(receipt_row.get("candidate_output_sha256"))))):
            raise Invalid(f"receipt/candidate binding mismatch for {record['run_id']}")
        if (candidate.get("training_sha256")!=record.get("training_sha256") or
            receipt_row.get("training_sha256")!=record.get("training_sha256")):
            raise Invalid(f"training identity mismatch for {record['run_id']}")
        expected_runner_manifest_sha=record.get("runner_manifest_sha256")
        if expected_runner_manifest_sha is not None and candidate.get("run_manifest_sha256")!=expected_runner_manifest_sha:
            raise Invalid(f"candidate output is bound to the wrong runner manifest for {record['run_id']}")
        training_path=Path(record.get("training_path",""))
        if not training_path.is_file() or sha256(training_path)!=record.get("training_sha256"):
            raise Invalid(f"training artifact missing/hash mismatch for {record['run_id']}")
        transcript=candidate.get("transcript")
        if transcript is None or candidate.get("completion_status")!="completed":
            raise Invalid(f"incomplete candidate output for {record['run_id']}")
        scorer_record={**record,"control":private["control"],"reference_member":private["reference_member"],
            "reference_candidate_id":f"c{private['reference_member']}","training_path":str(training_path)}
        if transcript.get("policy") not in {"ascending","disagreement"}:
            raise Invalid(f"invalid transcript policy for {record['run_id']}")
        verified=verify_transcript(scorer_record,transcript)
        loaded.append((scorer_record,transcript,verified,candidate,receipt_row))
    # Only now may the scorer decode/access assessment observations. Replay
    # transcripts and their frozen training/assessment identities are checked
    # before the loader is called; deterministic scored outcomes/work are
    # compared again in summarize() after independent scoring.
    assessment_data=load_assessment_data_after_replay_gate(
        loaded, lambda rows: load_sealed_assessments(args.assessment_bundle,rows))
    scores=[]
    for record,transcript,verified,_,_ in loaded:
        score=score_record(record,transcript,verified,assessment_data[record["run_id"]])
        score.update({key:record[key] for key in ("control_id","method","seed")})
        score["status"]="scored"
        score["exact"]=bool(score.get("assessment_exact_candidate_ids"))
        scores.append(score)
    summary=summarize([item[0] for item in loaded],[item[1] for item in loaded],scores)
    summary["run_count"]=len(scores)
    summary["scored_run_count"]=sum(score.get("status") == "scored" for score in scores)
    summary["all_scored"]=summary["scored_run_count"] == len(records)
    create_json(output,{"schema":"genetik-fdi-scores-v2","protocol_id":manifest["protocol_id"],
        "run_manifest_sha256":sha256(Path(args.run_manifest)),"summary":summary,"scores":scores})

if __name__ == "__main__":
    try: main()
    except (Invalid, KeyError, ValueError, OSError) as error:
        raise SystemExit(f"post-selection scoring failed: {error}")
