#!/usr/bin/env bash
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.."; pwd)"
BIN="$REPO_DIR/build/home_assistant"
FIFO="/tmp/assistant_in.fifo"
REC_SEC="${REC_SEC:-3}"
MIC_DEV="${MIC_DEV:-default}"   # `arecord -L` pour voir les noms. Ex: default, pulse, hw:0,0, plughw:0,0

rm -f "$FIFO"; mkfifo "$FIFO"

# lance l'assistant qui vocalise les réponses via espeak-ng/pulse
"$REPO_DIR/scripts/assistant_talker.sh" --offline --output-device pulse < "$FIFO" &
ASS_PID=$!

cleanup(){ kill $ASS_PID 2>/dev/null || true; rm -f "$FIFO"; }
trap cleanup EXIT

echo "[ptt] Entrée = parle ($REC_SEC s) ; /q = quitter"
while true; do
  read -r -p "" line || true
  [[ "$line" == "/q" ]] && break
  arecord -d "$REC_SEC" -f S16_LE -r 16000 -c 1 -D "$MIC_DEV" /tmp/utt.wav 2>/dev/null || true

  TEXT=$(python3 - <<'PY'
import os, json, wave, sys
try:
    from vosk import Model, KaldiRecognizer
except Exception:
    print("__VOSK_MISSING__", flush=True); sys.exit(0)
m = os.path.expanduser('~/models/vosk/vosk-model-small-fr-0.22')
if not os.path.isdir(m):
    print("__MODEL_MISSING__", flush=True); sys.exit(0)
wf = wave.open('/tmp/utt.wav','rb')
rec = KaldiRecognizer(Model(m), wf.getframerate())
while True:
    d = wf.readframes(4000)
    if not d: break
    rec.AcceptWaveform(d)
print(json.loads(rec.FinalResult()).get("text","").strip(), flush=True)
PY
)
  if [[ "$TEXT" == "__VOSK_MISSING__" ]]; then
    echo "[ptt] Vosk manquant. Installe:  python3 -m pip install --user vosk"; continue
  elif [[ "$TEXT" == "__MODEL_MISSING__" ]]; then
    echo "[ptt] Modèle manquant. Télécharge dans ~/models/vosk/vosk-model-small-fr-0.22"; continue
  fi

  if [[ -z "$TEXT" ]]; then echo "[ptt] Rien reconnu."; continue; fi
  echo "$TEXT" > "$FIFO"
done
