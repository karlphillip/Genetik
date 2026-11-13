# Coordinator resource accounting

Continuation coordinator phases run in a fresh supervised Python process. The
supervisor applies `RLIMIT_AS` before `exec`, bounds wall time and captured
output, samples the worker's Linux `/proc/<pid>/status` identity and memory
fields, and reaps its process group. The worker also applies the address-space
limit at entry before importing the research implementation.

The 512 MiB policy has two distinct measurements:

- `VmHWM` is the worker image's observed high-water resident set and is the
  value compared with the 512 MiB qualification cap. Linux documents this
  field as approximate, so the receipt names it as an observed worker peak.
- `RLIMIT_AS` is an enforced virtual-address-space limit. It is not an RSS
  limit and must not be reported as one.

`RUSAGE_SELF.ru_maxrss` is retained only as a diagnostic. On Linux its
high-water value can survive `execve`, so it may describe memory used by a
forking launcher rather than the fresh worker image. `wait4` usage is recorded
separately because its resource scope may include waited child processes. The
supervisor's own `VmHWM` and current `VmRSS` are also separate fields; current
RSS is never used as a peak or substituted for an unavailable measurement.

Coordinator CPU and exclusive wall accounting starts at the worker script
entrypoint, before importing the research modules. Per-run worker and
coordinator charges are subtracted from the phase residual to avoid double
counting. Parent supervisor overhead is measured and charged separately.
Unavailable measurements remain null and make the phase incomplete; they do
not become zero charges or a successful qualification.

The phase ledger distinguishes pre-phase admission refusal, phase failure,
preflight-stage failure, and preflight-qualification failure. An admission
refusal is terminal under the same frozen authorization. Only an admitted,
supervised preflight with verified resource receipts can qualify for its
explicit recovery transition. Concurrent attempts cannot close another
worker's active phase. Worker identity is bound by PID and process start time,
and the coordinator entrypoints and supervisor helper are content-pinned by
the frozen continuation manifest.

## Incident interpretation

The historical admission incident's 919 MiB reading cannot be attributed to
the coordinator's actual contemporaneous RSS: its failed-call receipt did not
retain the process identity or memory measurements. Independent Linux probes
did demonstrate that a fork/exec worker can inherit its launcher-era
`RUSAGE_SELF.ru_maxrss` while the worker's own `/proc` `VmHWM` is much lower.
That is a reproducible measurement-scope defect and a plausible explanation
for the reported value, but it does not prove the exact source of that
incident's 919 MiB. The 512 MiB cap is preserved.

This accounting is per supervised coordinator process, not an aggregate
process-tree RSS guarantee. `RLIMIT_AS` bounds each process's address space;
strict aggregate resident-memory enforcement would require a separately
qualified cgroup policy.
