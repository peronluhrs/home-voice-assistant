#!/usr/bin/env bash
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.."; pwd)"
BIN="$REPO_DIR/build/home_assistant"

# Parler via espeak-ng -> Pulse
say_pulse() {
  local txt="$1"
  if command -v espeak-ng >/dev/null 2>&1; then
    printf '%s' "$txt" | espeak-ng -v fr -s 165 --stdout 2>/dev/null | aplay -D pulse -q 2>/dev/null &
  fi
}

# Lance l'assistant et vocalise les lignes "assistant> ..."
"$BIN" "$@" | while IFS= read -r line; do
  printf '%s\n' "$line"
  case "$line" in
    "assistant> "*) say_pulse "${line#assistant> }" ;;
    *"assistant> "*) l="${line#*assistant> }"; say_pulse "$l" ;;
  esac
done
