#!/usr/bin/env bash
set +u
VA="$HOME/voice-assistant"
# Casque A2DP par défaut
MAC="88:C9:E8:3A:E7:85"; CARD="bluez_card.${MAC//:/_}"
echo -e "connect $MAC\nquit" | bluetoothctl >/dev/null 2>&1 || true
pactl set-card-profile "$CARD" a2dp_sink 2>/dev/null || true
SINK=$(LANG=C pactl list short sinks | awk -v m="${MAC//:/_}" '$2 ~ m && /a2dp-sink/ {print $2; exit}')
[ -n "$SINK" ] && pactl set-default-sink "$SINK"

# Piper (libs)
export PIPER_DIR="$VA/piper/bin"
export LD_LIBRARY_PATH="$PIPER_DIR:$PIPER_DIR/lib:${LD_LIBRARY_PATH:-}"
[ -d "$PIPER_DIR/espeak-ng-data" ] && export ESPEAK_DATA_PATH="$PIPER_DIR/espeak-ng-data"
export PIPER_MODEL="$VA/piper/models/fr_FR-upmc-medium.onnx"

# venv + serveur micro
source "$HOME/voiceenv/bin/activate"
python "$VA/bin/remote_mic_server.py" & SVR=$!

# URL pour le téléphone
IP=$(ip -4 route get 1.1.1.1 | awk '{for(i=1;i<=NF;i++) if($i=="src"){print $(i+1); exit}}')
echo "Ouvre sur le téléphone: http://$IP:5000"

# assistant (Ctrl+C/Échap dans le terminal pour quitter)
python "$VA/bin/assistant_phone.py"

kill "$SVR" 2>/dev/null || true

# --- Reconnaissance: whisper.cpp ---

# --- Reconnaissance: whisper.cpp (wrapper greedy + base) ---

# --- Reconnaissance: whisper.cpp (wrapper greedy + base) ---
export ASR_ENGINE=whispercpp
export WHISPER_BIN="$HOME/whisper.cpp/wb.sh"
export WHISPER_MODEL="$HOME/whisper.cpp/models/ggml-base.bin"
