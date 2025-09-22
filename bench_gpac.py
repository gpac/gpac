#!/usr/bin/env python3
import subprocess
import re
import statistics

# Regex to capture metrics from H1/H2/H3 logs
METRICS_RE = re.compile(
    r"\[(H\d-METRICS)\]\s+mode=(\w+)\s+url=(\S+)\s+bytes=(\d+)\s+ttfb_us=(\d+)\s+dur_us=(\d+)\s+thr=([\d\.]+)Mb/s\s+cpu_user_ms=(\d+)\s+cpu_sys_ms=(\d+)\s+(?:rss_kb|mem)=(\d+)\s+phase=(\w+)"
)

def run_test(cmd, proto, runs=4):
    results = []
    for i in range(runs):
        try:
            out = subprocess.check_output(cmd, shell=True, stderr=subprocess.STDOUT, text=True)
        except subprocess.CalledProcessError as e:
            out = e.output

        match = METRICS_RE.search(out)
        if not match:
            print(f"No metrics found for: {cmd}")
            continue

        tag, mode, url, bytes_, ttfb_us, dur_us, thr, cpu_user, cpu_sys, mem, phase = match.groups()

        res = {
            "proto": proto,
            "throughput": float(thr),  # already in Mb/s, don’t touch
            "ttfb": int(ttfb_us) / 1000.0,   # convert µs → ms
            "dur": int(dur_us) / 1000.0,     # convert µs → ms
            "cpu_user": int(cpu_user),
            "cpu_sys": int(cpu_sys),
            "cpu_total": int(cpu_user) + int(cpu_sys),
            "mem": int(mem)
        }
        results.append(res)
        print(f"Run {i+1}: {res}")
    return results

def summarize(proto, results):
    if not results:
        print(f"== {proto} (no data) ==\n")
        return
    avg = {
        "throughput": statistics.mean([r["throughput"] for r in results]),
        "ttfb": statistics.mean([r["ttfb"] for r in results]),
        "dur": statistics.mean([r["dur"] for r in results]),
        "cpu_user": statistics.mean([r["cpu_user"] for r in results]),
        "cpu_sys": statistics.mean([r["cpu_sys"] for r in results]),
        "cpu_total": statistics.mean([r["cpu_total"] for r in results]),
        "mem": statistics.mean([r["mem"] for r in results]),
    }
    print(f"\n== {proto} (avg over {len(results)} runs) ==")
    print(f"throughput={avg['throughput']:.2f} Mb/s")
    print(f"ttfb={avg['ttfb']:.2f} ms")
    print(f"dur={avg['dur']:.2f} ms")
    print(f"cpu_user={avg['cpu_user']:.2f} ms")
    print(f"cpu_sys={avg['cpu_sys']:.2f} ms")
    print(f"cpu_total={avg['cpu_total']:.2f} ms")
    print(f"mem={avg['mem']:.2f} KB\n")

def main():
    tests = {
    # H1 over TLS
    "H1":  "GPAC_NO_H3=1 ./bin/gcc/gpac -no-h2 -no-h2c "
           "-i https://127.0.0.1:444/counter_video_360.mp4 "
           "-o out_h1.mp4 -logs=http@info -broken-cert",

    # H2 over TLS (ALPN h2)
    "H2":  "GPAC_NO_H3=1 ./bin/gcc/gpac "
           "-i https://127.0.0.1:444/counter_video_360.mp4 "
           "-o out_h2.mp4 -logs=http@info -broken-cert",

    # H3 over QUIC
    "H3":  "./bin/gcc/gpac -h3=only   -i https://127.0.0.1:444/counter_video_360.mp4   -o out_h3.mp4 -logs=http@info -broken-cert",
}


    for proto, cmd in tests.items():
        print(f"== Running {proto} ==")
        results = run_test(cmd, proto, runs=4)
        summarize(proto, results)

if __name__ == "__main__":
    main()
