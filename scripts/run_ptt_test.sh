#!/usr/bin/env bash
set -euo pipefail

# Liste les devices (peut échouer si pas d'audio — on ignore)
./build/home_assistant --with-audio --list-devices || true

echo
echo "PTT + VAD :"
echo " - Appuie Entrée pour démarrer l'enregistrement"
echo " - Parle, puis laisse un silence (auto-stop VAD)"
echo " - Un WAV sera écrit dans captures/"
./build/home_assistant --with-audio --ptt \
  --record-seconds 10 \
  --vad --vad-threshold-db -35 --vad-min-voice-ms 120 --vad-silence-close-ms 600 \
  --save-wav captures/
