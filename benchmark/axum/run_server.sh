#!/usr/bin/env bash
# Minimal Axum HTTP benchmark server — same response as Forge/Python/Phoenix benches.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
APP="${ROOT}/bench_server"
PORT="${PORT:-19083}"

cd "$APP"

if ! command -v cargo >/dev/null 2>&1; then
  echo "cargo not found; install Rust (https://rustup.rs)" >&2
  exit 1
fi

# Avoid sandbox CARGO_TARGET_DIR pointing outside the crate tree.
unset CARGO_TARGET_DIR
cargo build --release --quiet
BIN="${APP}/target/release/bench_server"
if [[ ! -x "$BIN" ]]; then
  echo "bench_server binary not found at ${BIN}" >&2
  exit 1
fi
echo "Rust Axum benchmark server on port ${PORT}"
exec env PORT="${PORT}" "$BIN"
