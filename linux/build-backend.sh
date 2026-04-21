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
  "$SRC/handlers_common.c"
  "$SRC/handlers_chores.c"
  "$SRC/handlers_messages.c"
  "$SRC/handlers_roommates.c"
)

if command -v gcc >/dev/null 2>&1; then
  gcc -std=c2x -Wall -Wextra -I "$BACKEND_ROOT/include" "${FILES[@]}" -o "$OUT_FILE"
  echo "Built with GCC: $OUT_FILE"
  exit 0
fi

if command -v clang >/dev/null 2>&1; then
  clang -std=c2x -Wall -Wextra -I "$BACKEND_ROOT/include" "${FILES[@]}" -o "$OUT_FILE"
  echo "Built with Clang: $OUT_FILE"
  exit 0
fi

echo "No supported C compiler found. Install gcc or clang." >&2
exit 1
