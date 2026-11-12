#!/usr/bin/env python3
"""Run-scoped exact response peer for the finite-domain selector.

The peer receives only adaptive query frames. Its control/member arguments are
kept out of the producer process and are used solely to answer those queries.
Assessment data is neither loaded nor accepted by this program.
"""
from __future__ import annotations

import argparse
import json
import sys


class PeerError(ValueError):
    pass


def oracle(control: str, member: int, n: int) -> tuple[int, int]:
    """Return the frozen reference value and charged reference-operation count."""
    if control == "arithmetic":
        if member == 0: return n + 2, 1
        if member == 1: return 2 * n + 2, 2
        if member == 2: return n * n + 2, 2
        if member == 3: return 2, 0
    if control == "modular":
        if member == 0: return n % 3, 1
        if member == 1: return n % 5, 1
        if member == 2: return (n % 3) % 2, 2
        if member == 3: return n % 2, 1
    if control == "threshold":
        if member == 0: return abs(n), 2 if n < 0 else 1
        if member == 1: return n, 0
        if member == 2: return (abs(n), 2) if n < -1 else (n, 1)
        if member == 3: return (abs(n) + 1, 3) if n < 0 else (n, 1)
    if control == "conditional-modular":
        predicate = n % 3 == 1
        if member == 0: return (n + 2, 3) if predicate else (n * n - 2, 4)
        if member == 1: return (n + 3, 3) if predicate else (n * n - 2, 4)
        if member == 2: return (2 * n, 3) if predicate else (n * n - 2, 4)
        if member == 3: return n * n - 2, 2
    if control == "ambiguity":
        if member == 0: return n, 0
    if control == "outside":
        if member == 0: return ((1 << 40) if n == 0 else n), int(n == 0)
    raise PeerError("unsupported control")


def parse_request(line: str) -> tuple[str, int]:
    try:
        request = json.loads(line)
    except (ValueError, json.JSONDecodeError) as error:
        raise PeerError("malformed query JSON") from error
    if not isinstance(request, dict) or set(request) != {"type", "id", "payload"}:
        raise PeerError("query frame has an unexpected schema")
    if request["type"] != "query" or not isinstance(request["id"], str) or not request["id"]:
        raise PeerError("query type/id is invalid")
    payload = request["payload"]
    if not isinstance(payload, dict) or set(payload) != {"input"}:
        raise PeerError("query payload is malformed")
    value = payload["input"]
    if type(value) is not int or not -8 <= value <= 8:
        raise PeerError("query input is outside the frozen finite domain")
    return request["id"], value


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--control", required=True,
                        choices=("arithmetic", "modular", "threshold", "conditional-modular",
                                 "ambiguity", "outside"))
    parser.add_argument("--reference-member", required=True, type=int, choices=range(4))
    args = parser.parse_args()
    for line in sys.stdin:
        if not line.endswith("\n"):
            raise PeerError("unterminated query frame")
        query_id, n = parse_request(line[:-1])
        value, _ = oracle(args.control, args.reference_member, n)
        print(json.dumps({"type": "response", "id": query_id, "value": value},
                         separators=(",", ":")), flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except PeerError as error:
        print(f"oracle peer: {error}", file=sys.stderr)
        raise SystemExit(2)
