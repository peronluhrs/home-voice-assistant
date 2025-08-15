#!/usr/bin/env bash
set -euo pipefail
: "${VOSK_MODEL_DIR:?Set VOSK_MODEL_DIR to your model path}"
./build/home_assistant --stt-from-wav captures/vad_fixture.wav \
  --vosk-model "$VOSK_MODEL_DIR" --stt-dump-json captures/asr_fixture.json \
  --with-vosk
