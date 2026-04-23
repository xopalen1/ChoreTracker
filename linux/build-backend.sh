#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BACKEND_ROOT="$ROOT/backend"
SRC="$BACKEND_ROOT/src"
OUT_DIR="$BACKEND_ROOT/bin"
OUT_FILE="$OUT_DIR/roommate_backend"

mkdir -p "$OUT_DIR"

FILES=(
  "$SRC/main.c"
  "$SRC/net.c"
  "$SRC/http.c"
  "$SRC/server.c"
  "$SRC/router.c"
  "$SRC/db.c"
  "$SRC/json_utils.c"
  "$SRC/text_utils.c"
  "$SRC/string_builder.c"
  "$SRC/date_utils.c"
  "$SRC/reassignment.c"
  "$SRC/handlers_common.c"
  "$SRC/handlers_chores.c"
  "$SRC/handlers_messages.c"
  "$SRC/handlers_roommates.c"
)

# Debian/glibc visibility fixes:
# - strtok_r / strnlen need POSIX/XSI feature macros in some toolchains.
# - strcasecmp is declared in <strings.h>; force include to avoid source edits.
COMMON_CFLAGS=(
  -std=c2x
  -Wall
  -Wextra
  -D_DEFAULT_SOURCE
  -D_POSIX_C_SOURCE=200809L
  -include
  strings.h
  -I
  "$BACKEND_ROOT/include"
)

if command -v gcc >/dev/null 2>&1; then
  gcc "${COMMON_CFLAGS[@]}" "${FILES[@]}" -o "$OUT_FILE"
  echo "Built with GCC: $OUT_FILE"
  exit 0
fi

if command -v clang >/dev/null 2>&1; then
  clang "${COMMON_CFLAGS[@]}" "${FILES[@]}" -o "$OUT_FILE"
  echo "Built with Clang: $OUT_FILE"
  exit 0
fi

echo "No supported C compiler found. Install gcc or clang." >&2
exit 1
