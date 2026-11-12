"""Deterministic protocol tests run through the governed process supervisor."""
from __future__ import annotations

import json
import hashlib
import base64
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"
sys.path.insert(0, str(SCRIPTS))
import bounded_interactive_session as interactive_session


class BoundedInteractiveSession(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="genetik-interactive-test-")
        self.root = Path(self.temp.name)
        self.requester = self.root / "requester.py"
        self.responder = self.root / "responder.py"

    def tearDown(self):
        self.temp.cleanup()

    def execute(self, requester: str, responder: str, *, wall=2.0,
                max_frame=4096, max_bytes=65536, max_requests=8,
                max_terminal_frame=None, identity_override=None):
        self.requester.write_text(requester)
        self.responder.write_text(responder)
        def file_sha(path):
            return hashlib.sha256(Path(path).read_bytes()).hexdigest()
        def identity(path):
            return {"executable": sys.executable, "sha256": file_sha(sys.executable),
                    "runtime_files": [{"path": str(path), "sha256": file_sha(path)}]}
        config = {
            "request_argv": [sys.executable, str(self.requester)],
            "response_argv": [sys.executable, str(self.responder)],
            "cwd": str(self.root), "environment": {"PATH": os.environ.get("PATH", "/usr/bin:/bin"),
                                                       "PYTHONPATH": str(SCRIPTS), "LC_ALL": "C",
                                                       "PYTHONDONTWRITEBYTECODE": "1"},
            "request_identity": identity(self.requester),
            "response_identity": identity(self.responder),
            "wall_seconds": wall, "max_frame_bytes": max_frame,
            "max_protocol_bytes": max_bytes, "max_requests": max_requests,
            "max_cpu_seconds": 1.25, "max_memory_bytes": 256 * 1024 * 1024,
        }
        if max_terminal_frame is not None:
            config["max_terminal_frame_bytes"] = max_terminal_frame
        if identity_override is not None:
            config["request_identity"] = identity_override
        config_path = self.root / "session.json"
        config_path.write_text(json.dumps(config))
        wrapper = (
            "import json,sys; sys.path.insert(0," + repr(str(SCRIPTS)) + "); "
            "from bounded_interactive_session import run_session; "
            "c=json.load(open(sys.argv[1])); "
            "r=run_session(c['request_argv'],c['response_argv'],cwd=__import__('pathlib').Path(c['cwd']),"
            "environment=c['environment'],wall_seconds=c['wall_seconds'],max_frame_bytes=c['max_frame_bytes'],"
            "max_protocol_bytes=c['max_protocol_bytes'],max_requests=c['max_requests'],"
            "max_terminal_frame_bytes=c.get('max_terminal_frame_bytes'),"
            "request_identity=c['request_identity'],response_identity=c['response_identity'],"
            "max_cpu_seconds=c['max_cpu_seconds'],max_memory_bytes=c['max_memory_bytes']); "
            "print(json.dumps(r,sort_keys=True,separators=(',',':'))); "
            "sys.exit(0 if r['status']=='completed' else 3)"
        )
        from bounded_process import run_bounded_process
        limits = {"max_cpu_seconds": 1.5, "max_wall_seconds": wall + 0.5,
                  "max_address_space_bytes": 256 * 1024 * 1024,
                  "max_stdin_bytes": 16, "max_output_bytes": max_bytes * 4 + 8192,
                  "max_file_bytes": 1024 * 1024}
        result = run_bounded_process([sys.executable, "-c", wrapper, str(config_path)], b"",
                                     config["environment"], self.root, limits)
        if result["stdout"]:
            result["session"] = json.loads(result["stdout"].splitlines()[0])
        return result

    def test_fragmented_request_and_response_and_lazy_terminal(self):
        result = self.execute(
            "import os,time,sys\n"
            "os.write(1,b'{\\\"type\\\":'); time.sleep(.01)\n"
            "os.write(1,b'\\\"query\\\",\\\"id\\\":\\\"q1\\\",\\\"n\\\":7}\\n')\n"
            "reply=sys.stdin.buffer.readline(); print('{\\\"type\\\":\\\"final\\\",\\\"payload\\\":'+reply.strip().decode()+'}')\n",
            "import sys,time\n"
            "line=sys.stdin.buffer.readline(); time.sleep(.01)\n"
            "sys.stdout.write('{\\\"id\\\":\\\"q1\\\",\\\"value\\\":14}\\n');sys.stdout.flush()\n"
        )
        self.assertEqual(result["status"], "completed", result)
        self.assertEqual(result["session"]["request_count"], 1)
        self.assertGreater(result["session"]["waited_child_peak_rss_bytes"], 0)
        self.assertGreaterEqual(result["session"]["conservative_rss_sum_bytes"],
                                result["session"]["waited_child_peak_rss_bytes"])
        self.assertIn("not simultaneous group RSS", result["session"]["group_memory_measurement"])
        self.assertEqual(result["session"]["events"][0]["kind"], "exchange")
        self.assertEqual(result["session"]["events"][0]["response_sha256"],
                         hashlib.sha256(b'{"id":"q1","value":14}\n').hexdigest())

    def test_large_frames_exercise_nonblocking_backpressure(self):
        payload = "x" * 180000
        request = "import sys,json\nprint(json.dumps({'type':'query','id':'large','payload':'" + payload + "'}),flush=True)\n"
        request += "reply=sys.stdin.buffer.readline(); print('{\\\"type\\\":\\\"final\\\"}')\n"
        peer = ("import sys,json\nline=sys.stdin.buffer.readline();req=json.loads(line)\n"
                "print(json.dumps({'id':req['id'],'ok':True,'payload':'0'*180000}),flush=True)\n")
        result = self.execute(request, peer, max_frame=250000, max_bytes=500000,
                              wall=3.0)
        self.assertEqual(result["status"], "completed", result)
        self.assertGreater(result["session"]["protocol_bytes"], 350000)

    def test_terminal_frame_has_a_separate_declared_limit(self):
        payload = "z" * 600
        result = self.execute(
            "import json;print(json.dumps({'type':'final','payload':'" + payload + "'}),flush=True)\n",
            "pass\n", max_frame=128, max_terminal_frame=1024,
            max_bytes=2048)
        self.assertEqual(result["status"], "completed", result)
        self.assertEqual(result["session"]["status"], "completed")

    def test_malformed_frame_is_rejected(self):
        result = self.execute("import os;os.write(1,b'{broken}\\n')\n", "import time;time.sleep(2)\n")
        self.assertNotEqual(result["status"], "completed")
        self.assertIn(result.get("session", {}).get("status"), {"protocol_error", None})

    def test_partial_malformed_frame_bytes_are_preserved_on_failure(self):
        malformed = b'{"type":"query","id":"partial"'
        requester = "import os;os.write(1," + repr(malformed) + ")\n"
        result = self.execute(requester, "import sys;sys.stdin.buffer.read()\n")
        self.assertEqual(result.get("session", {}).get("status"), "protocol_error")
        record = result["session"]["byte_evidence"]["producer_stdout"]
        self.assertEqual(base64.b64decode(record["base64"]), malformed)
        self.assertEqual(record["sha256"], hashlib.sha256(malformed).hexdigest())

    def test_duplicate_json_fields_are_rejected(self):
        result = self.execute(
            "import os;os.write(1,b'{\\\"type\\\":\\\"query\\\",\\\"id\\\":\\\"a\\\",\\\"id\\\":\\\"b\\\"}\\n')\n",
            "import time;time.sleep(2)\n")
        self.assertEqual(result.get("session", {}).get("status"), "protocol_error")

    def test_premature_peer_eof_is_explicit(self):
        result = self.execute(
            "import sys;print('{\\\"type\\\":\\\"query\\\",\\\"id\\\":\\\"q1\\\",\\\"input\\\":0}',flush=True);sys.stdin.readline()\n",
            "import sys;sys.stdin.readline();sys.exit(0)\n")
        self.assertNotEqual(result["status"], "completed")
        if result.get("session"):
            self.assertEqual(result["session"]["status"], "premature_eof")

    def test_hung_peer_hits_deadline_and_is_terminated(self):
        result = self.execute(
            "import sys;print('{\\\"type\\\":\\\"query\\\",\\\"id\\\":\\\"q1\\\",\\\"input\\\":0}',flush=True);sys.stdin.readline()\n",
            "import sys,time;sys.stdin.readline();time.sleep(10)\n", wall=.15)
        self.assertNotEqual(result["status"], "completed")
        self.assertEqual(result.get("session", {}).get("status"), "timed_out")

    def test_termination_race_keeps_wait4_usage(self):
        process = subprocess.Popen([sys.executable, "-c", "import time;time.sleep(.04)"])
        original_wait = interactive_session._wait_child
        original_kill = os.kill
        delayed_once = False
        saw_termination_signal = False

        def delayed_wait(child, *, blocking=False):
            nonlocal delayed_once
            if not delayed_once and not blocking:
                delayed_once = True
                time.sleep(.08)
                return None
            result = original_wait(child, blocking=blocking)
            return result

        def observed_kill(pid, signum):
            nonlocal saw_termination_signal
            saw_termination_signal = True
            return original_kill(pid, signum)

        with mock.patch.object(interactive_session, "_wait_child", side_effect=delayed_wait), \
             mock.patch.object(interactive_session.os, "kill", side_effect=observed_kill):
            usage = interactive_session._terminate_children([process])
        self.assertTrue(saw_termination_signal, "fixture did not exercise signal-after-WNOHANG ordering")
        self.assertIn(process.pid, usage, "wait4 usage was lost when signal raced process exit")
        self.assertIsNotNone(process.returncode)

    def test_abnormal_peer_exit_is_not_success(self):
        result = self.execute(
            "import sys;print('{\\\"type\\\":\\\"query\\\",\\\"id\\\":\\\"q1\\\",\\\"input\\\":0}',flush=True);sys.stdin.readline()\n",
            "import sys;sys.stdin.readline();sys.exit(9)\n")
        self.assertNotEqual(result["status"], "completed")

    def test_response_must_match_request_correlation_id(self):
        result = self.execute(
            "import sys;print('{\\\"type\\\":\\\"query\\\",\\\"id\\\":\\\"q1\\\",\\\"input\\\":0}',flush=True);sys.stdin.readline()\n",
            "import sys;sys.stdin.readline();print('{\\\"id\\\":\\\"wrong\\\",\\\"value\\\":0}',flush=True)\n")
        self.assertEqual(result.get("session", {}).get("status"), "protocol_error")
        self.assertIn("correlation id does not match", result["session"]["diagnostic"])

    def test_buffered_terminal_frame_is_drained_after_producer_exit(self):
        result = self.execute(
            "import os;os.write(1,b'{\\\"type\\\":\\\"final\\\"}\\n')\n",
            "import sys;sys.stdin.buffer.read()\n")
        self.assertEqual(result["status"], "completed", result)
        self.assertTrue(result["session"]["byte_evidence"]["producer_stdout"]["complete"])

    def test_terminal_frame_with_hung_child_is_a_timeout(self):
        result = self.execute(
            "import os,time;os.write(1,b'{\\\"type\\\":\\\"final\\\"}\\n');time.sleep(10)\n",
            "import sys;sys.stdin.buffer.read()\n", wall=.15)
        self.assertEqual(result.get("session", {}).get("status"), "timed_out", result)

    def test_late_stderr_after_terminal_is_drained_and_limit_rechecked(self):
        result = self.execute(
            "import os,time;os.write(1,b'{\\\"type\\\":\\\"final\\\"}\\n');"
            "time.sleep(.02);os.write(2,b'x'*512)\n",
            "import sys;sys.stdin.buffer.read()\n", max_frame=128, max_bytes=256, wall=1.0)
        self.assertEqual(result.get("session", {}).get("status"), "resource_exhausted", result)
        stderr = result["session"]["byte_evidence"]["producer_stderr"]
        self.assertGreater(stderr["observed_bytes"], 0)
        self.assertFalse(result["session"]["stream_capture_complete"])

    def test_response_schema_requires_payload(self):
        result = self.execute(
            "import sys;print('{\\\"type\\\":\\\"query\\\",\\\"id\\\":\\\"q1\\\",\\\"input\\\":0}',flush=True);sys.stdin.readline()\n",
            "import sys,json;req=json.loads(sys.stdin.readline());print(json.dumps({'id':req['id']}),flush=True)\n")
        self.assertEqual(result.get("session", {}).get("status"), "protocol_error")

    def test_request_count_limit_is_explicit(self):
        requester = ("import sys\n"
                     "print('{\\\"type\\\":\\\"query\\\",\\\"id\\\":\\\"q1\\\",\\\"input\\\":1}',flush=True);sys.stdin.readline()\n"
                     "print('{\\\"type\\\":\\\"query\\\",\\\"id\\\":\\\"q2\\\",\\\"input\\\":2}',flush=True);sys.stdin.readline()\n")
        responder = ("import sys,json\n"
                     "for line in sys.stdin:\n"
                     " req=json.loads(line);print(json.dumps({'id':req['id'],'value':1}),flush=True)\n")
        result = self.execute(requester, responder, max_requests=1)
        self.assertEqual(result.get("session", {}).get("status"), "resource_exhausted")

    def test_cumulative_message_limit_is_enforced(self):
        result = self.execute(
            "import sys;print('{\\\"type\\\":\\\"query\\\",\\\"id\\\":1,\\\"n\\\":1}',flush=True);sys.stdin.readline();"
            "print('{\\\"type\\\":\\\"final\\\"}')\n",
            "import sys;import json;req=json.loads(sys.stdin.readline());print(json.dumps({'id':req['id'],'v':'1'*240}),flush=True)\n",
            max_frame=256, max_bytes=256)
        self.assertNotEqual(result["status"], "completed")
        self.assertEqual(result.get("session", {}).get("status"), "resource_exhausted", result)

    def test_corrupted_executable_hash_is_refused_before_child_launch(self):
        marker = self.root / "started"
        result = self.execute(
            "import pathlib;pathlib.Path(" + repr(str(marker)) + ").write_text('started')\n",
            "import time;time.sleep(1)\n",
            identity_override={"executable": sys.executable, "sha256": "0" * 64,
                               "runtime_files": []})
        self.assertNotEqual(result["status"], "completed")
        self.assertFalse(marker.exists())

    def test_final_frame_rejects_any_post_terminal_bytes(self):
        result = self.execute(
            "import os,time;os.write(1,b'{\\\"type\\\":\\\"final\\\"}\\n');time.sleep(.02);"
            "os.write(1,b'{\\\"type\\\":\\\"extra\\\"}\\n')\n",
            "import sys;sys.stdin.buffer.read()\n")
        self.assertNotEqual(result["status"], "completed")
        self.assertEqual(result.get("session", {}).get("status"), "protocol_error")

    def test_concurrent_stdout_and_stderr_pressure_is_drained_and_capped(self):
        noisy = "import os,sys,threading,json\n"
        noisy += "def spam():\n  for _ in range(64): os.write(2,b'e'*4096)\n"
        noisy += "t=threading.Thread(target=spam);t.start()\n"
        noisy += "os.write(1,b'{\\\"type\\\":\\\"query\\\",\\\"id\\\":\\\"q1\\\",\\\"input\\\":1}\\n')\n"
        noisy += "sys.stdin.buffer.readline();t.join();print('{\\\"type\\\":\\\"final\\\"}')\n"
        responder = "import os,sys,json,threading\n"
        responder += "def spam():\n  for _ in range(64): os.write(2,b'r'*4096)\n"
        responder += "t=threading.Thread(target=spam);t.start();req=json.loads(sys.stdin.buffer.readline())\n"
        responder += "print(json.dumps({'id':req['id'],'value':1}),flush=True);t.join()\n"
        result = self.execute(noisy, responder, max_bytes=50000, wall=1.5)
        self.assertNotEqual(result["status"], "completed")
        self.assertIn(result.get("session", {}).get("status"), {"resource_exhausted", "protocol_error"})

    def test_success_receipt_preserves_each_stream_byte_for_byte(self):
        producer = ("import os,sys\n"
                    "os.write(2,b'producer-\\xff-stderr')\n"
                    "os.write(1,b'{\\\"type\\\":\\\"query\\\",\\\"id\\\":\\\"q1\\\",\\\"input\\\":1}\\n')\n"
                    "sys.stdin.buffer.readline()\n"
                    "os.write(1,b'{\\\"type\\\":\\\"final\\\"}\\n')\n")
        peer = ("import os,sys,json\n"
                "os.write(2,b'peer-\\xfe-stderr')\n"
                "req=json.loads(sys.stdin.buffer.readline())\n"
                "os.write(1,(json.dumps({'id':req['id'],'value':1})+'\\n').encode())\n")
        result = self.execute(producer, peer)
        self.assertEqual(result["status"], "completed", result)
        evidence = result["session"]["byte_evidence"]
        for name, expected in (("producer_stderr", b"producer-\xff-stderr"),
                               ("peer_stderr", b"peer-\xfe-stderr")):
            record = evidence[name]
            self.assertEqual(base64.b64decode(record["base64"]), expected)
            self.assertEqual(record["sha256"], hashlib.sha256(expected).hexdigest())
            self.assertFalse(record["truncated"])
        expected_request = b'{"type":"query","id":"q1","input":1}\n'
        expected_final = b'{"type":"final"}\n'
        expected_response = b'{"id": "q1", "value": 1}\n'
        self.assertEqual(base64.b64decode(evidence["producer_stdout"]["base64"]),
                         expected_request + expected_final)
        self.assertEqual(base64.b64decode(evidence["peer_stdout"]["base64"]), expected_response)

    def test_surviving_descendant_is_cleaned_by_outer_process_group_supervisor(self):
        pid_file = self.root / "descendant.pid"
        peer = ("import sys,json,subprocess,pathlib\n"
                "req=json.loads(sys.stdin.buffer.readline())\n"
                "child=subprocess.Popen([sys.executable,'-c',\"import time;time.sleep(10)\"])\n"
                f"pathlib.Path({str(pid_file)!r}).write_text(str(child.pid))\n"
                "print(json.dumps({'id':req['id'],'value':1}),flush=True)\n"
                "sys.stdin.buffer.read()\n")
        requester = ("import sys\n"
                     "print('{\\\"type\\\":\\\"query\\\",\\\"id\\\":\\\"q1\\\",\\\"input\\\":1}',flush=True)\n"
                     "sys.stdin.buffer.readline();print('{\\\"type\\\":\\\"final\\\"}')\n")
        result = self.execute(requester, peer)
        self.assertIn(result["status"], {"orphaned_process_group", "timed_out"}, result)
        pid = int(pid_file.read_text())
        deadline = time.monotonic() + 1.0
        while time.monotonic() < deadline:
            try:
                stat = Path(f"/proc/{pid}/stat")
                if not stat.exists() or stat.read_text().rsplit(")", 1)[1].split()[0] == "Z":
                    break
            except OSError:
                break
            time.sleep(0.01)
        else:
            self.fail("session descendant survived outer process-group cleanup")


if __name__ == "__main__":
    unittest.main()
