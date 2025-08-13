#!/usr/bin/env bash
set -euo pipefail

# 1) Forcer Pulse utilisateur + basculer sur l’ampli BT (remplace par ton MAC si besoin)
export PULSE_SERVER=/run/user/$(id -u)/pulse/native
MAC="50:5B:C2:2F:39:99"
UNDERS=$(echo "$MAC" | tr ':' '_')
pactl set-card-profile "bluez_card.$UNDERS" a2dp_sink 2>/dev/null || true
SINK=$(pactl list short sinks | awk -v u="$UNDERS" '$2 ~ u {print $2; exit}')
[ -n "$SINK" ] && pactl set-default-sink "$SINK"

# 2) Lancer le push-to-talk Vosk -> assistant -> TTS (espeak) via Pulse
#    Ajuste si besoin : REC_SEC (durée), MIC_DEV (default|pulse|hw:0,0|plughw:0,0)
REC_SEC="${REC_SEC:-3}" MIC_DEV="${MIC_DEV:-default}" \
  "$(cd "$(dirname "$0")"; pwd)/ptt_vosk.sh"
