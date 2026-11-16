#!/usr/bin/env python3
"""Deterministic, application-only prime successor benchmark."""
import argparse
import ast
import csv
import hashlib
import json
import math
import re
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent
DATA = ROOT / "prime_successor_1000_50000.csv"
FROZEN_DATASET_SHA256 = "b9d6565dc78ca52d4bf9c2a25860d934f8395fa338ba8d848cd1031e5e181018"
PARTITIONS = (("training", 1000, 30000), ("validation", 30000, 40000), ("test", 40000, 50000))
MAX_PREDICTED_INTEGER = 100000


def is_prime_trial(n):
    """Independent verifier: trial division, separate from the reference sieve."""
    if n < 2:
        return False
    if n % 2 == 0:
        return n == 2
    d = 3
    while d * d <= n:
        if n % d == 0:
            return False
        d += 2
    return True


def sieve(limit):
    flags = bytearray(b"\x01") * (limit + 1)
    flags[0:2] = b"\x00\x00"
    for n in range(2, math.isqrt(limit) + 1):
        if flags[n]:
            flags[n*n:limit+1:n] = b"\x00" * (((limit - n*n) // n) + 1)
    return [n for n, flag in enumerate(flags) if flag]


def generate_dataset(path=DATA):
    # Generate the full reference using a sieve, then independently verify the
    # full prime sequence through the needed successor and every adjacency.
    reference = sieve(50021)
    independent = [n for n in range(2, 50022) if is_prime_trial(n)]
    if reference != independent:
        raise RuntimeError("sieve and independent trial-division prime sequences differ")
    inputs = [p for p in reference if 1000 <= p < 50000]
    if len(inputs) != 4965:
        raise RuntimeError(f"unexpected input count: {len(inputs)}")
    rows = []
    for i,p in enumerate(inputs):
        q = inputs[i+1] if i+1 < len(inputs) else next(x for x in reference if x > p)
        if not is_prime_trial(q) or not all(not is_prime_trial(n) for n in range(p + 1, q)):
            raise RuntimeError(f"independent successor verification failed at {p}")
        rows.append((p, q, q-p))
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="ascii") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(("p", "successor", "gap"))
        w.writerows(rows)
    raw = path.read_bytes()
    return {"dataset": path.name, "sha256": hashlib.sha256(raw).hexdigest(), "rows": len(rows),
            "first": rows[0], "last": rows[-1], "input_interval": "[1000,50000)",
            "successor_outside_interval": rows[-1][1]}


def load_dataset(path=DATA):
    content = Path(path).read_bytes()
    digest=hashlib.sha256(content).hexdigest()
    if digest != FROZEN_DATASET_SHA256:
        raise ValueError(f"dataset SHA-256 mismatch: {digest}")
    parsed=list(csv.reader(content.decode("ascii").splitlines()))
    if not parsed or parsed[0] != ["p","successor","gap"]:
        raise ValueError("dataset header mismatch")
    rows = [(int(a), int(b), int(c)) for a,b,c in parsed[1:]]
    if len(rows) != 4965 or rows[0][0] < 1000 or rows[-1][0] >= 50000:
        raise ValueError("dataset identity/count/range mismatch")
    if any(q-p != g for p,q,g in rows) or any(rows[i][0] >= rows[i+1][0] for i in range(len(rows)-1)):
        raise ValueError("dataset structure mismatch")
    return rows, digest


def validate_expr(source):
    tree = ast.parse(source, mode="eval")
    allowed_bin = (ast.Add, ast.Sub, ast.Mult, ast.Div, ast.FloorDiv, ast.Mod, ast.Pow)
    allowed_unary = (ast.UAdd, ast.USub)
    nodes = list(ast.walk(tree))
    if len(nodes) > 32:
        raise ValueError("predictive expression exceeds 32 AST nodes")
    parents={child: parent for parent in ast.walk(tree) for child in ast.iter_child_nodes(parent)}
    for n in nodes:
        if isinstance(n, ast.Name) and n.id not in ("p","log"):
            raise ValueError("only p is an allowed variable")
        if isinstance(n, ast.Name) and n.id=="log" and not (isinstance(parents.get(n),ast.Call) and parents[n].func is n):
            raise ValueError("log may appear only as a function")
        if isinstance(n, ast.Call):
            if not isinstance(n.func, ast.Name) or n.func.id != "log" or len(n.args) != 1 or n.keywords:
                raise ValueError("only the one-argument natural log function is allowed")
        elif isinstance(n, ast.BinOp):
            if not isinstance(n.op, allowed_bin): raise ValueError("unsupported binary operation")
        elif isinstance(n, ast.UnaryOp):
            if not isinstance(n.op, allowed_unary): raise ValueError("unsupported unary operation")
        elif isinstance(n, (ast.Expression, ast.Load, ast.Name, ast.Constant, ast.Add, ast.Sub,
                            ast.Mult, ast.Div, ast.FloorDiv, ast.Mod, ast.Pow, ast.UAdd, ast.USub)):
            if isinstance(n, ast.Constant) and (type(n.value) not in (int,float) or not math.isfinite(n.value)):
                raise ValueError("only finite numeric constants are allowed")
        elif not isinstance(n, ast.Call):
            raise ValueError(f"unsupported expression node: {type(n).__name__}")
    return tree, len([n for n in nodes if isinstance(n,(ast.BinOp,ast.UnaryOp,ast.Call))])


def evaluate_expr(node, p):
    if isinstance(node, ast.Expression): return evaluate_expr(node.body,p)
    if isinstance(node, ast.Constant): return float(node.value), 0
    if isinstance(node, ast.Name): return float(p), 0
    if isinstance(node, ast.UnaryOp):
        x,w=evaluate_expr(node.operand,p); return (+x if isinstance(node.op,ast.UAdd) else -x), w+1
    if isinstance(node, ast.BinOp):
        a,wa=evaluate_expr(node.left,p); b,wb=evaluate_expr(node.right,p)
        if isinstance(node.op,ast.Add): out=a+b
        elif isinstance(node.op,ast.Sub): out=a-b
        elif isinstance(node.op,ast.Mult): out=a*b
        elif isinstance(node.op,ast.Div): out=a/b
        elif isinstance(node.op,ast.FloorDiv): out=a//b
        elif isinstance(node.op,ast.Mod): out=a%b
        else:
            if abs(b)>16: raise ValueError("exponent magnitude is capped at 16")
            out=a**b
        return out,wa+wb+1
    if isinstance(node,ast.Call):
        x,w=evaluate_expr(node.args[0],p)
        return math.log(x),w+1
    raise ValueError("internal unsupported expression")


def round_half_away(value):
    return math.floor(value + 0.5) if value >= 0 else math.ceil(value - 0.5)


def next_prime(p):
    """Exact computational reference. Returns result and trial-division count."""
    n = p + 1
    work = 0
    while True:
        if n < 2: prime = False
        elif n == 2: prime = True
        elif n % 2 == 0: prime = False; work += 1
        else:
            prime = True
            d = 3
            while d*d <= n:
                work += 1
                if n % d == 0:
                    prime = False
                    break
                d += 2
        if prime: return n, work
        n += 1


def parse_algorithmic(source):
    if source == "exact_next_prime":
        return "exact", None
    match = re.fullmatch(r"bounded_trial_scan\(max_gap=(\d+),max_divisor=(\d+)\)", source)
    if not match:
        raise ValueError("algorithmic candidate must be exact_next_prime or bounded_trial_scan(max_gap=N,max_divisor=N)")
    gap, divisor = map(int, match.groups())
    if not 1 <= gap <= 512 or not 2 <= divisor <= 10000:
        raise ValueError("algorithmic bounds require 1<=max_gap<=512 and 2<=max_divisor<=10000")
    return "bounded_scan", (gap, divisor)


def bounded_trial_scan(p, max_gap, max_divisor):
    """Bounded candidate iteration with explicit divisibility checks."""
    work = 0
    for step in range(1, max_gap + 1):
        candidate = p + step
        work += 1
        limit = math.isqrt(candidate)
        if limit > max_divisor:
            continue
        prime = True
        for divisor in range(2, limit + 1):
            work += 1
            if candidate % divisor == 0:
                prime = False
                break
        if prime:
            return candidate, work
    return float("nan"), work


def metrics(rows, outputs, works, elapsed_ns):
    n=len(rows); exact=sum(y==q for (_,q,_),y in zip(rows,outputs))
    valid=sum(isinstance(y,int) and is_prime_trial(y) for y in outputs)
    errors=[abs((y if isinstance(y,(int,float)) else float("nan"))-p2) for (_,p2,_) ,y in zip(rows,outputs)]
    gap_errors=[abs(((y-p) if isinstance(y,(int,float)) else float("nan"))-g) for (p,_,g),y in zip(rows,outputs)]
    def safe_mean(xs):
        vals=[x for x in xs if math.isfinite(x)]
        return (sum(vals)/n) if len(vals)==n else None
    return {"queries":n,"exact_successor_accuracy":exact/n,"exact_successors":exact,
            "exact_gap_accuracy":sum((y-p)==g for (p,_,g),y in zip(rows,outputs) if isinstance(y,(int,float)))/n,
            "gap_mae":safe_mean(gap_errors),"prime_output_validity":valid/n,
            "runtime_ns_total":elapsed_ns,"runtime_ns_per_query":elapsed_ns/n,
            "mathematical_work_total":sum(works),"mathematical_work_per_query":sum(works)/n}


def predict(rows, kind, expression=None, constant_gap=None, algorithmic_limits=None):
    tree=None; complexity=1
    if kind=="predictive": tree,complexity=validate_expr(expression)
    outputs=[];works=[];start=time.perf_counter_ns()
    for p,_,_ in rows:
        if kind=="predictive":
            try:
                val,w=evaluate_expr(tree,p)
                rounded=round_half_away(val) if math.isfinite(val) else float("nan")
                outputs.append(rounded if isinstance(rounded,int) and 0<=rounded<=MAX_PREDICTED_INTEGER else float("nan"));works.append(w)
            except (ArithmeticError,ValueError,OverflowError):
                outputs.append(float("nan"));works.append(complexity if isinstance(complexity,int) else 0)
        elif kind=="constant_gap": outputs.append(p+constant_gap);works.append(1)
        elif kind=="p_plus_2": outputs.append(p+2);works.append(1)
        elif kind=="p_plus_6": outputs.append(p+6);works.append(1)
        elif kind=="log_gap": outputs.append(p+max(1,round(math.log(p))));works.append(2)
        elif kind=="exact":
            q,w=next_prime(p);outputs.append(q);works.append(w)
        elif kind=="bounded_scan":
            q,w=bounded_trial_scan(p,*algorithmic_limits);outputs.append(q);works.append(w)
    elapsed=time.perf_counter_ns()-start
    return outputs,works,elapsed,complexity


def score_candidate(rows, kind, expression=None, constant_gap=None, algorithmic_limits=None):
    complexity=validate_expr(expression)[1] if kind=="predictive" else ("bounded algorithm" if kind in ("exact","bounded_scan") else 1)
    scored={}
    for name,lo,hi in PARTITIONS:
        subset=[r for r in rows if lo<=r[0]<hi]
        outputs,works,elapsed,_=predict(subset,kind,expression,constant_gap,algorithmic_limits)
        scored[name]=metrics(subset,outputs,works,elapsed)
    scored["expression_complexity_nodes"]=complexity
    scored["recursive_successor_generation"]=recursive_diagnostic(rows,kind,expression,constant_gap,algorithmic_limits)
    return scored


def recursive_diagnostic(rows,kind,expression=None,constant_gap=None,algorithmic_limits=None):
    # Repeatedly feed each predicted successor back as the next input. This is
    # diagnostic only; it never contributes to model selection or split metrics.
    tree=validate_expr(expression)[0] if kind=="predictive" else None
    p=rows[0][0]; expected=[r[0] for r in rows[1:]]+[rows[-1][1]]; matched=0
    for q in expected:
        if kind=="predictive":
            try:
                val,_=evaluate_expr(tree,p); out=round_half_away(val)
                if not isinstance(out,int) or not 0<=out<=MAX_PREDICTED_INTEGER: break
            except (ArithmeticError,ValueError,OverflowError):
                break
        elif kind=="constant_gap": out=p+constant_gap
        elif kind=="p_plus_2": out=p+2
        elif kind=="p_plus_6": out=p+6
        elif kind=="log_gap": out=p+max(1,round(math.log(p)))
        elif kind=="exact": out=next_prime(p)[0]
        elif kind=="bounded_scan":
            out,_=bounded_trial_scan(p,*algorithmic_limits)
            if not isinstance(out,int): break
        if out != q: break
        matched += 1;p=out
    return {"seed":rows[0][0],"successive_steps":len(expected),"correct_prefix_steps":matched,"complete":matched==len(expected)}


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--generate",action="store_true",help="regenerate frozen CSV and manifest")
    ap.add_argument("--candidate",help="frozen expression for the selected track; predictive syntax uses p and log(p)")
    ap.add_argument("--track",choices=("predictive","algorithmic"),default="predictive")
    ap.add_argument("--dataset",type=Path,default=DATA)
    ap.add_argument("--output",type=Path,help="write canonical JSON report (default stdout)")
    args=ap.parse_args()
    if args.generate:
        manifest=generate_dataset(args.dataset)
        args.dataset.with_suffix(".manifest.json").write_text(json.dumps(manifest,indent=2)+"\n")
        print(json.dumps(manifest,indent=2));return
    rows,digest=load_dataset(args.dataset)
    train=[r for r in rows if 1000<=r[0]<30000]
    train_mean=sum(r[2] for r in train)/len(train)
    fitted_gap=math.floor(train_mean+0.5)
    candidates={"p_plus_2":("predictive","p + 2"),"p_plus_6":("predictive","p + 6"),
                "train_fitted_constant_gap":("predictive",f"p + {fitted_gap}"),
                "logarithmic_gap_round_log_p":("predictive","p + log(p)"),
                "exact_next_prime_computational_reference":("exact",None)}
    report={"protocol":"genetik-prime-successor-benchmark-v1","dataset_sha256":digest,
            "dataset_rows":len(rows),"train_fitted_constant_gap":fitted_gap,
            "partition_counts":{name:sum(lo<=p<hi for p,_,_ in rows) for name,lo,hi in PARTITIONS},
            "baselines":{}}
    for name,spec in candidates.items():
        kind,expression=spec
        report["baselines"][name]=score_candidate(rows,kind,expression)
    if args.candidate is not None:
        if args.track=="predictive": kind="predictive"; expression=args.candidate
        else:
            kind,algorithmic_limits=parse_algorithmic(args.candidate);expression=None
        candidate_configuration=(
            {"ast_node_limit":32,"rounding":"nearest-ties-away","predicted_integer_range":[0,MAX_PREDICTED_INTEGER]}
            if args.track=="predictive" else {"algorithmic_bounds":algorithmic_limits}
        )
        candidate_identity={"protocol":"genetik-prime-successor-benchmark-v1","track":args.track,
                            "expression":args.candidate,"configuration":candidate_configuration}
        identity_bytes=json.dumps(candidate_identity,sort_keys=True,separators=(",",":")).encode("utf-8")
        report["candidate"]={"track":args.track,"expression":args.candidate,
                              "expression_sha256":hashlib.sha256(args.candidate.encode("utf-8")).hexdigest(),
                              "candidate_identity_sha256":hashlib.sha256(identity_bytes).hexdigest(),
                              "complexity_definition":"AST operation count" if args.track=="predictive" else "bounded algorithm, explicit gap and divisor bounds",
                              "configuration":candidate_configuration,
                              "algorithmic_bounds":algorithmic_limits if args.track=="algorithmic" else None,
                              "metrics":score_candidate(rows,kind,expression,algorithmic_limits=algorithmic_limits if args.track=="algorithmic" else None)}
    text=json.dumps(report,sort_keys=True,indent=2,allow_nan=False)+"\n"
    if args.output: args.output.write_text(text)
    else: print(text,end="")


if __name__=="__main__": main()
