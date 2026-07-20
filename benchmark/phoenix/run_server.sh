#!/usr/bin/env bash
# Minimal Phoenix benchmark server — same response as Forge/Python benches.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
APP="${ROOT}/bench_server"
PORT="${PORT:-19082}"

cd "$APP"

if ! command -v mix >/dev/null 2>&1; then
  echo "mix not found; install Erlang/Elixir (asdf: erlang + elixir)" >&2
  exit 1
fi

export MIX_ENV=prod
export PORT

mix local.hex --force >/dev/null 2>&1 || true
mix deps.get --only prod
mix compile

echo "Phoenix benchmark server on port ${PORT}"
exec mix run --no-halt
