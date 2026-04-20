#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT"

PID_FILE="$ROOT/.app-pids"
stopped_any=0

stop_if_running() {
  local pid="$1"
  if [[ -n "$pid" ]] && kill -0 "$pid" >/dev/null 2>&1; then
    kill "$pid" >/dev/null 2>&1 || true
    echo "Stopped process PID $pid."
    stopped_any=1
  fi
}

if [[ -f "$PID_FILE" ]]; then
  # shellcheck source=/dev/null
  source "$PID_FILE" || true
  stop_if_running "${backend_pid:-}"
  stop_if_running "${frontend_pid:-}"
  rm -f "$PID_FILE"
fi

for port in 8080 5500; do
  if command -v lsof >/dev/null 2>&1; then
    while IFS= read -r pid; do
      [[ -n "$pid" ]] || continue
      stop_if_running "$pid"
    done < <(lsof -tiTCP:"$port" -sTCP:LISTEN 2>/dev/null || true)
  elif command -v ss >/dev/null 2>&1; then
    while IFS= read -r pid; do
      [[ -n "$pid" ]] || continue
      stop_if_running "$pid"
    done < <(ss -ltnp "sport = :$port" 2>/dev/null | sed -n 's/.*pid=\([0-9]\+\).*/\1/p' | sort -u)
  fi
done

if [[ "$stopped_any" -eq 0 ]]; then
  echo "No running app processes were found."
fi
