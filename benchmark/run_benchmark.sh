#!/usr/bin/env bash
# Benchmark Forge vs Python vs Phoenix vs Rust Axum HTTP servers
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FORGE_BIN="${ROOT}/build/bin/bench_server"
PY_SERVER="${ROOT}/benchmark/python/server.py"
PHX_SERVER="${ROOT}/benchmark/phoenix/run_server.sh"
AXUM_SERVER="${ROOT}/benchmark/axum/run_server.sh"
FORGE_PORT=19080
PY_PORT=19081
PHX_PORT=19082
AXUM_PORT=19083
REQUESTS=1000000
CONCURRENCY=1000
RESULTS="${ROOT}/benchmark/results.txt"
CPU_COUNT="$(nproc 2>/dev/null || echo 1)"

mkdir -p "$(dirname "$RESULTS")"

run_load() {
    local url="$1"
    if command -v hey >/dev/null 2>&1; then
        hey -n "$REQUESTS" -c "$CONCURRENCY" -m GET "$url" 2>&1
    else
        python3 - "$url" "$REQUESTS" "$CONCURRENCY" <<'PY'
import socket, sys, time
from concurrent.futures import ThreadPoolExecutor, as_completed
from urllib.parse import urlparse

url, n, c = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
parsed = urlparse(url)
host = parsed.hostname or "127.0.0.1"
port = parsed.port or 80
req = (
    f"GET {parsed.path or '/'} HTTP/1.1\r\n"
    f"Host: {host}\r\n"
    f"Connection: close\r\n\r\n"
).encode()

ok = err = 0
start = time.perf_counter()
batch = 20000

def one(_):
    last_err = None
    for _attempt in range(3):
        try:
            s = socket.create_connection((host, port), timeout=30)
            s.sendall(req)
            while s.recv(8192):
                pass
            s.close()
            return 1
        except OSError as e:
            last_err = e
    raise last_err

done = 0
while done < n:
    chunk = min(batch, n - done)
    with ThreadPoolExecutor(max_workers=c) as pool:
        futs = [pool.submit(one, i) for i in range(chunk)]
        for f in as_completed(futs):
            try:
                ok += f.result()
            except Exception:
                err += 1
    done += chunk
    if done % 100000 == 0 or done == n:
        elapsed = time.perf_counter() - start
        rps = ok / elapsed if elapsed > 0 else 0
        print(f"Progress:      {done}/{n} ({rps:.0f} req/s, {err} errors)", flush=True)

elapsed = time.perf_counter() - start
rps = ok / elapsed if elapsed > 0 else 0
print(f"Requests:      {ok}")
print(f"Errors:        {err}")
print(f"Duration:      {elapsed:.4f}s")
print(f"Requests/sec:  {rps:.2f}")
PY
    fi
}

pids_on_port() {
    local port="$1"
    if command -v lsof >/dev/null 2>&1; then
        lsof -ti "tcp:${port}" -sTCP:LISTEN 2>/dev/null | sort -u
    elif command -v ss >/dev/null 2>&1; then
        ss -tlnp "sport = :${port}" 2>/dev/null | grep -oP 'pid=\K[0-9]+' | sort -u
    else
        fuser "${port}/tcp" 2>/dev/null | tr ' ' '\n' | sort -u
    fi
}

sample_server_resources() {
    local port="$1"
    local pids
    pids="$(pids_on_port "$port" | tr '\n' ' ')"
    if [[ -z "${pids// }" ]]; then
        echo "0 0"
        return
    fi

    local total_rss_kb=0
    local total_cpu=0
    for pid in $pids; do
        [[ -r "/proc/${pid}/status" ]] || continue
        local rss_kb cpu_pct
        rss_kb="$(awk '/VmRSS/{print $2}' "/proc/${pid}/status" 2>/dev/null || echo 0)"
        cpu_pct="$(ps -p "$pid" -o %cpu= 2>/dev/null | tr -d ' ,' || echo 0)"
        total_rss_kb=$((total_rss_kb + rss_kb))
        total_cpu="$(awk -v a="$total_cpu" -v b="${cpu_pct:-0}" 'BEGIN{printf "%.2f", a + b}')"
    done
    echo "$total_rss_kb $total_cpu"
}

RESOURCE_MON_PID=""

start_resource_monitor() {
    local port="$1"
    local log="$2"
    : >"$log"
    (
        while true; do
            # Avoid "read < <(cmd)" inside a background subshell — it can hang bash.
            set -- $(sample_server_resources "$port")
            printf '%s %s %s\n' "$(date +%s.%N)" "${1:-0}" "${2:-0}" >>"$log"
            sleep 1
        done
    ) &
    RESOURCE_MON_PID=$!
}

print_resource_summary() {
    local log="$1"
    if [[ ! -s "$log" ]]; then
        echo "Peak RSS:      n/a"
        echo "Avg CPU:       n/a (${CPU_COUNT} cores available)"
        echo "Peak CPU:      n/a"
        return
    fi

    awk -v cores="$CPU_COUNT" '
        NR > 0 {
            rss = $2 / 1024
            cpu = $3
            if (rss > peak_rss) peak_rss = rss
            if (cpu > peak_cpu) peak_cpu = cpu
            cpu_sum += cpu
            n++
        }
        END {
            if (n == 0) {
                print "Peak RSS:      n/a"
                print "Avg CPU:       n/a (" cores " cores available)"
                print "Peak CPU:      n/a"
            } else {
                printf "Peak RSS:      %.1f MB\n", peak_rss
                printf "Avg CPU:       %.1f%% (%d cores, %.1f%% of total capacity)\n", cpu_sum / n, cores, (cpu_sum / n) / cores
                printf "Peak CPU:      %.1f%%\n", peak_cpu
            }
        }
    ' "$log"
}

bench_one() {
    local name="$1" port="$2" cmd=("${@:3}")
    echo "=== $name (port $port) ==="
    # Use all CPU cores for the server process tree.
    if command -v taskset >/dev/null 2>&1; then
        taskset -c "0-$((CPU_COUNT - 1))" "${cmd[@]}" &
    else
        "${cmd[@]}" &
    fi
    local pid=$!
    local ready=0
    for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
        if curl -sf "http://127.0.0.1:${port}/" >/dev/null; then
            ready=1
            break
        fi
        sleep 1
    done
    if [[ "$ready" -ne 1 ]]; then
        echo "Server failed to start" >&2
        kill "$pid" 2>/dev/null || true
        wait "$pid" 2>/dev/null || true
        return 1
    fi

    local res_log
    res_log="$(mktemp)"
    start_resource_monitor "$port" "$res_log"
    local mon_pid="$RESOURCE_MON_PID"
    PYTHONUNBUFFERED=1 run_load "http://127.0.0.1:${port}/"
    kill "$mon_pid" 2>/dev/null || true
    wait "$mon_pid" 2>/dev/null || true
    print_resource_summary "$res_log"
    rm -f "$res_log"

    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
    # Ensure the port is free before the next server starts.
    fuser -k "${port}/tcp" 2>/dev/null || true
    sleep 1
    echo
}

{
    echo "Forge vs Python vs Phoenix vs Rust Axum HTTP Benchmark"
    echo "Date: $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
    echo "Host: $(uname -srm)"
    echo "CPU cores: $CPU_COUNT"
    echo "Requests: $REQUESTS, Concurrency: $CONCURRENCY"
    echo "Body: Hello, World (13 bytes)"
    echo "Server affinity: all cores (taskset 0-$((CPU_COUNT - 1)))"
    echo

    if [[ ! -x "$FORGE_BIN" ]]; then
        echo "Forge binary not found: $FORGE_BIN" >&2
        exit 1
    fi

    if [[ ! -x "$PHX_SERVER" ]]; then
        chmod +x "$PHX_SERVER"
    fi

    if [[ ! -x "$AXUM_SERVER" ]]; then
        chmod +x "$AXUM_SERVER"
    fi

    bench_one "Forge (AOT native)" "$FORGE_PORT" "$FORGE_BIN"
    bench_one "Python 3 (socket)" "$PY_PORT" python3 "$PY_SERVER"
    bench_one "Phoenix (Bandit)" "$PHX_PORT" env PORT="$PHX_PORT" "$PHX_SERVER"
    bench_one "Rust (Axum)" "$AXUM_PORT" env PORT="$AXUM_PORT" CARGO_TARGET_DIR= "$AXUM_SERVER"
} | tee "$RESULTS"

echo "Results saved to $RESULTS"
