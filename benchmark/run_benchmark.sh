#!/usr/bin/env bash
# Benchmark Forge vs Python HTTP servers
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FORGE_BIN="${ROOT}/build/bin/bench_server"
PY_SERVER="${ROOT}/benchmark/python/server.py"
FORGE_PORT=19080
PY_PORT=19081
REQUESTS=2000
CONCURRENCY=50
RESULTS="${ROOT}/benchmark/results.txt"

mkdir -p "$(dirname "$RESULTS")"

run_load() {
    local url="$1"
    if command -v hey >/dev/null 2>&1; then
        hey -n "$REQUESTS" -c "$CONCURRENCY" -m GET "$url" 2>&1
    else
        python3 - "$url" "$REQUESTS" "$CONCURRENCY" <<'PY'
import sys, time, urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed

url, n, c = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
ok = 0
start = time.perf_counter()

def one(_):
    with urllib.request.urlopen(url, timeout=5) as r:
        r.read()
    return 1

with ThreadPoolExecutor(max_workers=c) as pool:
    futs = [pool.submit(one, i) for i in range(n)]
    for f in as_completed(futs):
        ok += f.result()

elapsed = time.perf_counter() - start
rps = ok / elapsed if elapsed > 0 else 0
print(f"Requests:      {ok}")
print(f"Duration:      {elapsed:.4f}s")
print(f"Requests/sec:  {rps:.2f}")
PY
    fi
}

bench_one() {
    local name="$1" port="$2" cmd=("${@:3}")
    echo "=== $name (port $port) ==="
    "${cmd[@]}" &
    local pid=$!
    sleep 0.5
    if ! curl -sf "http://127.0.0.1:${port}/" >/dev/null; then
        echo "Server failed to start" >&2
        kill "$pid" 2>/dev/null || true
        wait "$pid" 2>/dev/null || true
        return 1
    fi
    run_load "http://127.0.0.1:${port}/"
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
    echo
}

{
    echo "Forge vs Python HTTP Benchmark"
    echo "Date: $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
    echo "Host: $(uname -srm)"
    echo "Requests: $REQUESTS, Concurrency: $CONCURRENCY"
    echo "Body: Hello, World (13 bytes)"
    echo

    if [[ ! -x "$FORGE_BIN" ]]; then
        echo "Forge binary not found: $FORGE_BIN" >&2
        exit 1
    fi

    bench_one "Forge (AOT native)" "$FORGE_PORT" "$FORGE_BIN"
    bench_one "Python 3 (socket)" "$PY_PORT" python3 "$PY_SERVER"
} | tee "$RESULTS"

echo "Results saved to $RESULTS"
