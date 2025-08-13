#!/bin/bash
set -euo pipefail

# Example usage:
# export VOSK_MODEL_PATH=/path/to/vosk-model
# export PIPER_MODEL_PATH=/path/to/piper-model.onnx
# ./scripts/run_audio_vosk_piper.sh

if [ -z "${VOSK_MODEL_PATH:-}" ]; then
    echo "Please set the VOSK_MODEL_PATH environment variable."
    exit 1
fi

if [ -z "${PIPER_MODEL_PATH:-}" ]; then
    echo "Please set the PIPER_MODEL_PATH environment variable."
    exit 1
fi

mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DWITH_AUDIO=ON -DWITH_VOSK=ON -DWITH_PIPER=ON
cmake --build .

./home_assistant \
    --with-audio \
    --asr vosk \
    --vosk-model "$VOSK_MODEL_PATH" \
    --with-piper \
    --piper-model "$PIPER_MODEL_PATH" \
    --record-seconds 5
