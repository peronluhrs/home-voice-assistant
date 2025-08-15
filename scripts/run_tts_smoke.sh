#!/usr/bin/env bash
set -euo pipefail
: "${PIPER_BIN:=piper}"
: "${PIPER_MODEL:=/path/to/model.onnx}"
mkdir -p captures
./build/home_assistant --with-piper --piper-bin "$PIPER_BIN" --piper-model "$PIPER_MODEL" --say "Bonjour, ceci est un test." || {
  echo "[fail] --say failed"
  exit 1
}
echo "[ok] TTS smoke test completed."
