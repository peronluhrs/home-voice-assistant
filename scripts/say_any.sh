#!/usr/bin/env bash
set -euo pipefail
TEXT="${*:-Bonjour, je parle.}"
OUT="/tmp/tts.wav"

if command -v piper >/dev/null 2>&1 && [ -n "${PIPER_MODEL:-}" ] && [ -f "$PIPER_MODEL" ]; then
  # Piper présent + modèle fourni
  piper -m "$PIPER_MODEL" -f "$OUT" <<< "$TEXT" >/dev/null 2>&1 || true
  if [ -s "$OUT" ]; then
    aplay -D default "$OUT" >/dev/null 2>&1 || true
    exit 0
  fi
fi

# Fallback universel : espeak-ng (sort directement sur la carte par défaut)
espeak-ng -v fr+f3 -s 165 "$TEXT" >/dev/null 2>&1 || true
