# Characterizing access-security reload

This experiment supports the HAG hostname-refresh discussion in
[#863](https://github.com/epics-base/epics-base/issues/863) and the review of
[#945](https://github.com/epics-base/epics-base/pull/945).
It measures current Base behavior without implementing periodic refresh.

## Method

The optional `asReloadProbe` executable uses existing database, IOC-shell, and
CA APIs. Its IOC and client modes run as separate processes. A provider IOC
serves constant-valued access-security inputs; a subject IOC applies a HAG
condition and, in input-backed cases, `CALC("A=1")` to write access. Read access
remains allowed so a retained client can observe record monitors throughout.

The runner generates synthetic policies with 1, 16, or 128 groups and one
protected record per group. Each group has either no input or one external CA
input. A single external client observes the first protected record. These
are laboratory workloads, not a sample of operational ACFs.

Each timing case has five warm-up reloads followed by thirty measured reloads.
Numeric HAG and hostname-string cases provide controls. A separate diagnostic
reload enables `asCaDebug` and prints `ascar(2)`; timing runs disable debug
output.

`reload_ns` measures only the synchronous `asInit()` call using
`epicsMonotonicGet()`. `inputs_connected_ns` measures from that call's start
until `ascaStats()` reports all inputs connected, using a 1 ms polling interval
and a 10 s recovery bound. Connection does not guarantee that the first input
value or the resulting access-rights update has already arrived. The separate
client records connection, rights, and monitor callbacks, and the runner waits
for expected rights and a fresh value monitor before continuing. For cases
without inputs, the input-connection measurement is not applicable.

Callback counts in the raw measurements cover each measured reload through
receipt of its subsequent value marker. Callback timestamps use the client's
own monotonic clock; parent receipt timestamps are observation timestamps,
not an additional measurement of the synchronous library call.

## Running

Build a clean checkout containing this change:

```sh
make -j4
make -C modules/libcom/test runtests TESTS=aslibtest
python3 modules/database/test/std/rec/runAsReloadExperiment.py \
    --smoke --output /tmp/as-reload-smoke
```

Python 3 is required only for the optional runner. Its output directory must
not already exist. The default full run uses `--groups 1 16 128 --warmup 5
--samples 30`. Outputs include every generated ACF, database, startup script,
process log, event trace, raw measurement, environment record, and summary.
No elapsed-time threshold determines success.

To exercise changing resolution without altering a host resolver configuration,
run in a disposable Linux container with `--network none`. Bind-mount a
task-owned file at both `/hosts.fixture` and `/etc/hosts`. The file must start
with `# asReloadProbe resolver fixture`. Pass `--hosts-file /hosts.fixture` to
the runner. It changes only that fixture and restores its original contents
on exit. CA servers bind loopback; no host port needs publishing.

The controlled name is `hag-refresh.test`. The runner verifies the current
resolver result through `aToIPAddr()`, then exercises address replacement,
restoration, lookup loss, and recovery with an unchanged ACF. This uses libc's
hosts-file resolution path; it does not measure DNS TTLs, authoritative DNS
propagation, or a DNS server's caching behavior. Hostname-string mode uses the
actual client hostname, as supplied by the existing CA implementation.

An example container build recipe is:

```dockerfile
FROM ubuntu:24.04@sha256:008173c23f95b170204355c12626cb5a965d779a7e1283b09e9cffbb1bf33ca3
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential perl python3 libreadline-dev ca-certificates git \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /work
```

With that image tagged `as-reload-test`, an unbuilt Linux checkout in the
current directory, and a new task-owned scratch directory:

```sh
scratch=$(mktemp -d)
printf '%s\n' '# asReloadProbe resolver fixture' '127.0.0.1 localhost' \
    '::1 localhost' '127.0.0.1 hag-refresh.test' > "$scratch/hosts.fixture"
docker run --rm --network none --cpus 4 --memory 4g \
    --user "$(id -u):$(id -g)" \
    --mount "type=bind,src=$PWD,dst=/work" \
    --mount "type=bind,src=$scratch,dst=/evidence" \
    --mount "type=bind,src=$scratch/hosts.fixture,dst=/hosts.fixture" \
    --mount "type=bind,src=$scratch/hosts.fixture,dst=/etc/hosts" \
    as-reload-test sh -c '
        make -j4 &&
        python3 modules/database/test/std/rec/runAsReloadExperiment.py \
            --hosts-file /hosts.fixture --output /evidence/run'
```

## Findings and validation

The 2026-09-30 run used upstream `7.0` at
`e6c7dd045c2f7bb6fea43961d6a7df1328bc03f7` plus this evidence patch at candidate
`653dea3347ea6418e99ce709e418fcc26bf73d71`. Later edits document subprocess
trust boundaries and format this report; they do not change experiment behavior.
The Linux
image ran on an x86-64 build host, with four CPU equivalents, a 4 GiB memory
limit, Ubuntu 24.04, GCC 13.3.0, Python 3.12.3, glibc 2.39, and Perl 5.38.2.
The host also had other workloads; these timings do not establish production
reload costs or a performance benefit for a future refresh implementation.

Image ID:
`sha256:6223da79095520e088d1aa1c81fa4c4d838b5138af1674b6f32cb603cfba4fcf`.
The base-image digest is pinned in the recipe above. The run used
`build-essential 12.10ubuntu1`, `libc6 2.39-0ubuntu8.9`,
`libreadline-dev 8.2-4build1`, `perl 5.38.2-3.2ubuntu0.6`, and
`python3 3.12.3-0ubuntu2.1`.

All values below are milliseconds except the write-removal count (out of 30
reloads). The call columns describe `asInit()`. Raw records
for the 240 measured reloads are in
[as-reload-samples.csv](as-reload-samples.csv).

| Groups | Inputs | Call median | Call p95 | Connected median | Write removals |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 0 | 0.077 | 0.152 | N/A | 0 / 30 |
| 1 | 1 | 0.122 | 7.753 | 31.685 | 30 / 30 |
| 16 | 0 | 0.069 | 0.075 | N/A | 0 / 30 |
| 16 | 16 | 0.183 | 0.205 | 31.771 | 30 / 30 |
| 128 | 0 | 0.336 | 0.349 | N/A | 0 / 30 |
| 128 | 128 | 0.742 | 0.773 | 31.509 | 30 / 30 |

The numeric and hostname-string controls had median synchronous reload times
of 0.032 ms and 0.031 ms respectively, with no extra rights callbacks.
Every input-backed measured reload caused a retained client to observe write
access removal and restoration, despite unchanged ACF bytes and constant input
values. Read access stayed granted. Input-backed cases also delivered 90 value
monitor callbacks for 30 value markers; no-input cases delivered 30. No client
connection-down callback occurred in any of the 240 measured reloads.

The input-channel recreation is also established by the existing
`asCaStop()`/`asCaStart()` path and the diagnostic logs, not inferred from
allocation addresses. The synchronous call alone was cheap in these synthetic
cases; the visible permission transitions make routine full reload a different
operation from refreshing DNS-derived membership while preserving inputs.

With the same ACF and retained client, the resolver scenarios produced:

| Resolver change | Write before | Write after | Reload status |
| --- | ---: | ---: | ---: |
| `127.0.0.1` to `127.0.0.2` | 1 | 0 | 0 |
| Restore `127.0.0.1` | 0 | 1 | 0 |
| Remove the hostname mapping | 1 | 0 | 0 |
| Restore the hostname mapping | 0 | 1 | 0 |

Lookup loss removes that HAG membership on a successful reload; it is distinct
from a parse failure, which preserves the previous loaded policy. The regression
tests exercise that rollback separately and retain both original client objects
through unchanged, changed, duplicate-effective, failed, and restored policies.

Validation completed:

- Native macOS core Base build, 90/90 `aslibtest` assertions, all 51 libCom test
  programs (4,588 assertions), and the four-case experiment smoke run.
- Linux shared core Base build, 90/90 `aslibtest` assertions, the controlled
  smoke run, and the full eight-case experiment with four resolver transitions.
- Linux libCom: 52 test programs and 4,590 assertions. The initial `--network
  none` run failed `osiSockTest` assertions 19–20, which require a broadcast
  broadcast interface. That unchanged test passed all 24 assertions when rerun
  on a task-owned internal Docker bridge. The CA measurements used `--network
  none` throughout.
- Linux core Base build with `SHARED_LIBRARIES=NO STATIC_BUILD=YES`, 90/90
  regression assertions, and probe linkage containing no shared Base libraries.
  System libraries remained dynamically linked.

Platform validation here covers macOS and Linux. Process logs, event traces,
fixtures, build logs, package versions, and image identity are retained in the
experiment output; the CSV contains the measured timing and callback records.

## Proposed next step

Use these observations to discuss the smallest change that refreshes HAG
membership without reinitializing unrelated policy and input subscriptions.
Start with reusable existing hostname-resolution logic in `asLib`, then provide
complete standard IOC scheduling and lifecycle integration. Existing connected
clients must receive rights updates through the current callbacks.

For a later prototype, the proposed defaults are opt-in operation, 300 seconds
between successful refreshes, 60 seconds between retries, and immediate removal
of a failed hostname mapping. These are proposals, not accepted policy. DNS
work belongs outside permission checks and the access-security lock; results
must be discarded if the loaded policy was replaced while resolution ran.
The evidence patch introduces no refresh API or production behavior change.
