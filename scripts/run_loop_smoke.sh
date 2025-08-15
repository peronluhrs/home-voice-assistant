#!/usr/bin/env bash
set -euo pipefail
mkdir -p captures logs
echo "[info] starting 1-turn loop without audio (offline)"
./build/home_assistant --offline --loop --loop-max-turns 1 --log-jsonl logs/loop.jsonl

echo "[info] if audio available, run:"
echo "./build/home_assistant --with-audio --ptt --loop --loop-max-turns 1 --loop-ptt-seconds 8 --loop-save-wavs captures/"
