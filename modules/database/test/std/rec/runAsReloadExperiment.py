#!/usr/bin/env python3
"""Optional host experiment; outputs fixtures, raw logs, and JSON measurements."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import socket
import statistics
# The runner executes programs from a trusted local Base build, without a shell.
import subprocess  # nosec B404
import threading
import time
import uuid


class Process:
    def __init__(self, argv, env, logfile):
        """Start a trusted local Base probe and collect its output."""
        self.events = []
        self.condition = threading.Condition()
        self.logfile = logfile
        # argv is constructed below from the selected build and generated fixtures.
        self.proc = subprocess.Popen(argv, env=env, stdin=subprocess.PIPE,  # nosec B603
                                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                     text=True, bufsize=1, shell=False)
        self.reader = threading.Thread(target=self.read, daemon=True)
        self.reader.start()

    def read(self):
        with self.logfile.open("w") as log:
            for line in self.proc.stdout:
                log.write(line)
                log.flush()
                marker = line.find("ASRELOAD {")
                if marker < 0:
                    continue
                event = json.loads(line[marker + len("ASRELOAD "):])
                event["received_ns"] = time.monotonic_ns()
                with self.condition:
                    self.events.append(event)
                    self.condition.notify_all()

    def send(self, command):
        self.proc.stdin.write(command + "\n")
        self.proc.stdin.flush()

    def wait(self, predicate, start=0, timeout=15):
        deadline = time.monotonic() + timeout
        with self.condition:
            while True:
                for event in self.events[start:]:
                    if predicate(event):
                        return event
                start = len(self.events)
                remaining = deadline - time.monotonic()
                if remaining <= 0 or self.proc.poll() is not None:
                    raise RuntimeError("Timed out or process exited; see " + str(self.logfile))
                self.condition.wait(min(remaining, 0.1))

    def state(self, expected):
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            start = len(self.events)
            self.send("state")
            event = self.wait(lambda e: e["event"] == "state", start)
            if event["connected"] and event["read"] and event["write"] == expected:
                return event
            time.sleep(0.01)
        raise RuntimeError("Client rights did not converge; see " + str(self.logfile))

    def close(self):
        if self.proc.poll() is None:
            self.proc.stdin.close()
            try:
                self.proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.proc.terminate()
                try:
                    self.proc.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    self.proc.kill()
                    self.proc.wait()
        self.reader.join(timeout=2)


def free_port():
    with socket.socket() as tcp:
        tcp.bind(("127.0.0.1", 0))
        return tcp.getsockname()[1]


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2) + "\n")


def mapping(path, address):
    # Truncate in place: an atomic rename would not update a bind-mounted inode.
    path.write_text("# asReloadProbe resolver fixture\n127.0.0.1 localhost\n"
                    "::1 localhost\n" +
                    (address + " hag-refresh.test\n" if address else ""))


def profile(args, probe, dbd, groups, inputs, mode):
    directory = args.output / ("%s-%d-%d" % (mode, groups, inputs))
    directory.mkdir()
    prefix = "ASRELOAD:" + uuid.uuid4().hex[:8] + ":"
    host = socket.gethostname() if mode == "string" else (
        "127.0.0.1" if mode == "numeric" else
        "hag-refresh.test" if args.hosts_file else "localhost")
    provider_db, subject_db, acf = (directory / name for name in
                                   ("provider.db", "subject.db", "policy.acf"))
    provider_db.write_text("\n".join(
        'record(ao,"%sG%d") { field(VAL,"1") }' % (prefix, i)
        for i in range(groups)))
    subject_db.write_text("\n".join(
        'record(ao,"%sT%d") { field(ASG,"g%d") }' % (prefix, i, i)
        for i in range(groups)))
    acf.write_text('HAG(hosts) {%s}\nASG(DEFAULT) { RULE(1, READ) }\n' % host +
                   "\n".join('ASG(g%d) { %s RULE(1, READ) '
                             'RULE(1, WRITE) { HAG(hosts) %s } }' %
                             (i, 'INPA("%sG%d")' % (prefix, i) if inputs else "",
                              'CALC("A=1")' if inputs else "")
                             for i in range(groups)))
    policy_hash = hashlib.sha256(acf.read_bytes()).hexdigest()
    provider_port, subject_port = free_port(), free_port()
    while subject_port == provider_port:
        subject_port = free_port()
    env = dict(os.environ, EPICS_CA_AUTO_ADDR_LIST="NO",
               EPICS_CA_ADDR_LIST="127.0.0.1:%d 127.0.0.1:%d" %
               (provider_port, subject_port), EPICS_CA_NAME_SERVERS="",
               EPICS_CAS_INTF_ADDR_LIST="127.0.0.1",
               EPICS_CAS_AUTO_BEACON_ADDR_LIST="NO", EPICS_CAS_BEACON_ADDR_LIST="")
    processes = []
    samples, phases = [], []
    try:
        for role, database, port in (("provider", provider_db, provider_port),
                                     ("subject", subject_db, subject_port)):
            startup = directory / (role + ".cmd")
            startup.write_text('dbLoadRecords("%s")\n' % database +
                               ('asSetFilename("%s")\nvar("asCheckClientIP",%d)\n' %
                                (acf, mode != "string") if role == "subject" else "") +
                               "iocInit()\n")
            proc = Process([str(probe), "ioc", str(dbd), str(startup)],
                           dict(env, EPICS_CAS_SERVER_PORT=str(port)),
                           directory / (role + ".log"))
            processes.append(proc)
            proc.wait(lambda e: e["event"] == "ready")
        subject = processes[1]
        client = Process([str(probe), "client", prefix + "T0"], env,
                         directory / "client.log")
        processes.append(client)
        client.state(1)
        client.wait(lambda e: e["event"] == "value")

        def reload(sample, rights):
            start = len(subject.events)
            subject.send("asReloadSample(%d)" % sample)
            result = subject.wait(lambda e: e["event"] == "reload", start)
            if result["status"] or result["disconnected"]:
                raise RuntimeError("Reload or input reconnection failed: " + str(result))
            if result["inputs"] != groups * inputs:
                raise RuntimeError("Unexpected access-security input count")
            client.state(rights)
            return result

        subject.send('var("asCaDebug",1)')
        subject.send("ascar(2)")
        reload(-1, 1)
        subject.send("ascar(2)")
        subject.send('var("asCaDebug",0)')
        for iteration in range(args.warmup + args.samples):
            event_start = len(client.events)
            result = reload(iteration, 1)
            start = len(client.events)
            subject.send('dbpf("%sT0",%d)' % (prefix, iteration + 1))
            client.wait(lambda e: e["event"] == "value" and
                        e["value"] == iteration + 1, start)
            events = client.events[event_start:]
            result["access_callbacks"] = sum(e["event"] == "access" for e in events)
            result["write_denials"] = sum(e["event"] == "access" and not e["write"]
                                         for e in events)
            result["disconnects"] = sum(e["event"] == "connection" and not e["connected"]
                                        for e in events)
            result["monitor_updates"] = sum(e["event"] == "value" for e in events)
            if iteration >= args.warmup:
                samples.append(result)

        if args.hosts_file and mode == "hostname" and groups == 1 and inputs == 1:
            for phase, address, before, after in (
                    ("address-change", "127.0.0.2", 1, 0),
                    ("address-restoration", "127.0.0.1", 0, 1),
                    ("lookup-loss", None, 1, 0),
                    ("lookup-recovery", "127.0.0.1", 0, 1)):
                mapping(args.hosts_file, address)
                start = len(subject.events)
                subject.send("asReloadResolve()")
                resolved = subject.wait(lambda e: e["event"] == "resolve", start)
                if address and resolved["address"] != address + ":0":
                    raise RuntimeError("Resolver fixture did not change as expected")
                if not address and not resolved["status"]:
                    raise RuntimeError("Expected lookup failure")
                before_state = client.state(before)
                result = reload(1000 + len(phases), after)
                phases.append(dict(phase=phase, resolver=resolved, before=before_state,
                                   after=client.state(after), reload=result))
        if hashlib.sha256(acf.read_bytes()).hexdigest() != policy_hash:
            raise RuntimeError("ACF changed during experiment")
        durations = sorted(s["reload_ns"] for s in samples)
        result = dict(groups=groups, inputs=groups * inputs, mode=mode,
                      acf_sha256=policy_hash, samples=len(samples), phases=phases,
                      reload_ns=dict(min=min(durations), median=statistics.median(durations),
                                     p95=durations[math.ceil(0.95 * len(durations)) - 1],
                                     max=max(durations)),
                      inputs_connected_ns_median=statistics.median(
                          s["inputs_connected_ns"] for s in samples),
                      observed_access_callbacks=sum(s["access_callbacks"] for s in samples),
                      observed_write_denials=sum(s["write_denials"] for s in samples),
                      observed_monitor_updates=sum(s["monitor_updates"] for s in samples),
                      observed_disconnects=sum(s["disconnects"] for s in samples))
        write_json(directory / "measurements.json", samples)
        return result
    finally:
        for proc in reversed(processes):
            proc.close()
        for role, proc in zip(("provider", "subject", "client"), processes):
            write_json(directory / (role + "-events.json"), proc.events)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", type=Path, default=Path(__file__).resolve().parents[5],
                        help="trusted local Base build containing this experiment")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--groups", type=int, nargs="+", default=[1, 16, 128])
    parser.add_argument("--warmup", type=int, default=5)
    parser.add_argument("--samples", type=int, default=30)
    parser.add_argument("--hosts-file", type=Path,
                        help="task-owned fixture bind-mounted as /etc/hosts in an isolated container")
    parser.add_argument("--smoke", action="store_true", help="one group, one warm-up, two samples")
    args = parser.parse_args()
    if args.smoke:
        args.groups, args.warmup, args.samples = [1], 1, 2
    if min(args.groups) < 1 or args.warmup < 0 or args.samples < 1:
        parser.error("groups/samples must be positive and warm-up nonnegative")
    args.base, args.output = args.base.resolve(), args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    # This is the existing architecture helper in the explicitly selected build.
    arch = subprocess.check_output(  # nosec B603
        ["perl", str(args.base / "src/tools/EpicsHostArch.pl")],
        text=True, shell=False).strip()
    testdir = args.base / "modules/database/test/std/rec"
    probe, dbd = testdir / ("O." + arch) / "asReloadProbe", testdir / "O.Common/recTestIoc.dbd"
    metadata = dict(platform=platform.platform(), arch=arch, python=platform.python_version(),
                    probe_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),
                    groups=args.groups, warmup=args.warmup, samples=args.samples,
                    hostname_source="container hosts fixture" if args.hosts_file else "system localhost")
    write_json(args.output / "environment.json", metadata)
    original = None
    if args.hosts_file:
        args.hosts_file = args.hosts_file.resolve()
        if args.hosts_file == Path("/etc/hosts"):
            parser.error("use a separate task-owned fixture path, not /etc/hosts")
        original = args.hosts_file.read_text()
        if not original.startswith("# asReloadProbe resolver fixture\n"):
            parser.error("hosts fixture must start with '# asReloadProbe resolver fixture'")
        if not os.path.samefile(args.hosts_file, "/etc/hosts"):
            parser.error("hosts fixture must be bind-mounted as this container's /etc/hosts")
        mapping(args.hosts_file, "127.0.0.1")
    results = []
    try:
        for groups in args.groups:
            for inputs in (0, 1):
                results.append(profile(args, probe, dbd, groups, inputs, "hostname"))
                write_json(args.output / "summary.json", results)
        results.append(profile(args, probe, dbd, 1, 0, "numeric"))
        results.append(profile(args, probe, dbd, 1, 0, "string"))
        write_json(args.output / "summary.json", results)
        print(json.dumps(results, indent=2))
    finally:
        if original is not None:
            args.hosts_file.write_text(original)


if __name__ == "__main__":
    main()
