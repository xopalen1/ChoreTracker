#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT"

PID_FILE="$ROOT/.app-pids"
stopped_any=0
NGINX_BIN="/usr/sbin/nginx"
NGINX_CONF_FILE="$ROOT/.nginx-roommate.conf"
NGINX_PID_FILE="$ROOT/.nginx-roommate.pid"

stop_if_running() {
  local pid="$1"
  if [[ -n "$pid" ]] && kill -0 "$pid" >/dev/null 2>&1; then
    if kill "$pid" >/dev/null 2>&1; then
      echo "Stopped process PID $pid."
      stopped_any=1
      return
    fi

    if sudo kill "$pid" >/dev/null 2>&1; then
      echo "Stopped process PID $pid (sudo)."
      stopped_any=1
    fi
  fi
}

if [[ -x "$NGINX_BIN" ]] && [[ -f "$NGINX_CONF_FILE" ]]; then
  if sudo "$NGINX_BIN" -c "$NGINX_CONF_FILE" -s stop >/dev/null 2>&1; then
    echo "Stopped nginx via $NGINX_BIN -s stop."
    stopped_any=1
  fi
fi

if [[ -f "$PID_FILE" ]]; then
  # shellcheck source=/dev/null
  source "$PID_FILE" || true
  stop_if_running "${backend_pid:-}"
  stop_if_running "${frontend_pid:-}"
  rm -f "$PID_FILE"
fi

rm -f "$NGINX_PID_FILE"

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
