#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT"

PID_FILE="$ROOT/.app-pids"
BACKEND_BIN="$ROOT/backend/bin/roommate_backend"
BACKEND_BUILD_SCRIPT="$SCRIPT_DIR/build-backend.sh"
NGINX_CONF_FILE="$ROOT/.nginx-roommate.conf"
NGINX_PID_FILE="$ROOT/.nginx-roommate.pid"
LOG_DIR="$ROOT/.logs"
NGINX_RUN_USER="${SUDO_USER:-${USER:-}}"

if [[ -z "$NGINX_RUN_USER" ]]; then
  NGINX_RUN_USER="www-data"
fi

backend_needs_build() {
  if [[ ! -x "$BACKEND_BIN" ]]; then
    return 0
  fi

  if find "$ROOT/backend/src" "$ROOT/backend/include" "$BACKEND_BUILD_SCRIPT" -type f -newer "$BACKEND_BIN" | grep -q .; then
    return 0
  fi

  return 1
}

if backend_needs_build; then
  echo "Backend binary missing or stale. Building backend..."
  bash "$BACKEND_BUILD_SCRIPT"
fi

NGINX_BIN="/usr/sbin/nginx"
if [[ ! -x "$NGINX_BIN" ]]; then
  echo "Nginx not found at $NGINX_BIN." >&2
  exit 1
fi

mkdir -p "$LOG_DIR"

cat >"$NGINX_CONF_FILE" <<EOF
user $NGINX_RUN_USER;
worker_processes 1;
pid "$NGINX_PID_FILE";

events {
  worker_connections 1024;
}

http {
  default_type application/octet-stream;
  sendfile on;
  keepalive_timeout 65;

  server {
    listen 5500 default_server;
    server_name _;
    root "$ROOT";
    index index.html;

    location = /index.html {
      try_files /index.html =404;
    }

    location / {
      try_files \$uri \$uri/ /index.html;
    }

    location /api/ {
      proxy_pass http://127.0.0.1:8080;
      proxy_http_version 1.1;
      proxy_set_header Host \$host;
      proxy_set_header X-Real-IP \$remote_addr;
      proxy_set_header X-Forwarded-For \$proxy_add_x_forwarded_for;
      proxy_set_header X-Forwarded-Proto \$scheme;
    }

    access_log "$LOG_DIR/nginx-access.log";
    error_log "$LOG_DIR/nginx-error.log";
  }
}
EOF

nohup "$BACKEND_BIN" 8080 >/tmp/roommate-backend.log 2>&1 &
BACKEND_PID=$!

sudo "$NGINX_BIN" -t -c "$NGINX_CONF_FILE"
sudo "$NGINX_BIN" -c "$NGINX_CONF_FILE"

FRONTEND_PID=""
if [[ -f "$NGINX_PID_FILE" ]]; then
  FRONTEND_PID="$(cat "$NGINX_PID_FILE" 2>/dev/null || true)"
fi

cat >"$PID_FILE" <<EOF
backend_pid=$BACKEND_PID
frontend_pid=$FRONTEND_PID
frontend_pid_file=$NGINX_PID_FILE
EOF

is_valid_lan_ipv4() {
  local ip="$1"
  [[ -n "$ip" ]] || return 1
  [[ "$ip" =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}$ ]] || return 1
  [[ "$ip" != 127.* ]] || return 1
  [[ "$ip" != 169.254.* ]] || return 1
  return 0
}

is_private_lan_ipv4() {
  local ip="$1"
  is_valid_lan_ipv4 "$ip" || return 1
  [[ "$ip" == 10.* ]] && return 0
  [[ "$ip" == 192.168.* ]] && return 0
  [[ "$ip" =~ ^172\.(1[6-9]|2[0-9]|3[0-1])\. ]] && return 0
  return 1
}

primary_lan_ip=""
if command -v ip >/dev/null 2>&1; then
  primary_lan_ip="$(ip -4 route get 1 2>/dev/null | awk '{for(i=1;i<=NF;i++) if($i=="src") {print $(i+1); exit}}')"
fi

mapfile -t all_lan_ips < <(hostname -I 2>/dev/null | tr ' ' '\n' | sed '/^$/d' | sort -u)

private_ips=()
for ip in "${all_lan_ips[@]:-}"; do
  if is_private_lan_ipv4 "$ip"; then
    private_ips+=("$ip")
  fi
done

if [[ ${#private_ips[@]} -gt 0 ]]; then
  all_lan_ips=("${private_ips[@]}")
fi

if ! is_valid_lan_ipv4 "$primary_lan_ip" && [[ ${#all_lan_ips[@]} -gt 0 ]]; then
  primary_lan_ip="${all_lan_ips[0]}"
fi

echo "Started backend on port 8080 and frontend (Nginx) on port 5500."
if is_valid_lan_ipv4 "$primary_lan_ip"; then
  echo "Open on same Wi-Fi: http://$primary_lan_ip:5500"
else
  echo "Open on same Wi-Fi using this machine's IP address on port 5500."
fi

if [[ ${#all_lan_ips[@]} -gt 1 ]]; then
  echo "Other LAN addresses on this PC:"
  for ip in "${all_lan_ips[@]}"; do
    if is_valid_lan_ipv4 "$ip" && [[ "$ip" != "$primary_lan_ip" ]]; then
      echo "  http://$ip:5500"
    fi
  done
fi
