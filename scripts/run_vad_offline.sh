#!/usr/bin/env bash
set -euo pipefail

# Génère un WAV de fixture puis lance le VAD offline (pas besoin d'audio)
./build/home_assistant --gen-vad-fixture-wav captures/vad_fixture.wav
./build/home_assistant --vad-from-wav captures/vad_fixture.wav --vad-dump-json captures/vad_segments.json

echo
echo "[ok] fichiers produits :"
ls -l captures/ || true
