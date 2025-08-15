#!/usr/bin/env bash
set -Eeuo pipefail
VA="$HOME/voice-assistant"
CF="$HOME/cloudflared"
LOG_CF="/tmp/cf.log"
LOG_AS="/tmp/assistant.log"
RECIPIENT="peronluhrs@gmail.com"

# charge SMTP_* depuis .env si présent
[ -f "$VA/.env" ] && set -a && . "$VA/.env" && set +a

# stop propres
fuser -k 5000/tcp 2>/dev/null || true
pkill -f cloudflared 2>/dev/null || true

# force HTTP (TLS via Cloudflare)
mv "$VA/bin/cert.pem" "$VA/bin/cert.pem.off" 2>/dev/null || true
mv "$VA/bin/key.pem"  "$VA/bin/key.pem.off"  2>/dev/null || true

:> "$LOG_AS"; :> "$LOG_CF"

# 1) Lancer l’assistant (unbufferized) + notre wrapper Whisper
(
  export PYTHONUNBUFFERED=1
  export WHISPER_BIN="$HOME/whisper.cpp/wb.sh"
  source "$HOME/voiceenv/bin/activate" 2>/dev/null || true
  bash -lc "$VA/bin/run_assistant.sh"
) >>"$LOG_AS" 2>&1 &

# 2) Démarrer le tunnel tout de suite (pas de blocage)
if [ ! -x "$CF" ]; then
  echo "Téléchargement de cloudflared…"
  curl -sSL -o "$CF" https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64
  chmod +x "$CF"
fi
"$CF" tunnel --url http://127.0.0.1:5000 --protocol http2 --no-autoupdate 2>&1 | tee -a "$LOG_CF" >/dev/null &
CF_PID=$!

# 3) Récupérer l’URL
URL=""
for i in {1..30}; do
  URL=$(grep -o 'https://[[:alnum:].-]*trycloudflare.com' "$LOG_CF" | tail -n1 || true)
  [ -n "$URL" ] && break
  sleep 1
done
[ -z "$URL" ] && { echo "⚠️  Pas d’URL Cloudflare. Voir $LOG_CF"; exit 1; }

echo
echo "🌐 Ouvre sur le téléphone : $URL"
echo "📒 Logs : tail -f $LOG_AS   |   tail -f $LOG_CF"

# 4) Envoyer l’e-mail (même si :5000 pas encore prêt)
if [ -n "${SMTP_USER:-}" ] && [ -n "${SMTP_PASS:-}" ]; then
python3 - <<PY || true
import os, ssl, smtplib
from email.message import EmailMessage
u=os.environ["SMTP_USER"]; p=os.environ["SMTP_PASS"]
host=os.environ.get("SMTP_HOST","smtp.gmail.com")
port=int(os.environ.get("SMTP_PORT","587"))
use_ssl=os.environ.get("SMTP_SSL","0")=="1"
to="${RECIPIENT}"; url="${URL}"
msg=EmailMessage(); msg["Subject"]="Assistant vocal — Lien Cloudflare"; msg["From"]=u; msg["To"]=to
msg.set_content(f"Assistant (tunnel) prêt :\\n{url}\\n\\nSi la page 502, attends 5–10s et recharge.")
ctx=ssl.create_default_context()
if use_ssl:
    with smtplib.SMTP_SSL(host, port, context=ctx, timeout=30) as s: s.login(u,p); s.send_message(msg)
else:
    with smtplib.SMTP(host, port, timeout=30) as s: s.starttls(context=ctx); s.login(u,p); s.send_message(msg)
print("✉️  E-mail envoyé à", to)
PY
else
  echo "✉️  SMTP non configuré (.env)."
fi

# 5) Petit diagnostic si :5000 pas encore up
READY=0
for i in {1..10}; do
  if curl -sS -o /dev/null -w "%{http_code}" http://127.0.0.1:5000/ | grep -q '^200$'; then READY=1; break; fi
  sleep 1
done
if [ $READY -eq 0 ]; then
  echo "⏳ L’assistant n’est pas encore prêt (port 5000). Extraits logs:"
  tail -n 40 "$LOG_AS" || true
fi

# 6) Ouvrir des fenêtres de logs (optionnel)
if command -v gnome-terminal >/dev/null; then
  gnome-terminal -- bash -lc "echo Assistant; tail -f $LOG_AS"
  gnome-terminal -- bash -lc "echo Tunnel; tail -f $LOG_CF"
elif command -v xterm >/dev/null; then
  xterm -e "tail -f $LOG_AS" &
  xterm -e "tail -f $LOG_CF" &
fi

wait $CF_PID
