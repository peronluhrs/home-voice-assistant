#!/usr/bin/env bash
RATE=16000
echo "Entrée = PARLER ; Entrée = ARRÊTER ; Ctrl+C = quitter."
while true; do
  read -r
  echo "[REC] ... parle ..."
  arecord -D pulse -r $RATE -f S16_LE -q -t wav /tmp/ptt.wav &
  REC_PID=$!
  read -r
  echo "[STOP]"
  kill -INT "$REC_PID" 2>/dev/null || true
  wait "$REC_PID" 2>/dev/null || true
  echo "[PLAYBACK]"
  aplay -D pulse /tmp/ptt.wav
  espeak-ng -v fr "Reçu, je t'entends." --stdout | paplay
done
